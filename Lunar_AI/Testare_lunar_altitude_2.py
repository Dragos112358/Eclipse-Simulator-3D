"""
Diagnostic: testeaza mai multe valori FINEST_RESOLUTION pe greutatile din
model2.pt (fara antrenare), ca sa gasesti cu ce config a fost produs
checkpoint-ul care dadea val RMSE ~50m.
"""

import numpy as np
import torch
import torch.nn as nn

from Antrenare_model import DEM_PATH, LunarDEMSampler, N_LEVELS, N_FEATURES, LOG2_HASHMAP_SIZE, BASE_RESOLUTION
from Antrenare_model import MultiResHashEncoding, LunarAltitudeNet

CHECKPOINT = "lunar_altitude_model2.pt"
CANDIDATES = [2048, 4096, 8192, 16384, 29338, 32768, 45000]
N_EVAL_SAMPLES = 200_000
BATCH = 16384

def build_model_with_finest(finest_resolution, device):
    model = LunarAltitudeNet().to(device)
    model.encoding = MultiResHashEncoding(
        n_levels=N_LEVELS, n_features=N_FEATURES,
        log2_hashmap_size=LOG2_HASHMAP_SIZE, base_resolution=BASE_RESOLUTION,
        finest_resolution=finest_resolution,
    ).to(device)
    return model

def main():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    ckpt = torch.load(CHECKPOINT, map_location=device)

    sampler = LunarDEMSampler(DEM_PATH)
    sampler.alt_mean = ckpt["alt_mean"]
    sampler.alt_std = ckpt["alt_std"]

    # aceleasi puncte de test pentru toti candidatii, ca sa fie comparabil
    torch.manual_seed(0)
    np.random.seed(0)
    coords, alt = sampler.sample_batch(N_EVAL_SAMPLES, device=device)

    loss_fn = nn.MSELoss()
    print(f"{'FINEST_RESOLUTION':>18} | {'val RMSE (m)':>12}")
    for fr in CANDIDATES:
        model = build_model_with_finest(fr, device)
        try:
            model.load_state_dict(ckpt["model_state"])
        except RuntimeError as e:
            print(f"{fr:>18} | shape mismatch: {e}")
            continue
        model.eval()

        total_loss, total_n = 0.0, 0
        with torch.no_grad():
            for i in range(0, N_EVAL_SAMPLES, BATCH):
                c = coords[i:i+BATCH]
                a = alt[i:i+BATCH]
                pred = model(c, active_frac=1.0)
                total_loss += loss_fn(pred, a).item() * len(a)
                total_n += len(a)
        rmse_m = (total_loss / total_n) ** 0.5 * sampler.alt_std
        print(f"{fr:>18} | {rmse_m:>12.2f}")

if __name__ == "__main__":
    main()