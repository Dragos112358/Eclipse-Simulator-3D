"""
Lunar Altitude Neural Network — antrenare pe DEM lunar (SLDEM2015/LOLA)
Versiune corectată pentru overfitting sever.

Schimbări față de varianta anterioară:
  1. Eșantionare CONTINUĂ (interpolare biliniară) la fiecare batch, în loc de
     un set fix de puncte refolosit la fiecare epocă -> elimină memorarea.
  2. FĂRĂ hold-out geografic: eșantionarea fiind deja continuă, nu există risc
     de memorare exactă -> train și val folosesc tot domeniul, ceea ce
     măsoară corect calitatea de reconstrucție (scopul real aici e să
     reprezinți întregul DEM, nu să generalizezi la zone neștiute).
  3. FINEST_RESOLUTION calculat corect din rezoluția nativă a DEM-ului
     (înainte era de ~14x prea grosier -> plafon structural de eroare).
  4. Coarse-to-fine: nivelele fine ale hash grid-ului se activează progresiv.
  5. Weight decay (mic pe MLP, minim pe embeddings) + gradient clipping.
  6. Early stopping + checkpoint pe baza celui mai bun val RMSE.
"""

import copy
import sys

import numpy as np
import rasterio
import torch
import torch.nn as nn
from tqdm import tqdm

DEM_PATH = "lunar_dem.tif"
CHECKPOINT_PATH = "lunar_altitude_model2.pt"

# ---------------------------------------------------------------------------
# Hiperparametri
# ---------------------------------------------------------------------------
N_LEVELS = 16
N_FEATURES = 2
LOG2_HASHMAP_SIZE = 20          # crescut pentru rezoluția fină de mai jos (mai puține coliziuni)
BASE_RESOLUTION = 16
FINEST_RESOLUTION = 32768       # ≈ lățime_px / π -> potrivit cu rezoluția nativă a DEM-ului
                                 # (2048 era de ~14x prea grosier -> de-acolo plafonul de
                                 # 115-120m pe train, indiferent cât antrenai)

N_EPOCHS = 800                  # antrenăm mai mult, dar cu early stopping
SAMPLES_PER_EPOCH = 2_000_000   # domeniul e uriaș; 500k/epocă acoperea prea puțin din el
BATCH_SIZE = 16384              # scade dacă apare out-of-memory pe GPU
LR = 5e-3
WEIGHT_DECAY_MLP = 1e-5
WEIGHT_DECAY_EMB = 1e-7
GRAD_CLIP_NORM = 1.0
PATIENCE_EPOCHS = 30            # early stopping

# Eșantionarea e deja CONTINUĂ (bilinear, re-generată la fiecare batch, nu un
# set fix) -> nu există risc de memorare exactă, deci nu mai e nevoie de un
# hold-out geografic. Un hold-out în regiuni (bandă sau celule de câțiva
# grade) crea "găuri" mult mai mari decât o celulă fină a hash grid-ului,
# ceea ce măsura extrapolare imposibilă, nu calitate reală. Train și val
# eșantionează acum din TOT domeniul -> RMSE-ul de val devine o măsură
# corectă de reconstrucție (asta chiar contează aici: rețeaua trebuie să
# reprezinte întregul DEM, nu să generalizeze la zone neștiute).
COARSE_TO_FINE_WARMUP_FRAC = 0.4  # fracție din N_EPOCHS în care se activează toate nivelele


