"""
Validare specifica pentru limb profile: compara modelul cu DEM-ul brut
pe 100 de puncte random alese dintre extremele locale (varfuri/creste).
"""
import numpy as np
import torch
from scipy.ndimage import maximum_filter

from Antrenare_model import DEM_PATH, LunarDEMSampler, LunarAltitudeNet
from Testare_lunar_altitude_2 import CHECKPOINT

CHECKPOINT = "Lunar_model_small.pt"


def find_local_peaks_tiled(elevation, size=15, top_k=2000, tile_size=4096):
    """
    Gaseste varfuri locale folosind scipy pe blocuri (tiles) pentru a preveni
    blocarea memoriei RAM/VRAM si a rula eficient.
    """
    H, W = elevation.shape
    all_peak_rows = []
    all_peak_cols = []
    all_peak_vals = []

    overlap = size
    for r in range(0, H, tile_size - overlap):
        r_end = min(r + tile_size, H)
        r_start_valid = 0 if r == 0 else overlap // 2
        r_end_valid = (r_end - r) if r_end == H else (r_end - r - overlap // 2)

        for c in range(0, W, tile_size - overlap):
            c_end = min(c + tile_size, W)
            c_start_valid = 0 if c == 0 else overlap // 2
            c_end_valid = (c_end - c) if c_end == W else (c_end - c - overlap // 2)

            tile = elevation[r:r_end, c:c_end]
            local_max = maximum_filter(tile, size=size) == tile

            valid_mask = np.zeros_like(tile, dtype=bool)
            valid_mask[r_start_valid:r_end_valid, c_start_valid:c_end_valid] = True
            local_max = np.logical_and(local_max, valid_mask)

            p_Rows, p_Cols = np.where(local_max)
            p_Vals = tile[p_Rows, p_Cols]

            all_peak_rows.append(p_Rows + r)
            all_peak_cols.append(p_Cols + c)
            all_peak_vals.append(p_Vals)

    peak_rows = np.concatenate(all_peak_rows)
    peak_cols = np.concatenate(all_peak_cols)
    peak_vals = np.concatenate(all_peak_vals)

    order = np.argsort(peak_vals)[::-1][:top_k]
    return peak_rows[order], peak_cols[order], peak_vals[order]


def eval_on_random_peaks(model, sampler, device, num_samples=100):
    print("Se cauta varfurile locale in DEM...")
    rows, cols, true_vals = find_local_peaks_tiled(sampler.elevation, size=15, top_k=3000)

    # Selectam random 100 de puncte din totalul varfurilor gasite
    indices = np.random.choice(len(true_vals), size=min(num_samples, len(true_vals)), replace=False)

    rows = rows[indices]
    cols = cols[indices]
    true_vals = true_vals[indices]

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

    # Evaluare pe GPU (folosind Tensor pentru active_frac, cerut de TorchScript)
    model.eval()
    with torch.no_grad():
        active_frac_tensor = torch.tensor(1.0, dtype=coords.dtype, device=device)
        pred_norm = model(coords, active_frac_tensor)

    pred = pred_norm.cpu().numpy().flatten() * sampler.alt_std + sampler.alt_mean
    true_vals = true_vals.flatten()
    err = pred - true_vals

    # Afisare vizuala detaliata pentru cele 100 de puncte
    print("\n" + "=" * 80)
    print(f"{'Nr.':<5} | {'Lon (°)':<10} | {'Lat (°)':<10} | {'Real (m)':<10} | {'Model (m)':<10} | {'Eroare (m)':<10}")
    print("=" * 80)

    for i in range(len(true_vals)):
        print(
            f"{i + 1:<5} | {lon[i]:<10.2f} | {lat[i]:<10.2f} | {true_vals[i]:<10.1f} | {pred[i]:<10.1f} | {err[i]:+10.1f}")

    print("=" * 80)
    print(f"Statistici pe cele {len(true_vals)} puncte random:")
    print(f"Eroare medie (bias): {err.mean():.2f} m")
    print(f"RMSE: {np.sqrt((err ** 2).mean()):.2f} m")
    print(f"Eroare maxima in abs: {np.abs(err).max():.2f} m")
    return err


def main():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device utilizat: {device}")

    # Solutia 1: Setam explicit weights_only=False pentru a permite incarcarea
    try:
        ckpt = torch.load(CHECKPOINT, map_location=device, weights_only=False)
        sampler = LunarDEMSampler(DEM_PATH)
        sampler.alt_mean = ckpt["alt_mean"]
        sampler.alt_std = ckpt["alt_std"]

        model = LunarAltitudeNet().to(device)
        model.load_state_dict(ckpt["model_state"])
    except Exception as e:
        # Solutia 2: Daca fisierul este TorchScript, il fortam pe CPU pentru a evita erorile de device mix
        print("Incarcare ca TorchScript model (pe CPU)...")
        model = torch.jit.load(CHECKPOINT, map_location="cpu")
        sampler = LunarDEMSampler(DEM_PATH)
        sampler.alt_mean = 0.0
        sampler.alt_std = 1.0
        device = "cpu"  # Rulam evaluarea pe CPU pentru compatibilitate totala cu TorchScript

    eval_on_random_peaks(model, sampler, device)


if __name__ == "__main__":
    main()