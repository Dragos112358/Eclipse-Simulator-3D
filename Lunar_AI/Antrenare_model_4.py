"""
Lunar Altitude Neural Network v6 — CONTACT BANDS ONLY.
In loc sa antrenam pe toata suprafata Lunii, restrangem domeniul la doua
benzi verticale de longitudine (in jurul +90 si -90 grade selenografice),
acolo unde cad efectiv toate contactele C1-C4 posibile (vezi
measure_contact_longitude_spread). Latitudinea ramane completa (-90..90),
pentru ca formula contactelor acopera aproape tot intervalul de latitudine
in functie de unghiul de pozitie.

Beneficiu: acelasi buget de memorie / hashmap acopera acum doar ~(4*W)/360
din suprafata totala (W = BAND_HALF_WIDTH_DEG), deci poti creste rezolutia
efectiva de mai multe ori FARA sa depasesti bugetul de 128MB, sau poti tine
aceeasi rezolutie cu un hashmap mult mai mic si mai putine coliziuni.
"""

import copy
import sys

import numpy as np
import rasterio
import torch
import torch.nn as nn
from tqdm import tqdm

DEM_PATH = "lunar_dem.tif"
CHECKPOINT_PATH = "lunar_altitude_contact_bands_2000_epoci.pt"
CHECKPOINT_PATH_FP16 = "lunar_altitude_contact_bands_fp16_2000_epoci.pt"

# ---------------------------------------------------------------------------
# Domeniul restrans (rezultat din measure_contact_longitude_spread + marja)
# ---------------------------------------------------------------------------
LIMB_CENTERS_DEG = [-90.0, 90.0]
BAND_HALF_WIDTH_DEG = 10.0   # <-- INLOCUIESTE cu (deviatia_maxima_masurata + 5-8 marja)

# ---------------------------------------------------------------------------
# Hiperparametri encoding — recalculati pentru domeniul restrans
# ---------------------------------------------------------------------------
N_LEVELS = 16
N_FEATURES = 2
LOG2_HASHMAP_SIZE = 21          # scazut de la 22: domeniul e mult mai mic, hashmap-ul
                                 # mare de dinainte era gandit pentru toata suprafata
BASE_RESOLUTION_LAT = 16
FINEST_RESOLUTION_LAT = 46080    # neschimbat: tot pe -90..90 latitudine, tot rezolutia nativa
LON_LAT_ASPECT = None            # calculat automat mai jos, in functie de banda

DEPLOY_MEMORY_BUDGET_BYTES = 256 * 1024 * 1024
TRAIN_FP32_BUDGET_BYTES = 2 * DEPLOY_MEMORY_BUDGET_BYTES

N_EPOCHS = 1000
SAMPLES_PER_EPOCH = 2_000_000
BATCH_SIZE = 16384
LR = 5e-3
WEIGHT_DECAY_MLP = 1e-5
WEIGHT_DECAY_EMB = 1e-7
GRAD_CLIP_NORM = 1.0
PATIENCE_EPOCHS = 50

FOCUS_FRAC_MAX = 0.6
FOCUS_WARMUP_EPOCHS = 60
COARSE_TO_FINE_WARMUP_FRAC = 0.4


