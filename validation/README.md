# Validation

This folder contains the material used to check the contact times and the local eclipse circumstances computed by 3D Eclipse Simulator against an independent reference, and to cross-check the eclipse catalogue.

## Summary

| Eclipse | Location | Regime | max \|Δ\| on C1–C4 |
|---|---|---|---|
| 11 Aug 1999 (total) | Bucharest | core | 1.7 s |
| 12 Aug 2026 (total) | Zaragoza | core | 1.0 s |
| 2 Aug 2027 (total) | Luxor | core | 1.3 s |
| 26 Dec 2038 (total) | Foxton, NZ | far_future | 22.8 s (see [Known discrepancy](#known-discrepancy-2038)) |

Over the three eclipses in the near-present regime (12 contacts): **max \|Δ\| = 1.7 s, mean \|Δ\| = 0.95 s, RMS = 1.03 s.**
The 2038 case (4 contacts: max \|Δ\| = 22.8 s, mean Δ = +21.5 s) is reported separately because its difference is dominated by the predicted value of ΔT, not by the geometry.

All four eclipses are **total**. No annular or partial-only location has been validated yet.

## What is compared

- **Reference:** Xavier Jubier's interactive eclipse maps (local-circumstances pop-up of each map).
- **Quantity:** geometric contact times C1–C4 in UTC (Jubier's table lists them without limb correction; his limb correction is a separate `LC` column), at the same observer coordinates and elevation.
- **Derived quantities:** totality duration (C3 − C2), full eclipse duration (C4 − C1) and the shift of the centre of totality (mean of C2 and C3), computed from the contact times.
- **Secondary quantities:** magnitude, Moon/Sun size ratio, path width, totality duration, umbral velocity and umbral depth.
- **Reference coverage:** Jubier's maps cover only a limited range of years around the present. Outside it there is **no independent reference**.
- **Resolution:** both sides report times to 0.1 s, so differences are quantised to 0.1 s.

## Reference settings

| Eclipse | Observer (lat, lon) | Altitude (m, simulator = Jubier) | ΔT Jubier | ΔT simulator | Δ ΔT (sim − Jubier) | Reference read on |
|---|---|---|---|---|---|---|
| 1999-08-11 | 44.43522° N, 26.04814° E | 82 | 63.7 s | 63.68 s | −0.02 s | 2026-02-05 |
| 2026-08-12 | 41.64714° N, 0.88565° W | 227 | 69.1 s | 69.10 s | 0.00 s | 2026-03-05 |
| 2027-08-02 | 25.63483° N, 32.59431° E | 77 | 69.3 s | 69.08 s | −0.22 s | 2026-03-05 |
| 2038-12-26 | 40.47225° S, 175.23682° E | 3 | 83.9 s | 69.61 s | −14.29 s | 2026-04-05 |

The simulator values below were collected with `collect_simulator.py` directly from the running backend, at Jubier's coordinates and elevation; the ΔT column is the value the backend actually used.

For the three near-present eclipses the ΔT difference is at most 0.22 s, so it cannot account for the ~1 s contact offsets (and a ΔT difference would in any case shift all four contacts in the same direction). Only 2038 differs significantly (−14.3 s), which is why it is reported separately.

## Per-contact comparison

Times in UTC. Δ = this project − Jubier, in seconds.

### 11 August 1999 — Bucharest

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 09:41:18.6 | 09:41:20.3 | −1.7 |
| C2 | 11:05:43.7 | 11:05:43.3 | +0.4 |
| C3 | 11:08:03.9 | 11:08:05.4 | −1.5 |
| C4 | 12:28:40.6 | 12:28:40.1 | +0.5 |

### 12 August 2026 — Zaragoza

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 17:34:42.2 | 17:34:43.2 | −1.0 |
| C2 | 18:29:04.8 | 18:29:04.2 | +0.6 |
| C3 | 18:30:27.1 | 18:30:27.9 | −0.8 |
| C4 | 19:21:30.1 | 19:21:29.5 | +0.6 |

C4 occurs after sunset in Zaragoza (Jubier marks it with `*`).

### 2 August 2027 — Luxor Bridge

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 08:40:14.3 | 08:40:15.4 | −1.1 |
| C2 | 10:02:09.9 | 10:02:08.8 | +1.1 |
| C3 | 10:08:25.5 | 10:08:26.3 | −0.8 |
| C4 | 11:26:37.2 | 11:26:35.9 | +1.3 |

## Observed systematic pattern

In all three near-present eclipses the signs are the same: **C1 early, C2 late, C3 early, C4 late**.

| Contact | mean Δ (s) | min | max |
|---|---|---|---|
| C1 | −1.3 | −1.7 | −1.0 |
| C2 | +0.7 | +0.4 | +1.1 |
| C3 | −1.0 | −1.5 | −0.8 |
| C4 | +0.8 | +0.5 | +1.3 |

### Durations and centre of totality (derived from the contacts)

| Eclipse | Totality C3−C2 (this project) | Totality C3−C2 (Jubier) | Δ totality | Δ full eclipse (C4−C1) | Centre-of-totality shift ((ΔC2+ΔC3)/2) |
|---|---|---|---|---|---|
| 1999 | 2m 20.2s | 2m 22.1s | −1.9 s | +2.2 s | −0.55 s |
| 2026 | 1m 22.3s | 1m 23.7s | −1.4 s | +1.6 s | −0.10 s |
| 2027 | 6m 15.6s | 6m 17.5s | −1.9 s | +2.4 s | +0.15 s |
| 2038 | 2m 10.6s | 2m 12.5s | −1.9 s | +0.4 s | +21.85 s |

The simulator's totality is **shorter by 1.4–1.9 s** in every eclipse (including 2038) and its partial phase is longer (full eclipse +1.6 to +2.4 s in the near-present cases). In the near-present cases the centre of totality agrees within about 0.5 s, so the offset is symmetric around mid-eclipse: it changes the duration, not the timing. This is a systematic offset, not noise, and a ΔT difference cannot explain it, because a ΔT difference shifts all four contacts the same way. In 2038 the whole pattern is shifted by ΔT (centre +21.85 s) while the durations stay in line with the other eclipses (the full-eclipse difference is only +0.4 s).

Likely cause (not yet isolated): the lunar radius used for the contacts. The backend solves the external contacts C1/C4 with a larger lunar radius than the internal contacts C2/C3. A radius difference of about 1.35 km corresponds to roughly 1.3 s of contact time at these geometries, the same order as the observed offsets. Re-running the comparison with a single mean lunar radius, or with the limb-corrected C1L–C4L, would confirm or refute this.

## Secondary quantities

| Eclipse | Quantity | Simulator | Jubier | Diff | Rel. diff |
|---|---|---|---|---|---|
| 1999 | Magnitude at maximum | 1.01351 | 1.01314 | +0.00037 | 0.037 % |
| 1999 | Moon/Sun size ratio | 1.02932 | 1.02856 | +0.00076 | 0.074 % |
| 1999 | Path width | 112.4 km | 112.2 km | +0.2 km | 0.18 % |
| 1999 | Totality duration | 2m 20.3s | 2m 22.1s | −1.8 s | 1.27 % |
| 1999 | Umbral velocity | 0.6788 km/s | 0.679 km/s | −0.0002 km/s | 0.029 % |
| 1999 | Umbral depth | 51.7 km | 51.6 km | +0.1 km | 0.19 % |
| 2026 | Magnitude at maximum | 1.00736 | 1.00697 | +0.00039 | 0.039 % |
| 2026 | Moon/Sun size ratio | 1.03314 | 1.03238 | +0.00076 | 0.074 % |
| 2026 | Path width | 291.5 km | 294.0 km | −2.5 km | 0.85 % |
| 2026 | Totality duration | 1m 22.2s | 1m 23.7s | −1.5 s | 1.79 % |
| 2026 | Umbral velocity | 2.8044 km/s | 2.836 km/s | −0.0316 km/s | 1.1 % |
| 2026 | Umbral depth | 62.8 km | 63.3 km | −0.5 km | 0.79 % |
| 2027 | Magnitude at maximum | 1.03316 | 1.03275 | +0.00041 | 0.040 % |
| 2027 | Moon/Sun size ratio | 1.07985 | 1.07904 | +0.00081 | 0.075 % |
| 2027 | Path width | 257.5 km | 257.3 km | +0.2 km | 0.078 % |
| 2027 | Totality duration | 6m 15.7s | 6m 17.6s | −1.9 s | 0.50 % |
| 2027 | Umbral velocity | 0.6664 km/s | 0.667 km/s | −0.0006 km/s | 0.090 % |
| 2027 | Umbral depth | 106.8 km | 106.6 km | +0.2 km | 0.19 % |
| 2038 | Magnitude at maximum | 1.01264 | 1.01217 | +0.00047 | 0.046 % |
| 2038 | Moon/Sun size ratio | 1.02698 | 1.02623 | +0.00075 | 0.073 % |
| 2038 | Path width | 93.2 km | 93.1 km | +0.1 km | 0.107 % |
| 2038 | Totality duration | 2m 10.6s | 2m 12.4s | −1.8 s | 1.36 % |
| 2038 | Umbral velocity | 0.6911 km/s | 0.689 km/s | +0.0021 km/s | 0.30 % |
| 2038 | Umbral depth | 43.6 km | 43.2 km | +0.4 km | 0.93 % |


The simulator magnitude is about 0.04 % and the Moon/Sun size ratio about 0.075 % larger than Jubier's in every case, which comes from the lunar radius used for the displayed values. The 2026 eclipse is the largest outlier for width and speed because the Sun is at about 6° altitude at maximum: the shadow footprint changes quickly with the incidence angle there, and atmospheric refraction is not modelled in the width.

## Known discrepancy (2038)

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 23:56:27.9 | 23:56:06.9 (25 Dec) | +21.0 |
| C2 | 01:21:54.8 | 01:21:32.0 | +22.8 |
| C3 | 01:24:05.4 | 01:23:44.5 | +20.9 |
| C4 | 02:45:39.0 | 02:45:17.6 | +21.4 |

Mean Δ = +21.5 s. The two ΔT values differ by about 14.3 s:

| | ΔT for 2038 | Nature of the value |
|---|---|---|
| Jubier | 83.9 s | Agrees with the Espenak & Meeus 2005–2050 polynomial (≈ 84.0 s), which NASA states is derived from estimated values for 2010 and 2050, not fitted to observations. For 2026–2027 the same polynomial gives ≈ 75–76 s while Jubier uses 69.1–69.3 s, so it already overshoots the present. |
| This project | 69.61 s | The backend calls `load.timescale()`, i.e. Skyfield's built-in ΔT table. Past the end of that table Skyfield continues from the last tabulated value with the slope of the last year of data. That slope is near zero (backend values: 69.10 s in 2026, 69.08 s in 2027, 69.61 s in 2038), so ΔT barely moves over 11 years. This is a persistence forecast: "the recent trend continues". |

Both are extrapolations. A rotation-rate forecast 12 years ahead has no demonstrated accuracy, so this case measures how sensitive the contact times are to ΔT, not the error of either program. A ΔT difference of this size moves the path by a few kilometres and changes C2/C3 and the totality duration more than C1/C4. Which value is closer cannot be decided until the eclipse happens; the true value is plausibly between the two. A sensitivity run with ΔT forced to each value would separate the ΔT effect from the geometry.

## Limb correction

Jubier lists the lunar-limb correction separately (`LC` column), for C2 and C3 only:

| Eclipse | LC at C2 | LC at C3 |
|---|---|---|
| 1999 | −1.8 s | −0.3 s |
| 2026 | +0.0 s | −1.5 s |
| 2027 | −2.1 s | −3.0 s |
| 2038 | −2.4 s | −2.1 s |

The simulator's limb-corrected contacts (C1L–C4L, from the neural elevation model) are not part of the tables above. To compare them, add `C1L`–`C4L` columns to `jubier_reference.csv` (Jubier's geometric times corrected by the `LC` column) and run `python run_validation.py --use-limb`.

## Catalogue cross-check

A standalone Besselian-elements script run over the 71,264-event catalogue found 719 eclipses whose umbra/antumbra touched Romania in 30,000 years (plus ~13,000 partial-only) and reproduced the documented track of the 11 Aug 1999 eclipse.

## Files

| File | Purpose |
|---|---|
| `cases.csv` | Eclipses, observer coordinates, elevations and regime (`core` / `far_future`) to evaluate |
| `collect_simulator.py` | Queries the running backend over UDP (same protocol as Unreal) and saves `simulator_results.json`, including the ΔT the backend used |
| `jubier_reference.csv` | Reference values read from Jubier's maps (contacts, ΔT, altitude, secondary quantities) |
| `run_validation.py` | Merges `simulator_results.json`, `cases.csv` and `jubier_reference.csv`, computes Δ, statistics and the Markdown tables |
| `simulator_results.json` | Output of the last backend run, used to produce the tables above |
| `simulator_from_thesis_figures.json` | Simulator values originally read from the report screenshots, kept for reference |

## How to reproduce

Close Unreal first (it uses the same UDP port), then:

    pip install -r ../Server/requirements.txt
    python collect_simulator.py --server-dir ../Content/Encoder
    python run_validation.py --out summary.md

To regenerate the tables without running the backend, use the saved results:

    python run_validation.py --sim simulator_results.json --out summary.md