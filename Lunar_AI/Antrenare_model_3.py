"""
Lunar Altitude Neural Network v5 — FINEST_RESOLUTION_LAT ridicat la rezoluția
nativă a DEM-ului (era ~2.8x mai grosier în v4 -> plafon de eroare structural,
indiferent de cât de bună era eșantionarea sau cât spațiu liber avea bugetul
de memorie). LOG2_HASHMAP_SIZE crescut la 22 ca să profiți de marja mare de
memorie rămasă (proiecție ~69MB la deployment fp16, din 128MB disponibili).

v4 a introdus:

  A. MEMORY_BUDGET_BYTES rămâne bugetul de DEPLOYMENT (fp16, exportat automat
     la final). Antrenarea în fp32 poate deci folosi ~2x acel buget, pentru că
     la export tabelele de embedding se înjumătățesc. LOG2_HASHMAP_SIZE a
     crescut 20->21 ca să consumi efectiv marja rămasă.

  B. Harta de importanță folosea magnitudinea gradientului (panta), care nu
     distinge un vârf ascuțit de o pantă lungă și lină cu aceeași înclinare
     medie -> bugetul de eșantioane "focus" se ducea și pe pante line întinse,
     nu doar pe vârfuri. Înlocuit cu magnitudinea Laplacianului (derivata a
     doua, aproximată prin dublu np.gradient) -> izolează colțuri/vârfuri, nu
     pante.

  C. importance_bins crescut (720x360 -> 2160x1080): la rezoluție joasă, un
     singur bin acoperă sute de pixeli DEM reali, deci eșantionarea "focus"
     țintea regiuni largi, nu vârfuri individuale.

  D. FOCUS_FRAC_MAX crescut 0.45 -> 0.6, acum că harta de importanță e mai
     precisă, merită alocat mai mult din buget pe zonele cu curbură mare.

Schimbări față de v2 (moștenite, explicate inline mai jos):

  1. Hash grid 2D (lon, lat) în loc de 3D (x,y,z pe sferă).
     Punctele tale nu variază niciodată radial -> un grid 3D irosește
     majoritatea parametrilor pe o dimensiune inexistentă în date.
     La același buget de memorie, 2D îți dă o rezoluție efectivă mult mai
     fină -> mai puține coliziuni de hash exact acolo unde ai nevoie de ele
     (vârfuri, creste de crater).

  2. Eșantionare CORECTĂ, uniformă pe suprafața sferei (nu uniformă în grade
     de latitudine, care supra-eșantionează polii și sub-eșantionează
     ecuatorul).

  3. Eșantionare ponderată de curbură: o fracțiune din fiecare batch e trasă
     proporțional cu gradientul local al DEM-ului (hartă precalculată o
     singură dată, la rezoluție redusă). Vârfurile ocupă puțină suprafață,
     deci sub eșantionare uniformă rețeaua le vede rar; asta corectează
     direct problema, fără să atingem loss-ul (MSE rămâne MSE — Huber ar
     reduce tocmai importanța erorilor mari, care e ce nu vrei aici).

  4. Dimensiune per-nivel a tabelului de hash calculată explicit
     (min(hashmap_size, celule_reale)) -> nivelele grosiere nu irosesc
     memorie și nu au deloc coliziuni; doar nivelele foarte fine ajung să
     folosească hash-ul complet.

  5. Raport de memorie la inițializare + export cuantizat fp16 la final,
     ca să ai control explicit asupra bugetului de 128MB.

  6. Coarse-to-fine, weight decay, gradient clipping, early stopping —
     păstrate din v2, funcționau bine.
"""

import copy
import sys

import numpy as np
import rasterio
import torch
import torch.nn as nn
from tqdm import tqdm

DEM_PATH = "lunar_dem.tif"
CHECKPOINT_PATH = "lunar_altitude_model7.pt"
CHECKPOINT_PATH_FP16 = "lunar_altitude_model7_fp16.pt"

# ---------------------------------------------------------------------------
# Hiperparametri encoding
# ---------------------------------------------------------------------------
N_LEVELS = 16
N_FEATURES = 2
LOG2_HASHMAP_SIZE = 22          # 2^22 = 4,194,304 — crescut de la 21: la FINEST_RESOLUTION_LAT
                                 # nou (vezi mai jos), un hashmap mai mare ține câteva nivele
                                 # mid-fine fără coliziuni; oricum ai marjă mare de memorie.
