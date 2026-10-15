# Validation

This folder contains everything used to check **3D Eclipse Simulator** against independent references:

1. **Local circumstances vs. Xavier Jubier's maps** — contact times C1–C4 for four total eclipses (Section 3).
2. **Besselian-element cross-check** — an independent, Skyfield-free re-computation of the local circumstances from the catalogue derived from John Irwin's 2021 database, over 18 cases covering total, annular, hybrid and partial eclipses (Section 5).
3. **Catalogue cross-check** — a track-level sanity check over the whole 71,264-event catalogue (Section 6).

Everything here is reproducible from the scripts in this folder (Section 11).

## Contents

1. [Summary](#1-summary)
2. [Scope: what is and is not validated](#2-scope-what-is-and-is-not-validated)
3. [Comparison with Jubier](#3-comparison-with-jubier)
4. [Known discrepancy: 2038](#4-known-discrepancy-2038)
5. [Besselian cross-check (Irwin 2021 catalogue)](#5-besselian-cross-check-irwin-2021-catalogue)
6. [Catalogue cross-check](#6-catalogue-cross-check)
7. [Limb correction](#7-limb-correction)
8. [Limitations](#8-limitations)
9. [Open items](#9-open-items)
10. [Files](#10-files)
11. [How to reproduce](#11-how-to-reproduce)
12. [Conventions and glossary](#12-conventions-and-glossary)
13. [References](#13-references)

---

## 1. Summary

| Eclipse | Location | Regime | max \|Δ\| on C1–C4 |
|---|---|---|---|
| 11 Aug 1999 (total) | Bucharest | core | 1.7 s |
| 12 Aug 2026 (total) | Zaragoza | core | 1.0 s |
| 2 Aug 2027 (total) | Luxor | core | 1.3 s |
| 26 Dec 2038 (total) | Foxton, NZ | far_future | 22.8 s (see [Section 4](#4-known-discrepancy-2038)) |

Over the three eclipses in the near-present regime (12 contacts): **max \|Δ\| = 1.7 s, mean \|Δ\| = 0.95 s, RMS = 1.03 s.**

The 2038 case (4 contacts: max \|Δ\| = 22.8 s, mean Δ = +21.5 s) is reported separately because its difference is dominated by the *predicted value of ΔT*, not by the geometry.

Key findings:

- **Timing agrees with Jubier to about 1 s** for eclipses with a well-known ΔT.
- **A small systematic pattern exists** (C1 early, C2 late, C3 early, C4 late). It shortens totality by 1.4–1.9 s and leaves the centre of totality unchanged. The likely cause is the lunar radius convention (Section 3.5).
- **The 2038 offset (~21.5 s) is a ΔT effect.** The two programs use different ΔT forecasts; neither can be called "correct" until the eclipse happens.
- **An independent Besselian computation (no Skyfield) agrees on C1, C4 and MAX to within 0.9 s** over all 18 cases and all eclipse types (RMS 0.39 s over 54 values). The internal contacts C2/C3 differ by 1.5–7.8 s, in the same direction as the Jubier offsets (Section 5.5).
- **One reporting bug was found:** for partial (and near-annular partial) eclipses the simulator's obscuration equals 100 × magnitude, which overstates the true obscuration by 6–11 percentage points (Section 5.5.3).

## 2. Scope: what is and is not validated

| Aspect | Reference | Status |
|---|---|---|
| Contact times C1–C4, total eclipses, 1999–2027 | Jubier | Validated (Section 3) |
| Totality duration, full duration, centre of totality | Jubier (derived) | Validated (Section 3.4) |
| Magnitude, size ratio, path width, umbral velocity/depth | Jubier | Validated (Section 3.6) |
| Far-future ΔT behaviour (2038) | Jubier | Compared, **not decidable** (Section 4) |
| Annular, hybrid and partial eclipses | Besselian cross-check | Cross-checked on 18 cases (Section 5.5): contacts, magnitude OK; partial obscuration has a bug |
| Limb-corrected contacts C1L–C4L | None yet | **Not validated** (Section 7) |
| Eclipses outside Jubier's map range | None | **No independent reference exists** |
| Sky objects, constellation, sunrise/sunset | None | Not validated |

All four Jubier comparisons are **total** eclipses. The simulator also produces results for annular, hybrid and partial cases (see `simulator_results.json`), but those have no Jubier reference; they are covered only by the Besselian cross-check.

---

## 3. Comparison with Jubier

### 3.1 What is compared

- **Reference:** Xavier Jubier's interactive eclipse maps (local-circumstances pop-up of each map).
- **Quantity:** geometric contact times C1–C4 in UTC. Jubier's table lists them without limb correction; his limb correction is a separate `LC` column.
- **Same inputs on both sides:** identical observer coordinates and elevation.
- **Derived quantities:** totality duration (C3 − C2), full eclipse duration (C4 − C1) and the shift of the centre of totality (mean of C2 and C3), computed from the contact times.
- **Secondary quantities:** magnitude, Moon/Sun size ratio, path width, totality duration, umbral velocity and umbral depth.
- **Reference coverage:** Jubier's maps cover only a limited range of years around the present. Outside it there is **no independent reference**.
- **Resolution:** both sides report times to 0.1 s, so differences are quantised to 0.1 s.

### 3.2 Reference settings

| Eclipse | Observer (lat, lon) | Altitude (m, simulator = Jubier) | ΔT Jubier | ΔT simulator | Δ ΔT (sim − Jubier) | Reference read on |
|---|---|---|---|---|---|---|
| 1999-08-11 | 44.43522° N, 26.04814° E | 82 | 63.7 s | 63.68 s | −0.02 s | 2026-03-10 |
| 2026-08-12 | 41.64714° N, 0.88565° W | 227 | 69.1 s | 69.10 s | 0.00 s | 2026-03-10 |
| 2027-08-02 | 25.63483° N, 32.59431° E | 77 | 69.3 s | 69.08 s | −0.22 s | 2026-03-10 |
| 2038-12-26 | 40.47225° S, 175.23682° E | 3 | 83.9 s | 69.61 s | −14.29 s | 2026-03-10 |

The simulator values were collected with `collect_simulator.py` directly from the running backend, at Jubier's coordinates and elevation; the ΔT column is the value the backend actually used.

For the three near-present eclipses the ΔT difference is at most 0.22 s, so it cannot account for the ~1 s contact offsets (and a ΔT difference would in any case shift all four contacts in the same direction). Only 2038 differs significantly (−14.3 s), which is why it is reported separately.

### 3.3 Per-contact comparison

Times in UTC. Δ = this project − Jubier, in seconds.

#### 11 August 1999 — Bucharest

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 09:41:18.6 | 09:41:20.3 | −1.7 |
| C2 | 11:05:43.7 | 11:05:43.3 | +0.4 |
| C3 | 11:08:03.9 | 11:08:05.4 | −1.5 |
| C4 | 12:28:40.6 | 12:28:40.1 | +0.5 |

#### 12 August 2026 — Zaragoza

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 17:34:42.2 | 17:34:43.2 | −1.0 |
| C2 | 18:29:04.8 | 18:29:04.2 | +0.6 |
| C3 | 18:30:27.1 | 18:30:27.9 | −0.8 |
| C4 | 19:21:30.1 | 19:21:29.5 | +0.6 |

C4 occurs after sunset in Zaragoza (Jubier marks it with `*`). The Sun is only about 6° high at totality, so this is the most demanding geometry of the set.

#### 2 August 2027 — Luxor Bridge

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | 08:40:14.3 | 08:40:15.4 | −1.1 |
| C2 | 10:02:09.9 | 10:02:08.8 | +1.1 |
| C3 | 10:08:25.5 | 10:08:26.3 | −0.8 |
| C4 | 11:26:37.2 | 11:26:35.9 | +1.3 |

### 3.4 Observed systematic pattern

In all three near-present eclipses the signs are the same: **C1 early, C2 late, C3 early, C4 late**.

| Contact | mean Δ (s) | min | max |
|---|---|---|---|
| C1 | −1.3 | −1.7 | −1.0 |
| C2 | +0.7 | +0.4 | +1.1 |
| C3 | −1.0 | −1.5 | −0.8 |
| C4 | +0.8 | +0.5 | +1.3 |

#### Durations and centre of totality (derived from the contacts)

| Eclipse | Totality C3−C2 (this project) | Totality C3−C2 (Jubier) | Δ totality | Δ full eclipse (C4−C1) | Centre-of-totality shift ((ΔC2+ΔC3)/2) |
|---|---|---|---|---|---|
| 1999 | 2m 20.2s | 2m 22.1s | −1.9 s | +2.2 s | −0.55 s |
| 2026 | 1m 22.3s | 1m 23.7s | −1.4 s | +1.6 s | −0.10 s |
| 2027 | 6m 15.6s | 6m 17.5s | −1.9 s | +2.4 s | +0.15 s |
| 2038 | 2m 10.6s | 2m 12.5s | −1.9 s | +0.4 s | +21.85 s |

The simulator's totality is **shorter by 1.4–1.9 s** in every eclipse (including 2038) and its partial phase is longer (full eclipse +1.6 to +2.4 s in the near-present cases). In the near-present cases the centre of totality agrees within about 0.5 s, so the offset is symmetric around mid-eclipse: it changes the *duration*, not the *timing*.

This is a systematic offset, not noise, and a ΔT difference cannot explain it, because a ΔT difference shifts all four contacts the same way. In 2038 the whole pattern is shifted by ΔT (centre +21.85 s) while the durations stay in line with the other eclipses (the full-eclipse difference is only +0.4 s).

### 3.5 Likely cause (not yet isolated)

The lunar radius used for the contacts. The backend solves the external contacts C1/C4 with a larger lunar radius than the internal contacts C2/C3. A radius difference of about 1.35 km corresponds to roughly 1.3 s of contact time at these geometries, the same order as the observed offsets.

This is consistent with the Besselian convention, where the penumbral and umbral radii (`l1`, `l2`) are built with different lunar-radius constants. Whether Jubier uses a single mean radius for the geometric contacts is not documented in the pop-up, so the hypothesis stays open.

The Besselian cross-check (Section 5.5) sharpens this: against the catalogue, the simulator's **external contacts C1/C4 match to within 0.9 s, while the internal contacts C2/C3 are off by about 2 s each** (totality shorter, annularity longer). So the offset sits in the internal-contact radius, not in the penumbra. For scale, a ~1.4 km radius difference is about the gap between the standard penumbral and umbral lunar-radius constants (0.2725076 vs 0.2722810 Earth radii, about 1.45 km); whether the simulator or the catalogue uses these constants this way should be checked in the backend source.

Re-running the comparison with a single mean lunar radius, or with the limb-corrected C1L–C4L, would confirm or refute it (Section 9).

### 3.6 Secondary quantities

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

How to read this table:

- The simulator magnitude is about **0.04 %** and the Moon/Sun size ratio about **0.075 %** larger than Jubier's in every case. This is a constant bias, which points to the lunar radius used for the displayed values.
- The **2026 eclipse is the largest outlier** for width and speed because the Sun is at about 6° altitude at maximum: the shadow footprint changes quickly with the incidence angle there, and atmospheric refraction is not modelled in the width.
- Totality duration differences (−1.5 to −1.9 s) match the contact-based values in Section 3.4, as they should.

---

## 4. Known discrepancy: 2038

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

**Interpretation.** Both values are extrapolations. A rotation-rate forecast 12 years ahead has no demonstrated accuracy, so this case measures how sensitive the contact times are to ΔT, not the error of either program.

- A ΔT difference of this size moves the path by a few kilometres and changes C2/C3 and the totality duration more than C1/C4.
- The 14.3 s ΔT gap produces a ~21.5 s contact shift, which is the expected order of magnitude: the Moon's shadow crosses the Earth faster than the Earth's surface rotates under it, so the timing error exceeds the ΔT error.
- Which value is closer cannot be decided until the eclipse happens; the true value is plausibly between the two.
- A sensitivity run with ΔT forced to each value would separate the ΔT effect from the geometry (Section 9).

---

## 5. Besselian cross-check (Irwin 2021 catalogue)

### 5.1 Purpose

Jubier covers only four total eclipses in a limited year range. To cover **all eclipse types** and to test the simulator's geometry with code that shares nothing with it, `Compare_simulator_vs_basselian.py` recomputes the local circumstances **from the Besselian elements of the catalogue** (`Eclipse_de_soare.txt`, based on John Irwin's 2021 database), **without Skyfield**.

The method follows Meeus, *Astronomical Algorithms*, chapter 54, and Espenak's Besselian-element formulation.

### 5.2 What the script does

For each case in `simulator_results.json`:

1. Picks the catalogue row whose reference time `t0` is closest to the eclipse (within 0.6 day).
2. Evaluates the polynomials `x, y, d, mu, l1, l2, f1, f2` in time `t` (hours from `t0`, TT).
3. Converts the observer's geodetic position to the geocentric quantities `rho·sin(phi')` and `rho·cos(phi')`, including elevation.
4. Computes the fundamental-plane coordinates `xi, eta, zeta`, then the distance `m` between the shadow axis and the observer and the local radii `L1 = l1 − zeta·f1`, `L2 = l2 − zeta·f2`.
5. Finds maximum eclipse (minimum of `m`) with a 1-minute scan and a ternary search.
6. Finds C1/C4 as the roots of `m − L1` and C2/C3 as the roots of `m − |L2|` by bisection.
7. Computes magnitude `(L1 − m) / (L1 + L2)` and obscuration from the circle-overlap area.
8. Converts TT to UTC with **the same ΔT the simulator used** (read from `simulator_results.json`).
9. Prints the differences (simulator − reference) and writes `besselian_reference.csv`.

Because ΔT is shared, **differences reflect geometry and lunar radii, not ΔT**.

### 5.3 Units in the catalogue (important)

| Quantity | Unit in `Eclipse_de_soare.txt` |
|---|---|
| `x`, `y`, `l1`, `l2` | Earth equatorial radii |
| `d` (declination of the shadow axis) | **radians** |
| `mu` (hour angle of the axis) and its rate | **radians** and radians/hour |
| `t` | hours from `t0` (TT) |

The units were confirmed on the 1999-Aug-11 row: `d0 = 0.2675 rad = 15.3°` and `mu1 = 0.2618 rad/h = 15°/h`.

**An early version of the script treated `d` and `mu` as degrees.** Its output is obviously wrong and must be discarded: magnitudes around 0.5 for total eclipses, no C2/C3 for any case, and a partial phase of about 1h40m instead of about 2h45m for the 1999 eclipse. If you see numbers like that, you are running the old script. The current `Compare_simulator_vs_basselian.py` converts correctly (`mu` via `math.degrees`, `d` used directly in radians).

### 5.3.1 Quick sanity test for any run

A correct run must satisfy all of these for the total-eclipse cases:

- C2 and C3 are present and C3 − C2 is within a few seconds of the simulator's totality duration.
- Magnitude is slightly above 1.0 (for example about 1.013 for Bucharest 1999).
- Obscuration is 100 %.
- C1 is within a few seconds of the simulator's C1, not an hour away.

### 5.4 Cases

18 cases, selected to cover every eclipse type, both hemispheres, low and high Sun, and sea level to 1620 m.

| Type | Cases |
|---|---|
| Total (4) | 1999 Bucharest, 2026 Zaragoza, 2027 Luxor, 2038 Foxton |
| Annular (7) | 2005 Madrid, 2010 Thiruvananthapuram, 2012 Albuquerque, 2019 Singapore, 2020 Kurukshetra, 2023 Corpus Christi, 2030 Istanbul |
| Hybrid (2) | 2005 Panama City, 2023 Exmouth |
| Partial (5) | 2011, 2015, 2022 Bucharest; 2025 Reykjavik; 2029 Vancouver |

Notable stress cases:

- **2020 Kurukshetra**: annularity lasting only 37.6 s, path 21.6 km wide, almost a hybrid; tests numerical sensitivity near the annular/total boundary.
- **2019 Singapore**: umbral depth of only 4.4 km, i.e. an observer almost on the edge of the path.
- **2012 Albuquerque** and **2029 Vancouver**: the eclipse ends after sunset or starts before sunrise; tests the horizon handling.
- **2005 Panama City**: labelled annular in `cases.csv`, but both the simulator and the Besselian script find a partial eclipse there (Section 5.5.4).

### 5.5 Results

Run on 2026-10-15. Δ = simulator − Besselian reference, in seconds (magnitude in absolute units, obscuration in percentage points). `-` means the reference has no such contact (partial at that location). Times are quantised to 0.1 s.

| Case | Type at location | ΔC1 | ΔC2 | ΔMAX | ΔC3 | ΔC4 | Δ mag | Δ obsc. (pp) |
|---|---|---|---|---|---|---|---|---|
| 1999 Bucharest | total | −0.6 | +1.5 | −0.6 | −2.7 | −0.6 | +0.00014 | 0.00 |
| 2026 Zaragoza | total | −0.2 | +1.5 | −0.3 | −1.8 | −0.1 | +0.00014 | 0.00 |
| 2027 Luxor | total | −0.3 | +2.1 | −0.2 | −2.4 | −0.1 | +0.00015 | 0.00 |
| 2038 Foxton | total | +0.3 | +2.4 | 0.0 | −1.8 | +0.4 | +0.00014 | 0.00 |
| 2023 Exmouth | total | 0.0 | +2.4 | −0.5 | −2.5 | 0.0 | +0.00014 | 0.00 |
| 2005 Madrid | annular | +0.6 | −1.3 | +0.9 | +2.4 | +0.5 | +0.00013 | +0.05 |
| 2010 Thiruvananthapuram | annular | −0.1 | −4.2 | −0.7 | +3.8 | −0.3 | +0.00012 | +0.05 |
| 2012 Albuquerque | annular | +0.6 | −0.9 | +0.9 | +2.0 | +0.5 | +0.00013 | +0.05 |
| 2019 Singapore | annular | +0.1 | −7.7 | +0.3 | +7.8 | +0.1 | +0.00013 | +0.05 |
| 2020 Kurukshetra | annular | +0.2 | −2.6 | −0.1 | +3.0 | +0.2 | +0.00014 | +0.06 |
| 2023 Corpus Christi | annular | −0.1 | −2.3 | −0.3 | +2.1 | −0.2 | +0.00013 | +0.05 |
| 2030 Istanbul | annular | −0.2 | −1.9 | −0.7 | +1.5 | −0.2 | +0.00013 | +0.05 |
| 2005 Panama City | partial | +0.5 | - | +0.4 | - | +0.6 | +0.00013 | +0.56 |
| 2011 Bucharest | partial | +0.1 | - | −0.6 | - | +0.1 | +0.00005 | +7.79 |
| 2015 Bucharest | partial | +0.4 | - | −0.3 | - | +0.5 | −0.00004 | +9.89 |
| 2022 Bucharest | partial | 0.0 | - | −0.2 | - | −0.1 | −0.00006 | +11.00 |
| 2025 Reykjavik | partial | −0.2 | - | −0.3 | - | 0.0 | +0.00004 | +5.88 |
| 2029 Vancouver | partial | −0.2 | - | −0.4 | - | −0.2 | +0.00007 | +6.96 |

#### 5.5.1 External contacts and maximum: agreement

Over all 18 cases (54 values of ΔC1, ΔMAX, ΔC4): **max |Δ| = 0.9 s, mean Δ = −0.01 s, RMS = 0.39 s.** No sign pattern, no dependence on eclipse type, hemisphere, Sun altitude or elevation (Albuquerque at 1620 m and Vancouver with the Sun at −6° at C1 are as good as the rest). The contact solver, the observer geometry and the time conversion therefore reproduce the Besselian solution.

#### 5.5.2 Internal contacts C2/C3: a systematic offset

| Group | ΔC2 range (mean) | ΔC3 range (mean) | Effect on the central phase |
|---|---|---|---|
| Total (5 cases) | +1.5 … +2.4 s (+2.0) | −2.7 … −1.8 s (−2.2) | totality **shorter by 3.3–4.9 s** |
| Annular (7 cases) | −7.7 … −0.9 s (−3.0) | +1.5 … +7.8 s (+3.2) | annularity **longer by 2.9–15.5 s** |

Both groups are explained by one mechanism: **the simulator's internal-contact radius is slightly smaller than the catalogue's.** A smaller Moon shortens totality and lengthens annularity, which is exactly the observed sign change between the two groups. The same effect appears as the constant magnitude bias of +0.00012 to +0.00015 in every central case.

The size of the offset depends on how close the observer is to the edge of the central path, because near the edge the contact time changes slowly with distance:

- Singapore (umbral depth 4.4 km): −7.7 / +7.8 s.
- Kurukshetra (path 21.6 km wide): −2.6 / +3.0 s.
- Zaragoza, Luxor, 1999 Bucharest (observer near the centre line): about ±1.5–2.7 s.

Three-way comparison for totality duration, 1999 Bucharest: simulator 2m 20.2s, Jubier 2m 22.1s, Besselian catalogue about 2m 24.4s (simulator + 4.2 s). Jubier therefore sits between the two, and the simulator is the lowest of the three. The ~2 s offset per internal contact is of the same order as the Jubier offsets in Section 3.4 (C2 +0.4…+1.1 s, C3 −0.8…−1.5 s), but larger.


#### 5.5.3 2005 Panama City: not a classification bug

The Besselian script also finds no C2/C3 at this location, so both programs agree that Panama City sees a **partial** eclipse (magnitude 0.985). The `type_at_location: annular` label in `cases.csv` is wrong, or the coordinates lie just outside the annular path. The case is still useful as a near-annular partial test; the remaining +0.56 pp obscuration difference is the same partial-branch bug as in 5.5.3.

#### 5.5.4 How to read the result

- The simulator reproduces the catalogue's Besselian solution at C1, C4 and MAX to within 1 s for every type of eclipse.
- The residual error is concentrated in C2/C3 and in the displayed magnitude, and it points to the lunar radius used for the internal contacts (Section 3.5).
- The obscuration reported for partial eclipses is wrong and should be fixed before the numbers are published.

### 5.6 Limitations of this cross-check

- **The reference is the simulator's own catalogue.** The script is independent *code* (no Skyfield, different solver), but it uses the same *data*. It therefore tests the geometry implementation and the contact solver, not the correctness of the Besselian elements themselves.
- **No limb correction.** Besselian contacts are geometric, so they compare with C1–C4, not with C1L–C4L.
- **No atmospheric refraction** and no topographic horizon.
- **ΔT is shared**, so ΔT errors cancel by construction.
- **Eclipse selection** by `|t0 − jd_tt| < 0.6 d` is adequate for the cases here but would be ambiguous for two eclipses within the same day; none exist in the case list.

---

## 6. Catalogue cross-check

A standalone Besselian-elements script run over the 71,264-event catalogue found **719 eclipses whose umbra/antumbra touched Romania in 30,000 years** (plus ~13,000 partial-only) and reproduced the documented track of the 11 Aug 1999 eclipse.

This validates the catalogue plumbing (parsing, time conversion, track geometry) at scale, but, as in Section 5.6, it does not independently validate the underlying elements.

---

## 7. Limb correction

Jubier lists the lunar-limb correction separately (`LC` column), for C2 and C3 only:

| Eclipse | LC at C2 | LC at C3 |
|---|---|---|
| 1999 | −1.8 s | −0.3 s |
| 2026 | +0.0 s | −1.5 s |
| 2027 | −2.1 s | −3.0 s |
| 2038 | −2.4 s | −2.1 s |

The simulator's limb-corrected contacts (C1L–C4L, from the neural elevation model) are not part of the Section 3 tables. For orientation, this is the shift the simulator applies at the internal contacts (`C2L − C2`, `C3L − C3`, from `simulator_results.json`):

| Eclipse | C2L − C2 | C3L − C3 |
|---|---|---|
| 1999 | −0.8 s | +3.1 s |
| 2026 | 0.0 s | 0.0 s |
| 2027 | −1.0 s | +2.8 s |
| 2038 | −0.3 s | +2.4 s |

Two observations worth checking before any C1L–C4L comparison:

- **Sign at C3.** The simulator moves C3 *later* by 2.4–3.1 s, while Jubier's `LC` at C3 is negative. Depending on how Jubier's sign convention is defined, this may be a convention difference or a real discrepancy.
- **2026.** The simulator applies no correction (Sun at 6°), while Jubier lists −1.5 s at C3.

To compare them, add `C1L`–`C4L` columns to `jubier_reference.csv` (Jubier's geometric times corrected by the `LC` column) and run `python run_validation.py --use-limb`.

---

## 8. Limitations

1. **Few independent cases.** Four eclipses against Jubier; the generalisation to other eclipses relies on the Besselian cross-check.
2. **One reference per quantity.** Jubier's pop-up is a read-out, not a data file; values were transcribed manually on 2026-10-03.
3. **0.1 s quantisation** on both sides: differences below about 0.2 s are not meaningful individually.
4. **ΔT beyond the present is unknowable.** Results after about 2030 depend on the ΔT forecast (Section 4).
5. **No coverage outside Jubier's maps.** Eclipses in the distant past or future have no independent reference at all.
6. **Case selection is deliberate, not random.** The cases were chosen to cover types and geometries, not sampled uniformly, so the statistics (RMS, mean) describe those cases only.
7. **Refraction and topography** are not modelled in either the width or the horizon events.

## 9. Open items

| # | Item | Why it matters | How to resolve |
|---|---|---|---|
| 1 | **Partial obscuration = 100 × magnitude** | Overstates obscuration by 6–11 pp for partial eclipses (Section 5.5.3) | Fix the partial branch of the backend to use the circle-overlap area; re-run both scripts and check Δ obsc. ≈ 0 |
| 2 | Lunar radius used for C2/C3 | Explains the C2/C3 offset, the totality/annularity duration bias and the +0.013 % magnitude bias (Sections 3.5, 5.5.2) | Inspect the radii used in the backend; re-run with the catalogue's `l1`/`l2` convention or a single mean radius |
| 3 | ΔT sensitivity for 2038 | Separates the ΔT effect from geometry | Force the simulator's ΔT to 83.9 s and re-run |
| 4 | `cases.csv` label for Panama City | Labelled annular, but the location is partial (Section 5.5.4) | Move the observer inside the annular path or relabel the case |
| 5 | Limb-corrected contacts | Sign and size of `C3L − C3` vs Jubier's `LC` (Section 7) | Section 7 procedure |
| 6 | Truncated reports | `truncated: true` appears for 2012 Albuquerque and 2019 Singapore in `simulator_results.json` | Confirm no event is lost in the UDP response |
| 7 | Regenerate Section 3.6 from the latest run | A few values (for example 2026 umbral velocity) differ from the current `simulator_results.json` | `python run_validation.py --out summary.md` |

---

## 10. Files

| File | Purpose |
|---|---|
| `cases.csv` | Eclipses, observer coordinates, elevations and regime (`core` / `far_future`) to evaluate |
| `collect_simulator.py` | Queries the running backend over UDP (same protocol as Unreal) and saves `simulator_results.json`, including the ΔT the backend used |
| `jubier_reference.csv` | Reference values read from Jubier's maps (contacts, ΔT, altitude, secondary quantities) |
| `run_validation.py` | Merges `simulator_results.json`, `cases.csv` and `jubier_reference.csv`, computes Δ, statistics and the Markdown tables |
| `Compare_simulator_vs_basselian.py` | Independent Besselian-element computation from `Eclipse_de_soare.txt` (no Skyfield, standard library only) |
| `besselian_reference.csv` | Output of the last `Compare_simulator_vs_basselian.py` run (2026-10-15): UTC contacts, magnitude and obscuration per case |
| `simulator_results.json` | Output of the last backend run (18 cases), used to produce the tables above |

## 11. How to reproduce

From the `validation` folder, close Unreal first (it uses the same UDP port), then:

    pip install -r requirements.txt
    python collect_simulator.py --server-dir ../Content/Encoder
    python run_validation.py --out summary.md

`requirements.txt` only pulls in the backend's dependencies (`../Server/requirements.txt`); the validation scripts themselves use just the Python standard library.

`--server-dir` must point to the folder that contains `Server.py` and the `_internal` runtime. If the backend is already running, omit `--server-dir`.

To regenerate the Jubier tables without running the backend, use the saved results (no installation needed):

    python run_validation.py --sim simulator_results.json --out summary.md

To run the Besselian cross-check (no installation, no backend):

    python Compare_simulator_vs_basselian.py

Edit `CATALOG_PATH`, `SIM_PATH` and `OUT_PATH` at the top of the script if your folder layout differs (default catalogue path: `..\Content\Encoder\Eclipse_de_soare.txt`). The script prints, per case, the differences simulator − reference for C1, C2, MAX, C3, C4, the magnitude and the obscuration, and writes `besselian_reference.csv`.

## 12. Conventions and glossary

| Term | Meaning |
|---|---|
| C1 | First contact: the Moon's limb first touches the Sun's (start of the partial phase) |
| C2 / C3 | Second / third contact: start / end of totality or annularity |
| C4 | Fourth contact: the Moon's limb leaves the Sun's (end of the partial phase) |
| MAX | Greatest eclipse at the observer |
| C1L–C4L, MAXL | Contacts corrected for the lunar limb profile (mountains and valleys) |
| Δ | This project − reference, in seconds; negative means the simulator is earlier |
| UTC | All times are UTC; contacts after midnight carry the next calendar date |
| ΔT | TT − UT1; the uncertainty in Earth's rotation, the dominant error source for future eclipses |
| Besselian elements | Polynomials describing the Moon's shadow in the fundamental plane (`x`, `y`, `d`, `mu`, `l1`, `l2`, `tan f1`, `tan f2`) |
| `m` | Distance between the shadow axis and the observer in the fundamental plane |
| `L1`, `L2` | Penumbral and umbral radii at the observer's plane |
| Regime | `core` = near-present, within Jubier's coverage; `far_future` = outside it |
| Magnitude | Fraction of the Sun's diameter covered (above 1.0 for total eclipses) |
| Obscuration | Fraction of the Sun's area covered |

## 13. References

- Xavier Jubier, *Solar Eclipse Maps*, interactive local circumstances (reference for Section 3).
- John Irwin, Besselian-element eclipse database, 2021 (basis of the catalogue used in Sections 5 and 6).
- Jean Meeus, *Astronomical Algorithms*, 2nd ed., chapter 54 (solar eclipses, local circumstances).
- Fred Espenak and Jean Meeus, *Five Millennium Canon of Solar Eclipses* and the NASA polynomial expressions for ΔT (2005–2050 segment cited in Section 4).
- Skyfield documentation, timescale and ΔT handling beyond the tabulated range (behaviour cited in Section 4).