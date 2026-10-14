"""
Exporta modelul LunarAltitudeNet cu quantizare dinamica INT8,
pastrand normalizarea (alt_mean/alt_std) intr-un fisier meta separat.
"""

import copy

import torch
import torch.nn as nn
from torch.ao.quantization import quantize_dynamic
from torch.ao.quantization.qconfig import float_qparams_weight_only_qconfig, default_dynamic_qconfig

from Antrenare_model import LunarAltitudeNet

CHECKPOINT_IN = "Best_lunar_model.pt"
OUTPUT_TORCHSCRIPT = "Lunar_model_int8.pt"
OUTPUT_META = "Lunar_model_small_meta.pt"


def export_int8():
    device = "cpu"  # quantizare dinamica functioneaza doar pe CPU (fbgemm/qnnpack)

    ckpt = torch.load(CHECKPOINT_IN, map_location=device, weights_only=False)

    model_fp32 = LunarAltitudeNet().to(device)
    model_fp32.load_state_dict(ckpt["model_state"])
    model_fp32.eval()

    # CRITIC: nn.Linear si nn.Embedding folosesc scheme de quantizare diferite.
    # Fara sa specifici qconfig separat pentru Embedding, quantize_dynamic
    # ignora tacit embeddings-urile (partea cea mai mare din model, aici) si
    # cuantizeaza doar MLP-ul -> mărimea fisierului aproape nu s-ar schimba.
    qconfig_spec = {
        nn.Linear: default_dynamic_qconfig,
        nn.Embedding: float_qparams_weight_only_qconfig,
    }

    model_int8 = quantize_dynamic(
        copy.deepcopy(model_fp32),
        qconfig_spec=qconfig_spec,
        dtype=torch.quint8,
    )
    model_int8.eval()

    example_coords = torch.randn(4, 3, dtype=torch.float32, device=device)
    example_active_frac = torch.tensor(1.0, dtype=torch.float32, device=device)

    with torch.no_grad():
        traced = torch.jit.trace(model_int8, (example_coords, example_active_frac))

    traced.save(OUTPUT_TORCHSCRIPT)

    torch.save({
        "alt_mean": ckpt["alt_mean"],
        "alt_std": ckpt["alt_std"],
    }, OUTPUT_META)

    import os
    size_fp32 = os.path.getsize(CHECKPOINT_IN) / (1024 * 1024)
    size_int8 = os.path.getsize(OUTPUT_TORCHSCRIPT) / (1024 * 1024)

    print(f"Model INT8 salvat in {OUTPUT_TORCHSCRIPT}")
    print(f"Metadate normalizare salvate in {OUTPUT_META}")
    print(f"alt_mean={ckpt['alt_mean']:.4f}, alt_std={ckpt['alt_std']:.4f}")
    print(f"Marime FP32 originala: {size_fp32:.1f} MB")
    print(f"Marime INT8: {size_int8:.1f} MB (raport: {size_fp32 / size_int8:.2f}x)")

    # Sanity check: FP32 original (neatins) vs INT8 exportat
    with torch.no_grad():
        test_coords = torch.randn(1000, 3, dtype=torch.float32, device=device)
        test_coords = test_coords / test_coords.norm(dim=1, keepdim=True)

        pred_fp32 = model_fp32(test_coords, 1.0)
        pred_int8 = traced(test_coords, torch.tensor(1.0, dtype=torch.float32, device=device))

        diff = (pred_fp32 - pred_int8).abs()
        print(f"\nSanity check FP32 vs INT8 (spatiu normalizat):")
        print(f"  diferenta medie: {diff.mean().item():.6f}")
        print(f"  diferenta maxima: {diff.max().item():.6f}")
        print(f"  in metri, aprox: medie={diff.mean().item() * ckpt['alt_std']:.2f}m, "
              f"maxima={diff.max().item() * ckpt['alt_std']:.2f}m")


if __name__ == "__main__":
    export_int8()