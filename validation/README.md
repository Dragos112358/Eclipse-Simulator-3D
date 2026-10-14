# Validation

This folder contains the material used to check the contact times and the local eclipse circumstances computed by 3D Eclipse Simulator against an independent reference, and to cross-check the eclipse catalogue.

## Summary

| Eclipse | Location | max \|Δ\| on C1–C4 |
|---|---|---|
| 11 Aug 1999 (total) | Bucharest | 1.7 s |
| 12 Aug 2026 (total) | Zaragoza | 1.0 s |
| 2 Aug 2027 (total) | Luxor | 1.4 s |
| 26 Dec 2038 (total) | Foxton, NZ | 23.0 s (see [Known discrepancy](#known-discrepancy-2038)) |

Over the three eclipses in the near-present regime (12 contacts): **max \|Δ\| = 1.7 s, mean \|Δ\| = 0.96 s, RMS = 1.03 s.**
The 2038 case is reported separately because its difference is dominated by the predicted value of ΔT, not by the geometry.

All four eclipses are **total**. No annular or partial-only location has been validated yet.

## What is compared

- **Reference:** Xavier Jubier's interactive eclipse maps (local-circumstances pop-up of each map).
- **Quantity:** geometric contact times C1–C4 in UTC (Jubier's table lists them without limb correction; his limb correction is a separate `LC` column), at the same observer coordinates.
- **Reference coverage:** Jubier's maps cover only a limited range of years around the present. Outside it there is **no independent reference**.
- **Resolution:** both sides report times to 0.1 s, so differences are quantised to 0.1 s.

## Reference settings

| Eclipse | Observer (lat, lon) | Alt. Jubier | Alt. simulator | ΔT Jubier | Reference read on |
|---|---|---|---|---|---|
| 1999-08-11 | 44.43522° N, 26.04814° E | 82.0 m | not recorded | 63.7 s | not recorded |
| 2026-08-12 | 41.64714° N, 0.88565° W | 227.0 m | 310 m (drone above buildings) | 69.1 s | 2026-03-10 |
| 2027-08-02 | 25.63483° N, 32.59431° E | 77.0 m | not recorded | 69.3 s | 2026-03-10 |
| 2038-12-26 | 40.47225° S, 175.23682° E | 3.0 m | not recorded | 83.9 s | not recorded |

Observer coordinates are Jubier's. The values in the simulator screenshots differ from them by 0.0002–0.0004° (about 20–40 m), which is below the other error sources here.

The simulator values in the tables below were read from the application's report screenshots. `collect_simulator.py` regenerates them directly from the running backend, at Jubier's coordinates and elevation, and also records the ΔT the backend actually used.

## Per-contact comparison

Times in UTC. Δ = this project − Jubier, in seconds.

### 11 August 1999 — Bucharest

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 09:41:18.6 | 09:41:20.3 | −1.7 |
| C2 | 11:05:43.7 | 11:05:43.3 | +0.4 |
| C3 | 11:08:04.0 | 11:08:05.4 | −1.4 |
| C4 | 12:28:40.7 | 12:28:40.1 | +0.6 |

### 12 August 2026 — Zaragoza

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 17:34:42.2 | 17:34:43.2 | −1.0 |
| C2 | 18:29:04.8 | 18:29:04.2 | +0.6 |
| C3 | 18:30:27.1 | 18:30:27.9 | −0.8 |
| C4 | 19:21:30.1 | 19:21:29.5 | +0.6 |

C4 occurs after sunset in Zaragoza (Jubier marks it with `*`). The elevation differs (227 m vs 310 m).

### 2 August 2027 — Luxor Bridge

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 08:40:14.2 | 08:40:15.4 | −1.2 |
| C2 | 10:02:09.9 | 10:02:08.8 | +1.1 |
| C3 | 10:08:25.6 | 10:08:26.3 | −0.7 |
| C4 | 11:26:37.3 | 11:26:35.9 | +1.4 |

## Observed systematic pattern

In all three near-present eclipses the signs are the same: **C1 early, C2 late, C3 early, C4 late**.

| Contact | mean Δ (s) | min | max |
|---|---|---|---|
| C1 | −1.3 | −1.7 | −1.0 |
| C2 | +0.7 | +0.4 | +1.1 |
| C3 | −1.0 | −1.4 | −0.7 |
| C4 | +0.9 | +0.6 | +1.4 |

Equivalently, the simulator's totality is **shorter by 1.4–1.9 s** in every eclipse (including 2038) and its partial phase is longer. This is a systematic offset, not noise, and a ΔT difference cannot explain it, because a ΔT difference shifts all four contacts the same way.

Likely cause (not yet isolated): the lunar radius used for the contacts. The backend solves the external contacts C1/C4 with a larger lunar radius than the internal contacts C2/C3. A radius difference of about 1.35 km corresponds to roughly 1.3 s of contact time at these geometries, the same order as the observed offsets. Re-running the comparison with a single mean lunar radius, or with the limb-corrected C1L–C4L, would confirm or refute this.

## Secondary quantities

| Eclipse | Quantity | Simulator | Jubier | Rel. diff |
|---|---|---|---|---|
| 1999 | Magnitude at maximum | 1.01352 | 1.01314 | 0.038 % |
| 1999 | Moon/Sun size ratio | 1.02932 | 1.02856 | 0.074 % |
| 1999 | Totality duration | 2m 20.3s | 2m 22.1s | −1.8 s |
| 1999 | Path width | 112.4 km | 112.2 km | 0.18 % |
| 1999 | Umbral velocity | 0.6788 km/s | 0.679 km/s | 0.029 % |
| 1999 | Umbral depth | 51.7 km | 51.6 km | 0.19 % |
| 2026 | Magnitude at maximum | 1.00733 | 1.00697 | 0.036 % |
| 2026 | Moon/Sun size ratio | 1.03314 | 1.03238 | 0.074 % |
| 2026 | Totality duration | 1m 22.2s | 1m 23.7s | −1.5 s |
| 2026 | Path width | 291.6 km | 294.0 km | 0.82 % (2.4 km) |
| 2026 | Umbral velocity | 2.8044 km/s | 2.836 km/s | 1.1 % |
| 2026 | Umbral depth | 62.8 km | 63.3 km | 0.79 % |
| 2027 | Magnitude at maximum | 1.03318 | 1.03275 | 0.042 % |
| 2027 | Moon/Sun size ratio | 1.07985 | 1.07904 | 0.075 % |
| 2027 | Totality duration | 6m 15.7s | 6m 17.6s | −1.9 s |
| 2027 | Path width | 257.5 km | 257.3 km | 0.078 % |
| 2027 | Umbral velocity | 0.6664 km/s | 0.667 km/s | 0.090 % |
| 2027 | Umbral depth | 106.8 km | 106.6 km | 0.19 % |

The Moon/Sun size ratio is consistently about 0.075 % larger in the simulator, which comes from the lunar radius used for the displayed values. The 2026 eclipse is the largest outlier for width and speed because the Sun is at about 6° altitude at maximum: the shadow footprint changes quickly with the incidence angle there, and atmospheric refraction is not modelled in the width.

## Known discrepancy (2038)

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 23:56:28.0 | 23:56:06.9 (25 Dec) | +21.1 |
| C2 | 01:21:55.0 | 01:21:32.0 | +23.0 |
| C3 | 01:24:05.6 | 01:23:44.5 | +21.1 |
| C4 | 02:45:39.1 | 02:45:17.6 | +21.5 |

Mean Δ = +21.7 s. The two ΔT values differ by about 14.5 s:

| | ΔT for 2038 | Nature of the value |
|---|---|---|
| Jubier | 83.9 s | Agrees with the Espenak & Meeus 2005–2050 polynomial (≈ 84.0 s), which NASA states is derived from estimated values for 2010 and 2050, not fitted to observations. For 2026–2027 the same polynomial gives ≈ 75–76 s while Jubier uses 69.1–69.3 s, so it already overshoots the present. |
| This project | 69.38 s | The backend calls `load.timescale()`, i.e. Skyfield's built-in ΔT table. Past the end of that table Skyfield continues from the last tabulated value with the slope of the last year of data. That slope is near zero (ΔT has been almost flat in recent years: backend values 69.07 s in 2026, 69.03 s in 2027, 69.13 s in 2035), so 2038 comes out at 69.38 s. This is a persistence forecast: "the recent trend continues". |

Both are extrapolations. A rotation-rate forecast 12 years ahead has no demonstrated accuracy, so this case measures how sensitive the contact times are to ΔT, not the error of either program. A ΔT difference of this size moves the path by a few kilometres and changes C2/C3 and the totality duration more than C1/C4. Which value is closer cannot be decided until the eclipse happens; the true value is plausibly between the two. A sensitivity run with ΔT forced to each value would separate the ΔT effect from the geometry.

## Limb correction

Jubier lists the lunar-limb correction separately (`LC` column), for C2 and C3 only:

| Eclipse | LC at C2 | LC at C3 |
|---|---|---|
| 1999 | −1.8 s | −0.3 s |
| 2026 | +0.0 s | −1.5 s |
| 2027 | −2.1 s | −3.0 s |
| 2038 | −2.4 s | −2.1 s |

The simulator's limb-corrected contacts (C1L–C4L, from the neural elevation model) are not part of the table above. Run `python run_validation.py --use-limb` with the Jubier values corrected by the `LC` column to compare them.

## Catalogue cross-check

A standalone Besselian-elements script run over the 71,264-event catalogue found 719 eclipses whose umbra/antumbra touched Romania in 30,000 years (plus ~13,000 partial-only) and reproduced the documented track of the 11 Aug 1999 eclipse.

## Files

| File | Purpose |
|---|---|
| `cases.csv` | Eclipses, observer coordinates and elevations to evaluate |
| `collect_simulator.py` | Queries the running backend over UDP (same protocol as Unreal) and saves `simulator_results.json` |
| `jubier_reference.csv` | Reference values read from Jubier's maps |
| `run_validation.py` | Merges both, computes Δ, statistics and the Markdown tables |
| `compare_contacts.py` | Statistics helpers used by `run_validation.py` |
| `simulator_from_thesis_figures.json` | Simulator values read from the report screenshots, for reference |

## How to reproduce

Close Unreal first (it uses the same UDP port), then:

    pip install -r ../Server/requirements.txt
    python collect_simulator.py --server-dir ../Content/Encoder
    python run_validation.py --out summary.md

Use `python run_validation.py --sim simulator_from_thesis_figures.json` to reproduce the tables above without running the backend.