# ---------------------------------------------------------------------------
# 1. Eșantionare continuă din DEM (interpolare biliniară)
# ---------------------------------------------------------------------------
class LunarDEMSampler:
    """Nu mai există un set fix de puncte: fiecare apel al sample_batch()
    generează coordonate noi, aleatoare, și interpolează altitudinea biliniar
    din rasterul original. Asta face setul de antrenare efectiv infinit și
    elimină posibilitatea ca rețeaua să memoreze exact niște puncte."""

    def __init__(self, dem_path):
        with rasterio.open(dem_path) as src:
            self.elevation = src.read(1).astype(np.float32)
            self.transform = src.transform
            self.inv_transform = ~self.transform
            self.height, self.width = self.elevation.shape

        corners_px = [(0, 0), (self.width, 0), (0, self.height), (self.width, self.height)]
        corner_coords = [self.transform * c for c in corners_px]
        lons = [c[0] for c in corner_coords]
        lats = [c[1] for c in corner_coords]
        self.lon_min, self.lon_max = min(lons), max(lons)
        self.lat_min, self.lat_max = min(lats), max(lats)

        # normalizare calculată pe un eșantion mare
        probe_lon, probe_lat = self._random_lonlat(200_000)
        probe_alt = self._sample_altitude_bilinear(probe_lon, probe_lat)
        probe_alt = probe_alt[np.isfinite(probe_alt)]
        self.alt_mean = float(probe_alt.mean())
        self.alt_std = float(probe_alt.std() + 1e-6)

        # pragul corect de rezoluție: o celulă a hash grid-ului la nivelul cel
        # mai fin acoperă ~114.6°/FINEST_RESOLUTION pe o sferă -> comparăm cu
        # rezoluția nativă a DEM-ului în aceleași unități (grade/pixel).
        deg_per_pixel = 360.0 / self.width
        deg_per_finest_cell = 114.6 / FINEST_RESOLUTION
        recommended = int(round(114.6 / deg_per_pixel))
        print(f"[info] DEM nativ: {self.width}x{self.height} px "
              f"(~{deg_per_pixel:.5f}°/pixel). FINEST_RESOLUTION recomandat ≈ {recommended}.")
        if deg_per_finest_cell > deg_per_pixel * 3:
            print(f"[avertisment] FINEST_RESOLUTION={FINEST_RESOLUTION} e mult mai grosier "
                  f"decât rezoluția nativă a DEM-ului -> rețeaua nu poate reprezenta detaliul "
                  f"real din date (plafon de eroare structural, nu overfitting). "
                  f"Crește-l spre {recommended}.")
        elif deg_per_finest_cell < deg_per_pixel / 3:
            print(f"[avertisment] FINEST_RESOLUTION={FINEST_RESOLUTION} e mult mai fin decât "
                  f"rezoluția nativă a DEM-ului -> rețeaua poate învăța zgomot de interpolare "
                  f"sub-pixel. Scade-l spre {recommended}.")

    def _random_lonlat(self, n):
        lon = np.random.uniform(self.lon_min, self.lon_max, size=n).astype(np.float32)
        lat = np.random.uniform(self.lat_min, self.lat_max, size=n).astype(np.float32)
        return lon, lat

    def _sample_altitude_bilinear(self, lon, lat):
        cols, rows = self.inv_transform * (np.asarray(lon, dtype=np.float64),
                                            np.asarray(lat, dtype=np.float64))

        c0 = np.floor(cols).astype(np.int64)
        r0 = np.floor(rows).astype(np.int64)

        # clamp pe indici ÎNTREGI, după floor -> imun la rotunjiri la marginea
        # exactă a domeniului (colțul (width,height) e valid în convenția GDAL
        # dar e chiar în afara ultimului pixel, deci trebuie prins aici, nu
        # printr-un clip pe valorile float dinainte de floor).
        c0 = np.clip(c0, 0, self.width - 2)
        r0 = np.clip(r0, 0, self.height - 2)
        c1 = c0 + 1
        r1 = r0 + 1

        fc = np.clip(cols - c0, 0.0, 1.0)
        fr = np.clip(rows - r0, 0.0, 1.0)

        e00 = self.elevation[r0, c0]
        e01 = self.elevation[r0, c1]
        e10 = self.elevation[r1, c0]
        e11 = self.elevation[r1, c1]

        top = e00 * (1 - fc) + e01 * fc
        bot = e10 * (1 - fc) + e11 * fc
        return top * (1 - fr) + bot * fr

    def sample_batch(self, n, device="cpu"):
        lon, lat = self._random_lonlat(n)
        alt = self._sample_altitude_bilinear(lon, lat)

        valid = np.isfinite(alt)
        lon, lat, alt = lon[valid], lat[valid], alt[valid]

        lat_r = np.radians(lat)
        lon_r = np.radians(lon)
        x = np.cos(lat_r) * np.cos(lon_r)
        y = np.cos(lat_r) * np.sin(lon_r)
        z = np.sin(lat_r)

        coords = torch.tensor(np.stack([x, y, z], axis=1), dtype=torch.float32, device=device)
        alt_norm = torch.tensor((alt - self.alt_mean) / self.alt_std,
                                 dtype=torch.float32, device=device)
        return coords, alt_norm


