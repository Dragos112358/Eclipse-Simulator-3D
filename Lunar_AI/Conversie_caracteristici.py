"""
Versiune rapida de pregatire a DEM-ului lunar.

Simple Cylindrical cu standard_parallel=0 e o scalare liniara pura
(x_metri = R * lon_radiani, y_metri = R * lat_radiani) - nu exista
nicio distorsiune de reeșantionat. In loc sa reproiectam intregul
raster (lent, foloseste zeci de GB RAM), doar rescriem transformarea
afina din metri in grade si copiem pixelii pe blocuri (fara sa
incarcam tot fisierul in memorie deodata).

Instalare:
    pip install rasterio numpy tqdm
"""

import math
import numpy as np
import rasterio
from affine import Affine
from tqdm import tqdm

INPUT_PATH = "baza_date_luna.tif"
OUTPUT_PATH = "lunar_dem.tif"
MOON_RADIUS_M = 1737400

MOON_GEOGRAPHIC_CRS = f"+proj=longlat +a={MOON_RADIUS_M} +b={MOON_RADIUS_M} +no_defs"

# factor de conversie metri -> grade (scalare liniara, aceeasi pe ambele axe)
METERS_TO_DEGREES = 180.0 / (math.pi * MOON_RADIUS_M)

with rasterio.open(INPUT_PATH) as src:
    print("dtype:", src.dtypes)
    print("scale (per banda):", src.scales)
    print("offset (per banda):", src.offsets)
    print("CRS original:", src.crs)
    print("transform original:", src.transform)

    scale = src.scales[0] if src.scales and src.scales[0] else 1.0
    offset = src.offsets[0] if src.offsets and src.offsets[0] else 0.0
    print(f"Aplic: elevatie_m = pixel_DN * {scale} + {offset}")

    # rescriem transformarea: toti coeficientii afini scalati cu acelasi
    # factor liniar metri->grade (fara rotatie, deci b=d=0 raman 0)
    t = src.transform
    new_transform = Affine(
        t.a * METERS_TO_DEGREES, t.b * METERS_TO_DEGREES, t.c * METERS_TO_DEGREES,
        t.d * METERS_TO_DEGREES, t.e * METERS_TO_DEGREES, t.f * METERS_TO_DEGREES,
    )
    print("transform nou (grade):", new_transform)

    dst_profile = src.profile.copy()
    dst_profile.update(
        crs=MOON_GEOGRAPHIC_CRS,
        transform=new_transform,
        dtype="float32",
        nodata=np.nan,
        compress="deflate",   # reduce dimensiunea pe disc, nu afecteaza viteza mult
        predictor=3,          # potrivit pentru date float
        tiled=True,
        blockxsize=512,
        blockysize=512,
    )

    windows = list(src.block_windows(1))
    with rasterio.open(OUTPUT_PATH, "w", **dst_profile) as dst:
        for _, window in tqdm(windows, desc="Procesare blocuri"):
            block = src.read(1, window=window).astype(np.float32)
            block = block * scale + offset
            dst.write(block, 1, window=window)

print(f"\nGata: {OUTPUT_PATH} — grade (lat/lon), valori in metri reali.")
print("Verificare finala:")
with rasterio.open(OUTPUT_PATH) as check:
    print("  dtype:", check.dtypes)
    print("  bounds (ar trebui ~(-180,-90,180,90)):", check.bounds)
    sample = check.read(1, window=rasterio.windows.Window(0, 0, 4096, 4096))
    valid = sample[np.isfinite(sample)]
    if valid.size:
        print(f"  interval elevatie (esantion): [{valid.min():.1f}, {valid.max():.1f}] m")