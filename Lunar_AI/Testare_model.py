import numpy as np
import rasterio
import matplotlib.pyplot as plt
from Antrenare_model import predict, DEM_PATH, LunarDEMSampler


def test_single_points(n_points=10):
    """1. Compară predicția modelului cu valoarea reală din DEM pe puncte aleatoare."""
    print("=== Test 1: Puncte aleatoare (Model vs Ground Truth) ===")

    sampler = LunarDEMSampler(DEM_PATH)
    # Generăm coordonate aleatoare
    lons, lats = sampler._random_lonlat(n_points)

    # Valorile reale din DEM prin interpolare biliniară
    true_alts = sampler._sample_altitude_bilinear(lons, lats)

    # Predicțiile modelului nostru AI
    pred_alts = predict(lats, lons)

    errors = np.abs(pred_alts - true_alts)

    print(f"{'Lat':<10} | {'Lon':<10} | {'Real (m)':<12} | {'AI (m)':<12} | {'Eroare (m)':<10}")
    print("-" * 65)
    for lat, lon, true_a, pred_a, err in zip(lats, lons, true_alts, pred_alts, errors):
        print(f"{lat:10.2f} | {lon:10.2f} | {true_a:12.1f} | {pred_a:12.1f} | {err:10.2f}")

    print(f"\nEroare medie absolută (MAE) pe acest eșantion: {errors.mean():.2f} m")
    print(f"Eroare maximă: {errors.max():.2f} m\n")


def test_elevation_profile():
    """2. Generează un profil de altitudine de-a lungul unui meridian sau paralel
    (de exemplu, un "taif" sau secțiune prin relieful lunar) și îl plotează."""
    print("=== Test 2: Generare profil topografic ===")

    # Definim o linie de la latitudinea -45 la +45 pe longitudinea 30 (de exemplu)
    lats_line = np.linspace(-45.0, 45.0, 500)
    lons_line = np.full_like(lats_line, 30.0)

    sampler = LunarDEMSampler(DEM_PATH)
    true_profile = sampler._sample_altitude_bilinear(lons_line, lats_line)
    ai_profile = predict(lats_line, lons_line)

    # Plotare cu Matplotlib
    plt.figure(figsize=(12, 5))
    plt.plot(lats_line, true_profile, label="DEM Real (Ground Truth)", color="black", alpha=0.7, linewidth=2)
    plt.plot(lats_line, ai_profile, label="Model AI (Hash Grid)", color="red", linestyle="--", linewidth=1.5)
    plt.title("Profil Topografic Lunar (Lon: 30°E, Lat: -45° la +45°)")
    plt.xlabel("Latitudine (grade)")
    plt.ylabel("Altitudine (metri)")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()


def evaluate_global_rmse(n_samples=100_000):
    """3. Calculează RMSE-ul general pe un set mare de test independent."""
    print("=== Test 3: Evaluare statistică globală (RMSE) ===")
    sampler = LunarDEMSampler(DEM_PATH)

    lons, lats = sampler._random_lonlat(n_samples)
    true_alts = sampler._sample_altitude_bilinear(lons, lats)

    valid = np.isfinite(true_alts)
    true_alts = true_alts[valid]
    lats, lons = lats[valid], lons[valid]

    pred_alts = predict(lats, lons)

    rmse = np.sqrt(np.mean((pred_alts - true_alts) ** 2))
    print(f"RMSE calculat pe {n_samples} puncte noi: {rmse:.2f} metri")


if __name__ == "__main__":
    test_single_points(10)
    evaluate_global_rmse(50_000)
    test_elevation_profile()  # Deschide un grafic cu profilul lunar