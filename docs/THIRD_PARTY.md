# Third-party software, data and assets

3D Eclipse Simulator's own source code is released under the Apache License 2.0 (see [`LICENSE`](../LICENSE) and [`NOTICE`](../NOTICE)). Everything listed here is **not** covered by that licence and remains under its own terms.

## Software & astronomy engine

| | Used for |
|---|---|
| 🎮 [**Unreal Engine 5**](https://www.unrealengine.com/) — Epic Games (Unreal Engine EULA) | Real-time rendering engine |
| 🗺️ [**Cesium for Unreal**](https://cesium.com/) (Apache 2.0) | 3D globe and terrain streaming |
| 🌍 [**Cesium ion**](https://ion.cesium.com) — Cesium World Terrain and imagery layers | Terrain and imagery, streamed with each user's own access token. Data providers' credits must be displayed; they are shown by Cesium for Unreal's on-screen credits. |
| 🌐 [**Skyfield**](https://rhodesmill.org/skyfield/) — Brandon Rhodes | Astronomical calculations |
| 🪐 [**NASA JPL DE441**](https://naif.jpl.nasa.gov/) and SPICE kernels — NASA NAIF | Planetary and lunar ephemerides |
| 🛰️ **JPL satellite kernels** `jup365.bsp`, `sat441.bsp`, `nep097.bsp` — NASA NAIF | Jupiter, Saturn and Neptune moon systems |
| 📜 [**John Irwin**](http://www.jir1667.plus.com/eclipse/eclipsecats3/index.html) | *Thirty Millennium Catalog of Solar and Lunar Eclipses* (13,000 BC – 17,000 AD), 3rd ed., 2021 |
| 🌍 [**IERS**](https://www.iers.org/IERS/EN/DataProducts/EarthOrientationData/eop.html) | `finals2000A.all` Earth rotation data |
| 🧮 **Espenak & Meeus** | ΔT polynomials |
| ⭐ [**HYG Database**](https://www.astronexus.com/hyg) — David Nash / Astronexus (CC BY-SA 4.0) | Background star catalogue (HYG 4.2) |
| 🎞️ [**FFmpeg**](http://ffmpeg.org) (GPLv3.0) | Video processing. This software uses code of FFmpeg; source code is available on the official website. |
| 📍 [**Nominatim**](https://www.openstreetmap.org/copyright) — data © OpenStreetMap contributors | Geocoding service ([usage policy](https://operations.osmfoundation.org/policies/nominatim/)) |
| 🏙️ [**GeoNames**](https://www.geonames.org/) (CC BY 4.0) | City and country data (`cities5000`, `countryInfo`) used to generate `CountryPoints.csv` |
| 🐍 [**Python**](https://www.python.org/) — © Python Software Foundation | Backend server |
| 🔢 [**NumPy**](https://numpy.org/) (BSD License) | Scientific computing |
| 📌 [**GeoPy**](https://geopy.readthedocs.io/) (MIT License) | Geocoding client |
| 🌙 [**NASA LRO / LOLA**](https://astrogeology.usgs.gov/search/map/moon_lro_lola_dem_118m) LDEM 118 m — via USGS Astrogeology Science Center | Lunar elevation model used to train the limb model |

## Visual content (textures & 3D models)

| Asset | Source & license |
|---|---|
| Earth 2D texture | [NASA Scientific Visualization Studio](https://svs.gsfc.nasa.gov/2915) (public domain) |
| Moon texture | [NASA Scientific Visualization Studio](https://svs.gsfc.nasa.gov/4720/) (public domain) |
| Total solar eclipse photo | [NASA Science](https://science.nasa.gov/solar-system/skywatching/april-8-total-solar-eclipse-through-the-eyes-of-nasa/) (public domain) |
| Planetary textures | [SolarSystemScope.com](https://www.solarsystemscope.com/textures/) (CC BY 4.0) |
| Europa texture | [USGS Astrogeology Science Center](https://astrogeology.usgs.gov/search/map/europa_voyager_galileo_ssi_global_mosaic_500m) (public domain) |
| Io texture | [USGS Astrogeology Science Center](https://astrogeology.usgs.gov/search/map/io_galileo_ssi_voyager_color_merged_global_mosaic_1km) (public domain) |
| Ganymede texture | [NASA Science 3D Resources](https://science.nasa.gov/3d-resources/jupiter-ganymede/) (public domain) |
| Callisto texture | By Askaniy via [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Callisto_map_by_Askaniy.png) (CC BY-SA) |
| Triton texture | By Askaniy via [DeviantArt](https://www.deviantart.com/askaniy/art/Triton-Texture-Map-14K-761940736) (CC BY-SA) |
| Titan atmosphere texture | Based on original radar work by FarGetaNik ([Celestia Motherlode](http://www.celestiamotherlode.net/addon/addon_1675.html)), processed under CC BY-SA |
| 3D drone model | ["DJI FPV by SDC"](https://sketchfab.com/3d-models/dji-fpv-by-sdc-high-performance-drone-d471ea8c6235457b8e131842e2cf3783) by SDC PERFORMANCE (Lambo_SC04), licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Modified: changed its original color. |
| Constellation illustrations | [Western skycultures from Stellarium](https://github.com/Stellarium/stellarium-skycultures/tree/master/western/illustrations) (GPL) |

## Notes

- Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere. Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.
- All rights to astronomical data belong to the mentioned sources.