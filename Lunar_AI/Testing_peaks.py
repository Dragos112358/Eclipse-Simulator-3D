"""
Validare specifica pentru limb profile: compara modelul cu DEM-ul brut
pe extreme locale (varfuri/creste) folosind blocuri (tiling) pentru a nu umple VRAM-ul.
"""
import numpy as np
import torch
from scipy.ndimage import maximum_filter

from Antrenare_model import DEM_PATH, LunarDEMSampler, LunarAltitudeNet
from Testare_lunar_altitude_2 import CHECKPOINT

#CHECKPOINT = "Best_lunar_model.pt"
CHECKPOINT = "lunar_altitude_model2.pt"

def find_local_peaks_tiled(elevation, size=15, top_k=2000, tile_size=4096):
    """
    Gaseste varfuri locale folosind scipy pe blocuri (tiles) pentru a preveni
    blocarea memoriei RAM/VRAM si a rula eficient.
    """
    H, W = elevation.shape
    all_peak_rows = []
    all_peak_cols = []
    all_peak_vals = []

    # Procesam imaginea in bucati (cu un mic overlap de siguranta de 'size')
    overlap = size
    for r in range(0, H, tile_size - overlap):
        r_end = min(r + tile_size, H)
        r_start_valid = 0 if r == 0 else overlap // 2
        r_end_valid = (r_end - r) if r_end == H else (r_end - r - overlap // 2)

        for c in range(0, W, tile_size - overlap):
            c_end = min(c + tile_size, W)
            c_start_valid = 0 if c == 0 else overlap // 2
            c_end_valid = (c_end - c) if c_end == W else (c_end - c - overlap // 2)

            # Extragem tile-ul
            tile = elevation[r:r_end, c:c_end]

            # Aplicam filtru maxim local pe tile
            local_max = maximum_filter(tile, size=size) == tile

            # Restrictionăm la zona validă a tile-ului pentru a evita duplicatele la margini
            valid_mask = np.zeros_like(tile, dtype=bool)
            valid_mask[r_start_valid:r_end_valid, c_start_valid:c_end_valid] = True
            local_max = np.logical_and(local_max, valid_mask)

            p_Rows, p_Cols = np.where(local_max)
            p_Vals = tile[p_Rows, p_Cols]

            # Adaugam offset-ul global
            all_peak_rows.append(p_Rows + r)
            all_peak_cols.append(p_Cols + c)
            all_peak_vals.append(p_Vals)

    peak_rows = np.concatenate(all_peak_rows)
    peak_cols = np.concatenate(all_peak_cols)
    peak_vals = np.concatenate(all_peak_vals)

    # Sortam si pastram top_k globale
    order = np.argsort(peak_vals)[::-1][:top_k]
    return peak_rows[order], peak_cols[order], peak_vals[order]


def eval_on_peaks(model, sampler, device):
    print("Se cauta varfurile locale in DEM (metoda pe blocuri)...")
    rows, cols, true_vals = find_local_peaks_tiled(sampler.elevation, size=15, top_k=2000)

    # Conversie coordonate
    try:
        lon, lat = sampler.transform * (cols, rows)
    except Exception:
        lonlat = [sampler.transform * (c, r) for c, r in zip(cols, rows)]
        lon = np.array([p[0] for p in lonlat])
        lat = np.array([p[1] for p in lonlat])

    # Transformare sferica -> coordonate 3D direct pe GPU
    lat_r = torch.deg2rad(torch.as_tensor(lat, dtype=torch.float32, device=device))
    lon_r = torch.deg2rad(torch.as_tensor(lon, dtype=torch.float32, device=device))

    x = torch.cos(lat_r) * torch.cos(lon_r)
    y = torch.cos(lat_r) * torch.sin(lon_r)
    z = torch.sin(lat_r)
    coords = torch.stack([x, y, z], dim=1)

    # Evaluare pe GPU in batches mici pentru siguranta totala a VRAM-ului
    model.eval()
    preds = []
    BATCH_SIZE = 4096

    with torch.no_grad():
        for i in range(0, len(coords), BATCH_SIZE):
            batch_coords = coords[i: i + BATCH_SIZE]
            pred_norm = model(batch_coords, active_frac=1.0)
            preds.append(pred_norm.cpu().numpy().flatten())

    pred = np.concatenate(preds) * sampler.alt_std + sampler.alt_mean
    true_vals = true_vals.flatten()

    err = pred - true_vals
    print(f"Varfuri testate: {len(true_vals)}")
    print(f"Eroare medie (bias): {err.mean():.2f} m")
    print(f"RMSE pe varfuri: {np.sqrt((err ** 2).mean()):.2f} m")
    print(f"Eroare maxima: {np.abs(err).max():.2f} m")
    return err


def main():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device utilizat: {device}")

    ckpt = torch.load(CHECKPOINT, map_location=device)

    sampler = LunarDEMSampler(DEM_PATH)
    sampler.alt_mean = ckpt["alt_mean"]
    sampler.alt_std = ckpt["alt_std"]

    model = LunarAltitudeNet().to(device)
    model.load_state_dict(ckpt["model_state"])

    eval_on_peaks(model, sampler, device)


if __name__ == "__main__":
    main()