# ---------------------------------------------------------------------------
# 1. Sampler restrans la cele doua benzi de longitudine
# ---------------------------------------------------------------------------
class LunarDEMSamplerContactBands:
    """
    Ca LunarDEMSampler2D original, dar esantioneaza DOAR din cele doua benzi
    verticale de longitudine relevante pentru contactele C1-C4. Fiecare banda
    e mapata intr-o "jumatate" a domeniului u in [0,1): tile 0 (banda -90) ->
    u in [0, 0.5), tile 1 (banda +90) -> u in [0.5, 1). Nu exista wrap circular
    intre cele doua tile-uri (nu sunt vecine fizic pe Luna).
    """
    def __init__(self, dem_path, band_half_width_deg=BAND_HALF_WIDTH_DEG,
                 importance_bins=(2160, 1080)):
        self.w = band_half_width_deg
        self.centers = LIMB_CENTERS_DEG

        with rasterio.open(dem_path) as src:
            self.elevation = src.read(1).astype(np.float32)
            self.transform = src.transform
            self.height, self.width = self.elevation.shape

        corners_px = [(0, 0), (self.width, 0), (0, self.height), (self.width, self.height)]
        corner_coords = [self.transform * c for c in corners_px]
        lons = [c[0] for c in corner_coords]
        lats = [c[1] for c in corner_coords]
        self.dem_lon_min, self.dem_lon_max = min(lons), max(lons)
        self.lat_min, self.lat_max = min(lats), max(lats)

        deg_per_pixel = 360.0 / self.width
        total_band_deg = 4.0 * self.w   # 2 benzi x 2*w fiecare
        frac_of_globe = total_band_deg / 360.0
        recommended_lon_cells_in_u = int(round(total_band_deg / deg_per_pixel))
        recommended_lat_cells = int(round(180.0 / deg_per_pixel))
        print(f"[info] DEM nativ: {self.width}x{self.height} px (~{deg_per_pixel:.5f}°/pixel).")
        print(f"[info] Benzi: centre {self.centers}°, ±{self.w}° fiecare -> "
              f"{frac_of_globe*100:.1f}% din suprafata totala.")
        print(f"[info] FINEST_RESOLUTION_LAT recomandat ≈ {recommended_lat_cells} "
              f"(lon in u-space ≈ {recommended_lon_cells_in_u}) pt rezolutie nativa in benzi.")

        probe_lon, probe_lat = self._uniform_area_lonlat_in_bands(200_000)
        probe_alt = self._sample_bilinear(probe_lon, probe_lat)
        probe_alt = probe_alt[np.isfinite(probe_alt)]
        self.alt_mean = float(probe_alt.mean())
        self.alt_std = float(probe_alt.std() + 1e-6)

        self._build_importance_map(importance_bins)

    def _band_pixel_range(self, center_deg):
        inv_transform = ~self.transform
        lon_lo, lon_hi = center_deg - self.w, center_deg + self.w
        col_lo, _ = inv_transform * (lon_lo, 0.0)
        col_hi, _ = inv_transform * (lon_hi, 0.0)
        c0, c1 = sorted([int(np.floor(col_lo)), int(np.ceil(col_hi))])
        return max(0, c0), min(self.width, c1)

    def _build_importance_map(self, bins):
        w_bins_total, h_bins = bins
        w_bins_per_tile = max(4, w_bins_total // 2)
        step_y = max(1, self.height // h_bins)

        self._tile_importance = []
        for center in self.centers:
            c0, c1 = self._band_pixel_range(center)
            step_x = max(1, (c1 - c0) // w_bins_per_tile)
            small = self.elevation[::step_y, c0:c1:step_x].astype(np.float32)

            gy, gx = np.gradient(small)
            gyy, _ = np.gradient(gy)
            _, gxx = np.gradient(gx)
            curvature = np.abs(gxx) + np.abs(gyy)

            w = curvature - curvature.min()
            w = w / (w.max() + 1e-8)
            w = np.sqrt(w) + 0.05
            w = w / w.sum()

            self._tile_importance.append({
                'cdf': np.cumsum(w.ravel()),
                'shape': small.shape,
                'c0': c0, 'step_x': step_x, 'step_y': step_y,
            })

    def _uniform_area_lonlat_in_bands(self, n):
        tile_idx = np.random.randint(0, 2, size=n)
        u_local = np.random.uniform(-self.w, self.w, size=n)
        centers = np.array(self.centers)[tile_idx]
        lon = centers + u_local

        u = np.random.uniform(0.0, 1.0, size=n)
        sin_min = np.sin(np.radians(self.lat_min))
        sin_max = np.sin(np.radians(self.lat_max))
        lat = np.degrees(np.arcsin(sin_min + u * (sin_max - sin_min)))
        return lon.astype(np.float32), lat.astype(np.float32)

    def _biased_lonlat_in_bands(self, n):
        tile_idx = np.random.randint(0, 2, size=n)
        lon = np.empty(n, dtype=np.float32)
        lat = np.empty(n, dtype=np.float32)

        for t in (0, 1):
            mask = tile_idx == t
            cnt = int(mask.sum())
            if cnt == 0:
                continue
            info = self._tile_importance[t]
            r = np.random.uniform(0.0, 1.0, size=cnt)
            flat_idx = np.clip(np.searchsorted(info['cdf'], r), 0,
                                info['shape'][0] * info['shape'][1] - 1)
            iy, ix = np.unravel_index(flat_idx, info['shape'])

            px = info['c0'] + (ix + np.random.uniform(0.0, 1.0, size=cnt)) * info['step_x']
            py = (iy + np.random.uniform(0.0, 1.0, size=cnt)) * info['step_y']
            lon_t, lat_t = self.transform * (px, py)
            lon[mask] = lon_t
            lat[mask] = lat_t
        return lon, lat

    def _sample_bilinear(self, lon, lat):
        inv_transform = ~self.transform
        cols, rows = inv_transform * (np.asarray(lon, dtype=np.float64),
                                       np.asarray(lat, dtype=np.float64))
        c0 = np.clip(np.floor(cols).astype(np.int64), 0, self.width - 2)
        r0 = np.clip(np.floor(rows).astype(np.int64), 0, self.height - 2)
        c1, r1 = c0 + 1, r0 + 1
        fc = np.clip(cols - c0, 0.0, 1.0)
        fr = np.clip(rows - r0, 0.0, 1.0)

        e00 = self.elevation[r0, c0]; e01 = self.elevation[r0, c1]
        e10 = self.elevation[r1, c0]; e11 = self.elevation[r1, c1]
        top = e00 * (1 - fc) + e01 * fc
        bot = e10 * (1 - fc) + e11 * fc
        return top * (1 - fr) + bot * fr

    def lonlat_to_uv(self, lon, lat):
        """Mapeaza lon (langa +-90) + lat -> (u,v) in domeniul de antrenare cu tile-uri."""
        lon = np.asarray(lon, dtype=np.float64)
        dists = [np.abs(((lon - c + 180) % 360) - 180) for c in self.centers]
        tile_idx = np.argmin(np.stack(dists, axis=0), axis=0)
        center = np.array(self.centers)[tile_idx]
        delta = ((lon - center + 180) % 360) - 180
        u_local = np.clip((delta + self.w) / (2 * self.w), 0.0, 1.0)
        u = (tile_idx + u_local) / 2.0

        v = np.clip((lat - self.lat_min) / (self.lat_max - self.lat_min), 0.0, 1.0)
        out_of_band = np.abs(delta) > self.w
        return u.astype(np.float32), v.astype(np.float32), out_of_band

    def sample_batch(self, n, device="cpu", focus_frac=0.0):
        n_focus = int(n * focus_frac)
        n_uniform = n - n_focus

        lon_u, lat_u = self._uniform_area_lonlat_in_bands(n_uniform)
        if n_focus > 0:
            lon_f, lat_f = self._biased_lonlat_in_bands(n_focus)
            lon = np.concatenate([lon_u, lon_f])
            lat = np.concatenate([lat_u, lat_f])
        else:
            lon, lat = lon_u, lat_u

        alt = self._sample_bilinear(lon, lat)
        valid = np.isfinite(alt)
        lon, lat, alt = lon[valid], lat[valid], alt[valid]

        u, v, _ = self.lonlat_to_uv(lon, lat)

        u_t = torch.tensor(u, dtype=torch.float32, device=device)
        v_t = torch.tensor(v, dtype=torch.float32, device=device)
        alt_norm = torch.tensor((alt - self.alt_mean) / self.alt_std,
                                 dtype=torch.float32, device=device)
        return u_t, v_t, alt_norm


# ---------------------------------------------------------------------------
# 2. Hash grid 2D FARA wrap circular (domeniul nu mai e periodic — sunt
#    doua tile-uri separate impachetate in u, nu un cerc continuu)
# ---------------------------------------------------------------------------
PRIME_LON = 1
PRIME_LAT = 2654435761


class MultiResHashEncoding2DNonPeriodic(nn.Module):
    def __init__(self, n_levels=N_LEVELS, n_features=N_FEATURES,
                 log2_hashmap_size=LOG2_HASHMAP_SIZE,
                 base_resolution_lat=BASE_RESOLUTION_LAT,
                 finest_resolution_lat=FINEST_RESOLUTION_LAT,
                 recommended_lon_cells=None):
        super().__init__()
        self.n_levels = n_levels
        self.n_features = n_features
        self.max_table_size = 2 ** log2_hashmap_size

        b = np.exp((np.log(finest_resolution_lat) - np.log(base_resolution_lat)) / (n_levels - 1))
        self.res_lat = [max(1, int(round(base_resolution_lat * (b ** l)))) for l in range(n_levels)]

        # aspect efectiv: raportul dintre "lungimea" totala reprezentata pe u
        # (ambele benzi) si cea de pe v (180 grade), astfel incat celulele
        # sa ramana ~patrate in grade reale, nu doar in spatiul u/v normalizat
        finest_lon_cells = recommended_lon_cells or finest_resolution_lat
        aspect = finest_lon_cells / finest_resolution_lat
        self.res_lon = [max(1, int(round(r * aspect))) for r in self.res_lat]

        self.table_sizes, self.use_hash = [], []
        for rlon, rlat in zip(self.res_lon, self.res_lat):
            n_cells = (rlon + 1) * (rlat + 1)   # FARA -1: nu mai avem wrap, ambele axe au capete distincte
            if n_cells <= self.max_table_size:
                self.table_sizes.append(n_cells); self.use_hash.append(False)
            else:
                self.table_sizes.append(self.max_table_size); self.use_hash.append(True)

        self.embeddings = nn.ModuleList([nn.Embedding(sz, n_features) for sz in self.table_sizes])
        for emb in self.embeddings:
            nn.init.uniform_(emb.weight, -1e-4, 1e-4)

    def total_params(self):
        return sum(self.table_sizes) * self.n_features

    def memory_bytes(self, dtype_bytes=4):
        return self.total_params() * dtype_bytes

    def _index(self, ix, iy, level):
        rlat = self.res_lat[level]
        if not self.use_hash[level]:
            return ix * (rlat + 1) + iy
        h = (ix.to(torch.int64) * PRIME_LON) ^ (iy.to(torch.int64) * PRIME_LAT)
        return h % self.table_sizes[level]

    def forward(self, u, v, active_frac=1.0):
        outputs = []
        for level in range(self.n_levels):
            rlon, rlat = self.res_lon[level], self.res_lat[level]

            x, y = u * rlon, v * rlat
            x0f, y0f = torch.floor(x), torch.floor(y)
            fx, fy = (x - x0f).unsqueeze(-1), (y - y0f).unsqueeze(-1)

            # CLAMP, nu wrap: domeniul nu e circular
            x0 = x0f.long().clamp(0, rlon)
            x1 = (x0f.long() + 1).clamp(0, rlon)
            y0 = y0f.long().clamp(0, rlat)
            y1 = (y0f.long() + 1).clamp(0, rlat)

            idx00, idx01 = self._index(x0, y0, level), self._index(x0, y1, level)
            idx10, idx11 = self._index(x1, y0, level), self._index(x1, y1, level)
            e00, e01 = self.embeddings[level](idx00), self.embeddings[level](idx01)
            e10, e11 = self.embeddings[level](idx10), self.embeddings[level](idx11)

            top = e00 * (1 - fx) + e10 * fx
            bot = e01 * (1 - fx) + e11 * fx
            feat = top * (1 - fy) + bot * fy

            level_weight = float(np.clip(active_frac * self.n_levels - level, 0.0, 1.0))
            outputs.append(feat * level_weight)
        return torch.cat(outputs, dim=-1)


class LunarAltitudeContactBandsNet(nn.Module):
    def __init__(self, recommended_lon_cells, hidden=256):
        super().__init__()
        self.encoding = MultiResHashEncoding2DNonPeriodic(recommended_lon_cells=recommended_lon_cells)
        in_dim = N_LEVELS * N_FEATURES
        self.mlp = nn.Sequential(
            nn.Linear(in_dim, hidden), nn.GELU(),
            nn.Linear(hidden, hidden), nn.GELU(),
            nn.Linear(hidden, hidden), nn.GELU(),  # Adăugat încă un strat intermediar
            nn.Linear(hidden, 1),
        )

    def forward(self, u, v, active_frac=1.0):
        return self.mlp(self.encoding(u, v, active_frac=active_frac)).squeeze(-1)


def print_memory_report(model):
    emb_fp32 = model.encoding.memory_bytes(4)
    emb_fp16 = model.encoding.memory_bytes(2)
    mlp_bytes = sum(p.numel() for p in model.mlp.parameters()) * 4
    total_fp16 = emb_fp16 + mlp_bytes
    print(f"[memorie] embeddings fp32: {emb_fp32/1024**2:.1f} MB | "
          f"deployment fp16: {total_fp16/1024**2:.1f} MB / buget {DEPLOY_MEMORY_BUDGET_BYTES/1024**2:.0f} MB")
    n_hashed = sum(model.encoding.use_hash)
    print(f"[memorie] nivele fara coliziuni: {N_LEVELS - n_hashed}/{N_LEVELS}")
    if total_fp16 < DEPLOY_MEMORY_BUDGET_BYTES:
        print(f"[memorie] marja libera: {(DEPLOY_MEMORY_BUDGET_BYTES - total_fp16)/1024**2:.1f} MB "
              f"-> poti creste LOG2_HASHMAP_SIZE sau FINEST_RESOLUTION_LAT.")


def train():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}")

    sampler = LunarDEMSamplerContactBands(DEM_PATH)
    deg_per_pixel = 360.0 / sampler.width
    recommended_lon_cells = int(round((4.0 * BAND_HALF_WIDTH_DEG) / deg_per_pixel))

    model = LunarAltitudeContactBandsNet(recommended_lon_cells).to(device)
    print_memory_report(model)

    optimizer = torch.optim.Adam([
        {"params": model.encoding.embeddings.parameters(), "weight_decay": WEIGHT_DECAY_EMB},
        {"params": model.mlp.parameters(), "weight_decay": WEIGHT_DECAY_MLP},
    ], lr=LR, betas=(0.9, 0.99), eps=1e-15)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=N_EPOCHS)
    #loss_fn = nn.MSELoss()
    loss_fn = nn.HuberLoss(delta=1.0)  # În loc de nn.MSELoss()
    scaler = torch.amp.GradScaler("cuda" if device == "cuda" else "cpu")

    warmup_epochs = max(1, int(COARSE_TO_FINE_WARMUP_FRAC * N_EPOCHS))
    n_train_batches = SAMPLES_PER_EPOCH // BATCH_SIZE
    n_val_batches = max(1, n_train_batches // 10)

    best_val_rmse = float("inf")
    best_state = None
    epochs_no_improve = 0

    for epoch in range(N_EPOCHS):
        active_frac = min(1.0, (epoch + 1) / warmup_epochs)
        focus_frac = FOCUS_FRAC_MAX * min(1.0, epoch / FOCUS_WARMUP_EPOCHS)

        model.train()
        total_loss, total_n = 0.0, 0
        pbar = tqdm(range(n_train_batches), desc=f"Epoch {epoch+1}/{N_EPOCHS}",
                    dynamic_ncols=True, file=sys.stdout)
        for _ in pbar:
            u, v, alt = sampler.sample_batch(BATCH_SIZE, device=device, focus_frac=focus_frac)
            optimizer.zero_grad()
            with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                pred = model(u, v, active_frac=active_frac)
                loss = loss_fn(pred, alt)
            scaler.scale(loss).backward()
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(model.parameters(), GRAD_CLIP_NORM)
            scaler.step(optimizer); scaler.update()
            total_loss += loss.item() * len(alt); total_n += len(alt)
        scheduler.step()

        model.eval()
        val_loss, val_n = 0.0, 0
        with torch.no_grad():
            for _ in range(n_val_batches):
                u, v, alt = sampler.sample_batch(BATCH_SIZE, device=device, focus_frac=0.0)
                with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                    pred = model(u, v, active_frac=1.0)
                    val_loss += loss_fn(pred, alt).item() * len(alt)
                val_n += len(alt)

        train_rmse = (total_loss/total_n)**0.5 * sampler.alt_std
        val_rmse = (val_loss/val_n)**0.5 * sampler.alt_std
        print(f"Epoch {epoch+1}: train RMSE={train_rmse:.2f} m | val RMSE={val_rmse:.2f} m")

        if val_rmse < best_val_rmse - 1e-4:
            best_val_rmse = val_rmse
            best_state = copy.deepcopy(model.state_dict())
            epochs_no_improve = 0
        else:
            epochs_no_improve += 1
            if epochs_no_improve >= PATIENCE_EPOCHS and active_frac >= 1.0:
                print(f"[early stopping] best val RMSE={best_val_rmse:.2f} m")
                break

    if best_state is not None:
        model.load_state_dict(best_state)

    checkpoint = {
        "model_state": model.state_dict(),
        "alt_mean": sampler.alt_mean,
        "alt_std": sampler.alt_std,
        "best_val_rmse_m": best_val_rmse,
        "limb_centers_deg": LIMB_CENTERS_DEG,
        "band_half_width_deg": BAND_HALF_WIDTH_DEG,
        "lat_min": sampler.lat_min, "lat_max": sampler.lat_max,
        "recommended_lon_cells": recommended_lon_cells,
    }
    torch.save(checkpoint, CHECKPOINT_PATH)
    print(f"Model salvat in {CHECKPOINT_PATH} (best val RMSE={best_val_rmse:.2f} m)")

    half_state = {k: (v.half() if "embeddings" in k else v) for k, v in model.state_dict().items()}
    checkpoint_fp16 = dict(checkpoint, model_state=half_state, embeddings_dtype="fp16")
    torch.save(checkpoint_fp16, CHECKPOINT_PATH_FP16)
    print(f"Model fp16 salvat in {CHECKPOINT_PATH_FP16}")


if __name__ == "__main__":
    train()