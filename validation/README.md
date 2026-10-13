# Validation

This folder contains the material used to check the contact times computed by 3D Eclipse Simulator against an independent reference, and to cross-check the eclipse catalogue.

## What is compared

- **Reference:** Xavier Jubier's interactive eclipse maps ([2026](http://xjubier.free.fr/en/site_pages/solar_eclipses/TSE_2026_GoogleMapFull.html), [2027](http://xjubier.free.fr/en/site_pages/solar_eclipses/TSE_2027_GoogleMapFull.html)).
- **Quantity:** geometric contact times C1–C4 in UTC, at the same observer coordinates on both sides.
- **Reference coverage:** Jubier's data cover 1990–2040 only. Outside this interval there is **no independent reference**.
- **Reported figure:** the largest absolute difference over C1–C4. It is **0.1–2 s** over the eclipses compared (≤ 1 s for 12 Aug 2026, ≤ 1.5 s for 2 Aug 2027).

## Reference settings

| Setting | Value |
|---|---|
| Reference data retrieved on | **[FILL: date]** |
| Jubier ΔT setting | **[FILL]** |
| Jubier limb-profile setting | **[FILL: on / off]** |
| This project: ΔT | Espenak & Meeus polynomials (outside 1900–2100); see main README |
| This project: limb correction in the compared values | **[FILL: geometric C1–C4 only / C1L–C4L]** |

## Per-contact comparison

Times in UTC. Δ = this project − Jubier, in seconds.

### 12 August 2026 — Zaragoza, Spain (41.649° N, 0.889° W)

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | **[FILL]** | **[FILL]** | **[FILL]** |
| C2 | **[FILL]** | **[FILL]** | **[FILL]** |
| C3 | **[FILL]** | **[FILL]** | **[FILL]** |
| C4 | **[FILL]** | **[FILL]** | **[FILL]** |

### 2 August 2027 — Luxor, Egypt (25.635° N, 32.594° E)

| Contact | This project | Jubier | Δ (s) |
|---|---|---|---|
| C1 | **[FILL]** | **[FILL]** | **[FILL]** |
| C2 | **[FILL]** | **[FILL]** | **[FILL]** |
| C3 | **[FILL]** | **[FILL]** | **[FILL]** |
| C4 | **[FILL]** | **[FILL]** | **[FILL]** |

### Third location (optional)

Add a third eclipse and location here, ideally one from the 1990–2025 range with a different geometry (annular or a high-latitude eclipse), so the 0.1–2 s range is backed by more than two eclipses.

## Catalogue cross-check

A standalone Besselian-elements script was run over the 71,264-event catalogue. It found **719 eclipses** whose umbra/antumbra touched Romania in 30,000 years (plus ~13,000 partial-only) and reproduced the documented track of the 11 Aug 1999 eclipse.

## Files

| File | Purpose |
|---|---|
| **[FILL: script name]** | Besselian-elements script used for the cross-check |
| **[FILL: script / notebook]** | Comparison of this project's contact times with Jubier's values |
| **[FILL: data files]** | Reference values copied from Jubier's maps, with the retrieval date |

## How to reproduce

```bash
pip install -r ../Server/requirements.txt
python [FILL: script name]
```