# ---------------------------------------------------------------------------
# 2. Multi-resolution hash grid encoding, cu activare progresivă (coarse-to-fine)
# ---------------------------------------------------------------------------
class MultiResHashEncoding(nn.Module):
    def __init__(self, n_levels=N_LEVELS, n_features=N_FEATURES,
                 log2_hashmap_size=LOG2_HASHMAP_SIZE,
                 base_resolution=BASE_RESOLUTION,
                 finest_resolution=FINEST_RESOLUTION):
        super().__init__()
        self.n_levels = n_levels
        self.n_features = n_features
        self.hashmap_size = 2 ** log2_hashmap_size

        b = np.exp((np.log(finest_resolution) - np.log(base_resolution)) / (n_levels - 1))
        self.resolutions = [int(base_resolution * (b ** l)) for l in range(n_levels)]

        self.embeddings = nn.ModuleList([
            nn.Embedding(self.hashmap_size, n_features) for _ in range(n_levels)
        ])
        for emb in self.embeddings:
            nn.init.uniform_(emb.weight, -1e-4, 1e-4)

        self.register_buffer("primes", torch.tensor([1, 2654435761, 805459861], dtype=torch.int64))

    def hash_coords(self, coords_int, level):
        res = self.resolutions[level]
        if (res + 1) ** 3 <= self.hashmap_size:
            idx = (coords_int[:, 0] * (res + 1) * (res + 1)
                   + coords_int[:, 1] * (res + 1)
                   + coords_int[:, 2])
            return idx % self.hashmap_size
        xored = coords_int[:, 0] * self.primes[0]
        xored ^= coords_int[:, 1] * self.primes[1]
        xored ^= coords_int[:, 2] * self.primes[2]
        return xored % self.hashmap_size

    def forward(self, coords, active_frac=1.0):
        """active_frac in [0,1]: controlează câte nivele (de la grosier la fin)
        sunt active. La active_frac=1.0 toate nivelele contribuie normal.
        Folosit pentru schema coarse-to-fine anti-overfitting."""
        coords01 = (coords + 1) / 2
        outputs = []
        for level in range(self.n_levels):
            res = self.resolutions[level]
            scaled = coords01 * res
            floor = torch.floor(scaled)
            frac = scaled - floor
            floor = floor.long()

            level_feat = 0.0
            for dx in (0, 1):
                for dy in (0, 1):
                    for dz in (0, 1):
                        offset = torch.tensor([dx, dy, dz], device=coords.device)
                        corner = floor + offset
                        idx = self.hash_coords(corner, level)
                        feat = self.embeddings[level](idx)
                        w = (torch.where(offset[0] == 1, frac[:, 0], 1 - frac[:, 0]) *
                             torch.where(offset[1] == 1, frac[:, 1], 1 - frac[:, 1]) *
                             torch.where(offset[2] == 1, frac[:, 2], 1 - frac[:, 2]))
                        level_feat = level_feat + feat * w.unsqueeze(-1)

            level_weight = float(np.clip(active_frac * self.n_levels - level, 0.0, 1.0))
            outputs.append(level_feat * level_weight)
        return torch.cat(outputs, dim=-1)


# ---------------------------------------------------------------------------
# 3. Model
# ---------------------------------------------------------------------------
class LunarAltitudeNet(nn.Module):
    def __init__(self, hidden=128):
        super().__init__()
        self.encoding = MultiResHashEncoding()
        in_dim = N_LEVELS * N_FEATURES
        self.mlp = nn.Sequential(
            nn.Linear(in_dim, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, 1),
        )

    def forward(self, coords, active_frac=1.0):
        feats = self.encoding(coords, active_frac=active_frac)
        return self.mlp(feats).squeeze(-1)


