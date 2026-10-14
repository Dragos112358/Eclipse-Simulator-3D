"""
Continuare antrenare din checkpoint existent, pentru inca N epoci.
Reia LunarAltitudeNet de la best_val_rmse=50m si continua fine-tuning.
"""

import copy
import sys

import numpy as np
import torch
import torch.nn as nn
from tqdm import tqdm

from Antrenare_model import (
    DEM_PATH, LunarDEMSampler, LunarAltitudeNet, BATCH_SIZE, GRAD_CLIP_NORM
)

CHECKPOINT_IN = "lunar_altitude_model3.pt"
CHECKPOINT_OUT = "lunar_altitude_model4.pt"

EXTRA_EPOCHS = 200
SAMPLES_PER_EPOCH = 2_000_000
PATIENCE_EPOCHS = 40

LR_RESUME = 0.5e-5   # redus de la 1e-3 -> evita soc pe embeddings deja converse
WEIGHT_DECAY_MLP = 1e-6
WEIGHT_DECAY_EMB = 1e-7


def resume_train():
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Device: {device}")

    ckpt = torch.load(CHECKPOINT_IN, map_location=device)

    sampler = LunarDEMSampler(DEM_PATH)
    sampler.alt_mean = ckpt["alt_mean"]
    sampler.alt_std = ckpt["alt_std"]

    model = LunarAltitudeNet().to(device)
    model.load_state_dict(ckpt["model_state"])

    # --- sanity check: verifica RMSE-ul REAL inainte de orice pas de optimizare ---
    model.eval()
    with torch.no_grad():
        sanity_coords, sanity_alt = sampler.sample_batch(BATCH_SIZE * 10, device=device)
        sanity_pred = model(sanity_coords, active_frac=1.0)
        sanity_rmse = (nn.functional.mse_loss(sanity_pred, sanity_alt).item() ** 0.5) * sampler.alt_std
    print(f"Sanity check inainte de orice pas de optimizare: RMSE={sanity_rmse:.2f} m "
          f"(checkpoint zice best_val_rmse={ckpt.get('best_val_rmse_m'):.2f} m)")

    if abs(sanity_rmse - ckpt.get("best_val_rmse_m", sanity_rmse)) > 15:
        raise RuntimeError(
            "Mismatch mare intre RMSE masurat acum si cel din checkpoint -> "
            "verifica FINEST_RESOLUTION/BASE_RESOLUTION/N_LEVELS fata de antrenarea originala."
        )
    best_val_rmse = sanity_rmse  # folosim valoarea reala, nu cea (posibil stale) din checkpoint
    # --- sfarsit sanity check ---

    model.train()

    emb_params = list(model.encoding.embeddings.parameters())
    mlp_params = list(model.mlp.parameters())
    optimizer = torch.optim.Adam([
        {"params": emb_params, "weight_decay": WEIGHT_DECAY_EMB},
        {"params": mlp_params, "weight_decay": WEIGHT_DECAY_MLP},
    ], lr=LR_RESUME, betas=(0.9, 0.99), eps=1e-15)

    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EXTRA_EPOCHS)
    loss_fn = nn.MSELoss()
    scaler = torch.amp.GradScaler("cuda" if device == "cuda" else "cpu")

    n_train_batches = SAMPLES_PER_EPOCH // BATCH_SIZE
    n_val_batches = max(1, n_train_batches // 10)

    best_state = copy.deepcopy(model.state_dict())
    epochs_no_improve = 0

    for epoch in range(EXTRA_EPOCHS):
        model.train()
        total_loss = 0.0
        total_n = 0
        pbar = tqdm(range(n_train_batches), desc=f"Resume epoch {epoch + 1}/{EXTRA_EPOCHS}",
                    dynamic_ncols=True, file=sys.stdout)
        for _ in pbar:
            coords, alt = sampler.sample_batch(BATCH_SIZE, device=device)
            optimizer.zero_grad()

            with torch.amp.autocast("cuda" if device == "cuda" else "cpu"):
                pred = model(coords, active_frac=1.0)
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
                    pred = model(coords, active_frac=1.0)
                    val_loss += loss_fn(pred, alt).item() * len(alt)
                val_n += len(alt)

        train_rmse_m = (total_loss / total_n) ** 0.5 * sampler.alt_std
        val_rmse_m = (val_loss / val_n) ** 0.5 * sampler.alt_std
        print(f"Epoch {epoch + 1}: train RMSE={train_rmse_m:.2f} m | "
              f"val RMSE={val_rmse_m:.2f} m | lr={scheduler.get_last_lr()[0]:.2e}")

        if val_rmse_m < best_val_rmse - 1e-4:
            best_val_rmse = val_rmse_m
            best_state = copy.deepcopy(model.state_dict())
            epochs_no_improve = 0
        else:
            epochs_no_improve += 1
            if epochs_no_improve >= PATIENCE_EPOCHS:
                print(f"[early stopping] fara imbunatatire de {PATIENCE_EPOCHS} epoci "
                      f"(best val RMSE={best_val_rmse:.2f} m).")
                break

    model.load_state_dict(best_state)
    torch.save({
        "model_state": model.state_dict(),
        "alt_mean": sampler.alt_mean,
        "alt_std": sampler.alt_std,
        "best_val_rmse_m": best_val_rmse,
    }, CHECKPOINT_OUT)
    print(f"Model salvat in {CHECKPOINT_OUT} (best val RMSE={best_val_rmse:.2f} m)")


if __name__ == "__main__":
    resume_train()