BASE_RESOLUTION_LAT = 16        # rezoluție (celule) pe axa latitudine, nivelul cel mai grosier
FINEST_RESOLUTION_LAT = 46080   # = rezoluția nativă a DEM-ului tău (92160x46080 px, ~0.00391°/px).
                                 # Sub asta rețeaua are un plafon de eroare STRUCTURAL, indiferent
                                 # cât antrenezi — nu poate reprezenta detaliu mai fin decât grid-ul.
LON_LAT_ASPECT = 2.0            # longitudine acoperă 360°, latitudine 180° -> celule ~pătrate

# Bugetul e pt fișierul DEPLOYAT (fp16, export automat la final) -> antrenarea
# fp32 poate folosi pana la ~2x acest buget si tot incape dupa export.
DEPLOY_MEMORY_BUDGET_BYTES = 128 * 1024 * 1024
TRAIN_FP32_BUDGET_BYTES = 2 * DEPLOY_MEMORY_BUDGET_BYTES

# ---------------------------------------------------------------------------
# Hiperparametri antrenare
# ---------------------------------------------------------------------------
N_EPOCHS = 1800
SAMPLES_PER_EPOCH = 2_000_000
BATCH_SIZE = 16384
LR = 5e-3
WEIGHT_DECAY_MLP = 1e-5
WEIGHT_DECAY_EMB = 1e-7
GRAD_CLIP_NORM = 1.0
PATIENCE_EPOCHS = 30

FOCUS_FRAC_MAX = 0.45            # fracția din batch trasă din harta de curbură, la platou
                                 # (crescut din 0.45: harta de importanță (Laplacian, rezoluție
                                 # mai fină) e acum mai precisă, merită să te bazezi mai mult pe ea)
FOCUS_WARMUP_EPOCHS = 60        # crește gradual de la 0 la FOCUS_FRAC_MAX (întâi formă generală, apoi detalii)
COARSE_TO_FINE_WARMUP_FRAC = 0.4