# ---------------------------------------------------------------------------
# 4. Training loop
# ---------------------------------------------------------------------------
def train():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}")

    sampler = LunarDEMSampler(DEM_PATH)
    model = LunarAltitudeNet().to(device)

    # grupuri de parametri: weight decay diferit pentru embeddings vs. MLP
    emb_params = list(model.encoding.embeddings.parameters())
    mlp_params = list(model.mlp.parameters())
    optimizer = torch.optim.Adam([
        {"params": emb_params, "weight_decay": WEIGHT_DECAY_EMB},
        {"params": mlp_params, "weight_decay": WEIGHT_DECAY_MLP},
    ], lr=LR, betas=(0.9, 0.99), eps=1e-15)

    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=N_EPOCHS)
    loss_fn = nn.MSELoss()
    scaler = torch.amp.GradScaler("cuda" if device == "cuda" else "cpu")

    warmup_epochs = max(1, int(COARSE_TO_FINE_WARMUP_FRAC * N_EPOCHS))
    n_train_batches = SAMPLES_PER_EPOCH // BATCH_SIZE
    n_val_batches = max(1, n_train_batches // 10)

    best_val_rmse = float("inf")
    best_state = None
    epochs_no_improve = 0

    for epoch in range(N_EPOCHS):
        active_frac = min(1.0, (epoch + 1) / warmup_epochs)

        model.train()
        total_loss = 0.0
        total_n = 0
        pbar = tqdm(range(n_train_batches), desc=f"Epoch {epoch + 1}/{N_EPOCHS}",
                    dynamic_ncols=True, file=sys.stdout)
        for _ in pbar:
            coords, alt = sampler.sample_batch(BATCH_SIZE, device=device)
            optimizer.zero_grad()

            with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                pred = model(coords, active_frac=active_frac)
                loss = loss_fn(pred, alt)

            scaler.scale(loss).backward()
            scaler.unscale_(optimizer)
            torch.nn.utils.clip_grad_norm_(model.parameters(), GRAD_CLIP_NORM)
            scaler.step(optimizer)
            scaler.update()

            total_loss += loss.item() * len(alt)
            total_n += len(alt)
        scheduler.step()

        model.eval()
        val_loss = 0.0
        val_n = 0
        with torch.no_grad():
            for _ in range(n_val_batches):
                coords, alt = sampler.sample_batch(BATCH_SIZE, device=device)
                with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                    pred = model(coords, active_frac=1.0)  # la validare, toate nivelele active
                    val_loss += loss_fn(pred, alt).item() * len(alt)
                val_n += len(alt)

        train_rmse_m = (total_loss / total_n) ** 0.5 * sampler.alt_std
        val_rmse_m = (val_loss / val_n) ** 0.5 * sampler.alt_std
        gap = val_rmse_m - train_rmse_m
        print(f"Epoch {epoch + 1}: train RMSE={train_rmse_m:.2f} m | "
              f"val RMSE={val_rmse_m:.2f} m | gap={gap:.2f} m | "
              f"active_frac={active_frac:.2f}")

        if val_rmse_m < best_val_rmse - 1e-4:
            best_val_rmse = val_rmse_m
            best_state = copy.deepcopy(model.state_dict())
            epochs_no_improve = 0
        else:
            epochs_no_improve += 1
            if epochs_no_improve >= PATIENCE_EPOCHS and active_frac >= 1.0:
                print(f"[early stopping] fără îmbunătățire de {PATIENCE_EPOCHS} epoci "
                      f"(best val RMSE={best_val_rmse:.2f} m).")
                break

    if best_state is not None:
        model.load_state_dict(best_state)

    torch.save({
        "model_state": model.state_dict(),
        "alt_mean": sampler.alt_mean,
        "alt_std": sampler.alt_std,
        "best_val_rmse_m": best_val_rmse,
    }, CHECKPOINT_PATH)
    print(f"Model salvat in {CHECKPOINT_PATH} (best val RMSE={best_val_rmse:.2f} m)")


# ---------------------------------------------------------------------------
# 5. Inferenta: lat/lon (grade) -> altitudine (metri)
# ---------------------------------------------------------------------------
def predict(lat_deg, lon_deg, device=None):
    device = device or ("cuda" if torch.cuda.is_available() else "cpu")
    ckpt = torch.load(CHECKPOINT_PATH, map_location=device)
    model = LunarAltitudeNet().to(device)
    model.load_state_dict(ckpt["model_state"])
    model.eval()

    lat_r = np.radians(np.atleast_1d(lat_deg))
    lon_r = np.radians(np.atleast_1d(lon_deg))
    x = np.cos(lat_r) * np.cos(lon_r)
    y = np.cos(lat_r) * np.sin(lon_r)
    z = np.sin(lat_r)
    coords = torch.tensor(np.stack([x, y, z], axis=1), dtype=torch.float32, device=device)

    with torch.no_grad():
        with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
            pred_norm = model(coords, active_frac=1.0)
    return pred_norm.cpu().numpy() * ckpt["alt_std"] + ckpt["alt_mean"]


if __name__ == "__main__":
    train()
    # exemplu de inferenta dupa antrenare:
    # print(predict(lat_deg=[-30.5], lon_deg=[45.2]))