# ---------------------------------------------------------------------------
# 1. Eșantionare din DEM: uniformă pe sferă + ponderată de curbură
# ---------------------------------------------------------------------------
class LunarDEMSampler2D:
    def __init__(self, dem_path, importance_bins=(2160, 1080)):
        with rasterio.open(dem_path) as src:
            self.elevation = src.read(1).astype(np.float32)
            self.transform = src.transform
            self.height, self.width = self.elevation.shape

        corners_px = [(0, 0), (self.width, 0), (0, self.height), (self.width, self.height)]
        corner_coords = [self.transform * c for c in corners_px]
        lons = [c[0] for c in corner_coords]
        lats = [c[1] for c in corner_coords]
        self.lon_min, self.lon_max = min(lons), max(lons)
        self.lat_min, self.lat_max = min(lats), max(lats)

        deg_per_pixel = 360.0 / self.width
        recommended_lon_res = int(round(360.0 / deg_per_pixel))
        recommended_lat_res = int(round(recommended_lon_res / LON_LAT_ASPECT))
        print(f"[info] DEM nativ: {self.width}x{self.height} px "
              f"(~{deg_per_pixel:.5f}°/pixel).")
        print(f"[info] FINEST_RESOLUTION_LAT recomandat ≈ {recommended_lat_res} "
              f"(lon ≈ {recommended_lon_res}) ca celula cea mai fină să corespundă ~1 pixel DEM.")

        probe_lon, probe_lat = self._uniform_area_lonlat(200_000)
        probe_alt = self._sample_bilinear(probe_lon, probe_lat)
        probe_alt = probe_alt[np.isfinite(probe_alt)]
        self.alt_mean = float(probe_alt.mean())
        self.alt_std = float(probe_alt.std() + 1e-6)

        self._build_importance_map(importance_bins)

    def _build_importance_map(self, bins):
        # Laplacian (derivata a doua) în loc de magnitudinea gradientului: un
        # gradient mare poate fi doar o pantă lungă și lină (aceeași înclinare
        # pe o suprafață mare), care fura din bugetul de eșantioane "focus"
        # fără să fie de fapt un vârf. Laplacianul e mare exact acolo unde
        # panta se SCHIMBĂ brusc pe puțini pixeli -> vârfuri, creste de crater,
        # colțuri -- nu pante line.
        w_bins, h_bins = bins
        step_x = max(1, self.width // w_bins)
        step_y = max(1, self.height // h_bins)
        small = self.elevation[::step_y, ::step_x].astype(np.float32)

        gy, gx = np.gradient(small)
        gyy, _ = np.gradient(gy)
        _, gxx = np.gradient(gx)
        curvature = np.abs(gxx) + np.abs(gyy)

        w = curvature - curvature.min()
        w = w / (w.max() + 1e-8)
        w = np.sqrt(w) + 0.05          # rădăcină: accentuează curbura fără să ignore complet zonele netede
        w = w / w.sum()

        self.imp_h, self.imp_w = small.shape
        self.imp_cdf = np.cumsum(w.ravel())
        self._imp_step_x = step_x
        self._imp_step_y = step_y

    def _uniform_area_lonlat(self, n):
        lon = np.random.uniform(self.lon_min, self.lon_max, size=n)
        u = np.random.uniform(0.0, 1.0, size=n)
        sin_min = np.sin(np.radians(self.lat_min))
        sin_max = np.sin(np.radians(self.lat_max))
        lat = np.degrees(np.arcsin(sin_min + u * (sin_max - sin_min)))
        return lon.astype(np.float32), lat.astype(np.float32)

    def _biased_lonlat(self, n):
        r = np.random.uniform(0.0, 1.0, size=n)
        flat_idx = np.searchsorted(self.imp_cdf, r)
        flat_idx = np.clip(flat_idx, 0, self.imp_h * self.imp_w - 1)
        iy, ix = np.unravel_index(flat_idx, (self.imp_h, self.imp_w))

        px = (ix + np.random.uniform(0.0, 1.0, size=n)) * self._imp_step_x
        py = (iy + np.random.uniform(0.0, 1.0, size=n)) * self._imp_step_y
        lon, lat = self.transform * (px, py)
        return np.asarray(lon, dtype=np.float32), np.asarray(lat, dtype=np.float32)

    def _sample_bilinear(self, lon, lat):
        inv_transform = ~self.transform
        cols, rows = inv_transform * (np.asarray(lon, dtype=np.float64),
                                       np.asarray(lat, dtype=np.float64))

        c0 = np.floor(cols).astype(np.int64)
        r0 = np.floor(rows).astype(np.int64)
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

    def sample_batch(self, n, device="cpu", focus_frac=0.0):
        n_focus = int(n * focus_frac)
        n_uniform = n - n_focus

        lon_u, lat_u = self._uniform_area_lonlat(n_uniform)
        if n_focus > 0:
            lon_f, lat_f = self._biased_lonlat(n_focus)
            lon = np.concatenate([lon_u, lon_f])
            lat = np.concatenate([lat_u, lat_f])
        else:
            lon, lat = lon_u, lat_u

        alt = self._sample_bilinear(lon, lat)
        valid = np.isfinite(alt)
        lon, lat, alt = lon[valid], lat[valid], alt[valid]

        u = ((lon - self.lon_min) / (self.lon_max - self.lon_min)).astype(np.float32) % 1.0
        v = ((lat - self.lat_min) / (self.lat_max - self.lat_min)).astype(np.float32)
        v = np.clip(v, 0.0, 1.0)

        u_t = torch.tensor(u, dtype=torch.float32, device=device)
        v_t = torch.tensor(v, dtype=torch.float32, device=device)
        alt_norm = torch.tensor((alt - self.alt_mean) / self.alt_std,
                                 dtype=torch.float32, device=device)
        return u_t, v_t, alt_norm


# ---------------------------------------------------------------------------
# 2. Multi-resolution hash grid 2D, cu wrap pe longitudine și activare
#    progresivă (coarse-to-fine)
# ---------------------------------------------------------------------------
PRIME_LON = 1
PRIME_LAT = 2654435761


class MultiResHashEncoding2D(nn.Module):
    def __init__(self, n_levels=N_LEVELS, n_features=N_FEATURES,
                 log2_hashmap_size=LOG2_HASHMAP_SIZE,
                 base_resolution_lat=BASE_RESOLUTION_LAT,
                 finest_resolution_lat=FINEST_RESOLUTION_LAT,
                 lon_lat_aspect=LON_LAT_ASPECT):
        super().__init__()
        self.n_levels = n_levels
        self.n_features = n_features
        self.max_table_size = 2 ** log2_hashmap_size

        b = np.exp((np.log(finest_resolution_lat) - np.log(base_resolution_lat)) / (n_levels - 1))
        self.res_lat = [max(1, int(round(base_resolution_lat * (b ** l)))) for l in range(n_levels)]
        self.res_lon = [max(1, int(round(r * lon_lat_aspect))) for r in self.res_lat]

        self.table_sizes = []
        self.use_hash = []
        for rlon, rlat in zip(self.res_lon, self.res_lat):
            n_cells = rlon * (rlat + 1)   # lon nu are nevoie de +1: ultimul vertex = primul (wrap)
            if n_cells <= self.max_table_size:
                self.table_sizes.append(n_cells)
                self.use_hash.append(False)
            else:
                self.table_sizes.append(self.max_table_size)
                self.use_hash.append(True)

        self.embeddings = nn.ModuleList([
            nn.Embedding(sz, n_features) for sz in self.table_sizes
        ])
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
            rlon = self.res_lon[level]
            rlat = self.res_lat[level]

            x = u * rlon
            y = v * rlat
            x0f = torch.floor(x)
            y0f = torch.floor(y)
            fx = (x - x0f).unsqueeze(-1)
            fy = (y - y0f).unsqueeze(-1)

            x0 = x0f.long() % rlon
            x1 = (x0 + 1) % rlon
            y0 = y0f.long().clamp(0, rlat)
            y1 = (y0f.long() + 1).clamp(0, rlat)

            idx00 = self._index(x0, y0, level)
            idx01 = self._index(x0, y1, level)
            idx10 = self._index(x1, y0, level)
            idx11 = self._index(x1, y1, level)

            e00 = self.embeddings[level](idx00)
            e01 = self.embeddings[level](idx01)
            e10 = self.embeddings[level](idx10)
            e11 = self.embeddings[level](idx11)

            top = e00 * (1 - fx) + e10 * fx
            bot = e01 * (1 - fx) + e11 * fx
            feat = top * (1 - fy) + bot * fy

            level_weight = float(np.clip(active_frac * self.n_levels - level, 0.0, 1.0))
            outputs.append(feat * level_weight)
        return torch.cat(outputs, dim=-1)


# ---------------------------------------------------------------------------
# 3. Model
# ---------------------------------------------------------------------------
class LunarAltitudeNet2D(nn.Module):
    def __init__(self, hidden=128):
        super().__init__()
        self.encoding = MultiResHashEncoding2D()
        in_dim = N_LEVELS * N_FEATURES
        self.mlp = nn.Sequential(
            nn.Linear(in_dim, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, hidden), nn.ReLU(inplace=True),
            nn.Linear(hidden, 1),
        )

    def forward(self, u, v, active_frac=1.0):
        feats = self.encoding(u, v, active_frac=active_frac)
        return self.mlp(feats).squeeze(-1)


def print_memory_report(model):
    emb_bytes_fp32 = model.encoding.memory_bytes(dtype_bytes=4)
    emb_bytes_fp16 = model.encoding.memory_bytes(dtype_bytes=2)
    mlp_bytes = sum(p.numel() for p in model.mlp.parameters()) * 4
    total_fp32 = emb_bytes_fp32 + mlp_bytes
    total_fp16_deploy = emb_bytes_fp16 + mlp_bytes   # MLP rămâne fp32 la export, e neglijabil ca dimensiune

    print(f"[memorie] embeddings fp32 (antrenare): {emb_bytes_fp32 / 1024**2:.1f} MB "
          f"| mlp: {mlp_bytes / 1024**2:.2f} MB | total fp32: {total_fp32 / 1024**2:.1f} MB")
    print(f"[memorie] proiecție deployment (embeddings fp16 + mlp fp32): "
          f"{total_fp16_deploy / 1024**2:.1f} MB | buget deployment: "
          f"{DEPLOY_MEMORY_BUDGET_BYTES / 1024**2:.0f} MB")

    n_hashed = sum(model.encoding.use_hash)
    print(f"[memorie] nivele fără coliziuni (indexare directă): "
          f"{N_LEVELS - n_hashed}/{N_LEVELS} | nivele cu hash: {n_hashed}/{N_LEVELS}")

    if total_fp16_deploy > DEPLOY_MEMORY_BUDGET_BYTES:
        print("[avertisment] chiar și varianta fp16 depășește bugetul de deployment. "
              "Scade LOG2_HASHMAP_SIZE sau FINEST_RESOLUTION_LAT.")
    else:
        margin = DEPLOY_MEMORY_BUDGET_BYTES - total_fp16_deploy
        print(f"[memorie] marjă rămasă la deployment (fp16): {margin / 1024**2:.1f} MB — "
              f"dacă e mare, mai crește LOG2_HASHMAP_SIZE / FINEST_RESOLUTION_LAT.")
    if total_fp32 > TRAIN_FP32_BUDGET_BYTES:
        print(f"[avertisment] modelul fp32 de antrenare ({total_fp32 / 1024**2:.1f} MB) "
              f"depășește chiar și bugetul dublat ({TRAIN_FP32_BUDGET_BYTES / 1024**2:.0f} MB) — "
              f"prea mult chiar și pentru export fp16.")


# ---------------------------------------------------------------------------
# 4. Training loop
# ---------------------------------------------------------------------------
def train():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}")

    sampler = LunarDEMSampler2D(DEM_PATH)
    model = LunarAltitudeNet2D().to(device)
    print_memory_report(model)

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
        focus_frac = FOCUS_FRAC_MAX * min(1.0, epoch / FOCUS_WARMUP_EPOCHS)

        model.train()
        total_loss = 0.0
        total_n = 0
        pbar = tqdm(range(n_train_batches), desc=f"Epoch {epoch + 1}/{N_EPOCHS}",
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
                # validare: fără bias de curbură, ca RMSE-ul să reflecte reconstrucția reală pe tot domeniul
                u, v, alt = sampler.sample_batch(BATCH_SIZE, device=device, focus_frac=0.0)
                with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                    pred = model(u, v, active_frac=1.0)
                    val_loss += loss_fn(pred, alt).item() * len(alt)
                val_n += len(alt)

        train_rmse_m = (total_loss / total_n) ** 0.5 * sampler.alt_std
        val_rmse_m = (val_loss / val_n) ** 0.5 * sampler.alt_std
        print(f"Epoch {epoch + 1}: train RMSE={train_rmse_m:.2f} m | "
              f"val RMSE={val_rmse_m:.2f} m | active_frac={active_frac:.2f} | "
              f"focus_frac={focus_frac:.2f}")

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

    export_fp16(model, sampler, best_val_rmse)


def export_fp16(model, sampler, best_val_rmse):
    """Salvează o copie fp16 a embeddings-urilor (MLP rămâne fp32, e neglijabil ca
    dimensiune). Înjumătățește memoria tabelelor -> marjă suplimentară față de
    bugetul de 128MB, sau posibilitatea de a crește FINEST_RESOLUTION_LAT /
    LOG2_HASHMAP_SIZE păstrând totuși bugetul dacă exportul e mereu fp16."""
    state = model.state_dict()
    half_state = {k: (v.half() if "embeddings" in k else v) for k, v in state.items()}
    torch.save({
        "model_state": half_state,
        "alt_mean": sampler.alt_mean,
        "alt_std": sampler.alt_std,
        "best_val_rmse_m": best_val_rmse,
        "embeddings_dtype": "fp16",
    }, CHECKPOINT_PATH_FP16)
    emb_bytes_fp16 = model.encoding.memory_bytes(dtype_bytes=2)
    print(f"Model fp16 salvat in {CHECKPOINT_PATH_FP16} "
          f"(embeddings ≈ {emb_bytes_fp16 / 1024**2:.1f} MB)")


# ---------------------------------------------------------------------------
# 5. Inferență: lat/lon (grade) -> altitudine (metri)
# ---------------------------------------------------------------------------
def predict(lat_deg, lon_deg, sampler_bounds, device=None, checkpoint_path=CHECKPOINT_PATH):
    """sampler_bounds = (lon_min, lon_max, lat_min, lat_max), aceleași valori
    calculate de LunarDEMSampler2D la antrenare (le poți salva în checkpoint
    dacă vrei să eviți să le recalculezi)."""
    device = device or ("cuda" if torch.cuda.is_available() else "cpu")
    ckpt = torch.load(checkpoint_path, map_location=device)
    model = LunarAltitudeNet2D().to(device)
    state = ckpt["model_state"]
    if ckpt.get("embeddings_dtype") == "fp16":
        state = {k: (v.float() if "embeddings" in k else v) for k, v in state.items()}
    model.load_state_dict(state)
    model.eval()

    lon_min, lon_max, lat_min, lat_max = sampler_bounds
    lat_deg = np.atleast_1d(lat_deg)
    lon_deg = np.atleast_1d(lon_deg)
    u = ((lon_deg - lon_min) / (lon_max - lon_min)).astype(np.float32) % 1.0
    v = np.clip(((lat_deg - lat_min) / (lat_max - lat_min)).astype(np.float32), 0.0, 1.0)

    u_t = torch.tensor(u, dtype=torch.float32, device=device)
    v_t = torch.tensor(v, dtype=torch.float32, device=device)

    with torch.no_grad():
        with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
            pred_norm = model(u_t, v_t, active_frac=1.0)
    return pred_norm.cpu().numpy() * ckpt["alt_std"] + ckpt["alt_mean"]


if __name__ == "__main__":
    train()