import os
import sys
import time
import socket
import struct
import math
import threading
import traceback

import numpy as np
from geopy.geocoders import Nominatim

from skyfield.api import Loader, wgs84
from skyfield.positionlib import Geocentric
from skyfield.constants import AU_KM
from skyfield import almanac
from skyfield.framelib import itrs
from skyfield.errors import EphemerisRangeError
import csv

# --- CONFIGURARE ---
HOST = '127.0.0.1'
PORT_IN = 5000
PORT_OUT = 5001

# --- CONSTANTE FIZICE (RELIEF LUNAR) ---
#RADIUS_SUN_KM = 695507.943
RADIUS_SUN_KM = 696000
RADIUS_MOON_PEAKS = 1738.75  # C1/C4
RADIUS_MOON_VALLEYS = 1736.05  # C2/C3
RADIUS_MOON_VISUAL = 1737.93  # HUD
RADIUS_MOON_XAVIER = 1736.65 #pentru calcul banda de totalitate
d_sun_km = RADIUS_SUN_KM * 2
d_moon_km = RADIUS_MOON_PEAKS * 2
EARTH_RADIUS_KM = 6371.0

# Atmosfera
STD_PRESSURE = 1013.25
STD_TEMP_C = 15.0

CUSTOM_DELTAT_YEAR_MIN = 1900
CUSTOM_DELTAT_YEAR_MAX = 2100

N_LEVELS, N_FEATURES, LOG2_HASHMAP_SIZE = 20, 2, 21
BASE_RES_LAT, FINEST_RES_LAT = 16, 46080
PRIME_LON, PRIME_LAT = 1, 2654435761

import torch
import torch.nn as nn
#import numpy as np

class _Encoding(nn.Module):
    def __init__(self, recommended_lon_cells):
        super().__init__()
        b = np.exp((np.log(FINEST_RES_LAT) - np.log(BASE_RES_LAT)) / (N_LEVELS - 1))
        self.res_lat = [max(1, int(round(BASE_RES_LAT * (b ** l)))) for l in range(N_LEVELS)]
        aspect = recommended_lon_cells / FINEST_RES_LAT
        self.res_lon = [max(1, int(round(r * aspect))) for r in self.res_lat]
        max_size = 2 ** LOG2_HASHMAP_SIZE
        self.table_sizes, self.use_hash = [], []
        for rlon, rlat in zip(self.res_lon, self.res_lat):
            n = (rlon + 1) * (rlat + 1)
            self.table_sizes.append(min(n, max_size))
            self.use_hash.append(n > max_size)
        self.embeddings = nn.ModuleList([nn.Embedding(s, N_FEATURES) for s in self.table_sizes])

    def _idx(self, ix, iy, lvl):
        if not self.use_hash[lvl]:
            return ix * (self.res_lat[lvl] + 1) + iy
        h = (ix.to(torch.int64) * PRIME_LON) ^ (iy.to(torch.int64) * PRIME_LAT)
        return h % self.table_sizes[lvl]

    def forward(self, u, v):
        out = []
        for lvl in range(N_LEVELS):
            rlon, rlat = self.res_lon[lvl], self.res_lat[lvl]
            x, y = u * rlon, v * rlat
            x0f, y0f = torch.floor(x), torch.floor(y)
            fx, fy = (x - x0f).unsqueeze(-1), (y - y0f).unsqueeze(-1)
            x0, x1 = x0f.long().clamp(0, rlon), (x0f.long() + 1).clamp(0, rlon)
            y0, y1 = y0f.long().clamp(0, rlat), (y0f.long() + 1).clamp(0, rlat)
            e00 = self.embeddings[lvl](self._idx(x0, y0, lvl))
            e01 = self.embeddings[lvl](self._idx(x0, y1, lvl))
            e10 = self.embeddings[lvl](self._idx(x1, y0, lvl))
            e11 = self.embeddings[lvl](self._idx(x1, y1, lvl))
            top = e00 * (1 - fx) + e10 * fx
            bot = e01 * (1 - fx) + e11 * fx
            out.append(top * (1 - fy) + bot * fy)
        return torch.cat(out, dim=-1)


class LunarAltitudeNet(nn.Module):
    def __init__(self, recommended_lon_cells, hidden=256):
        super().__init__()
        self.encoding = _Encoding(recommended_lon_cells)
        self.mlp = nn.Sequential(
            nn.Linear(N_LEVELS * N_FEATURES, hidden), nn.GELU(),
            nn.Linear(hidden, hidden), nn.GELU(),
            nn.Linear(hidden, hidden), nn.GELU(),
            nn.Linear(hidden, 1),
        )

    def forward(self, u, v):
        return self.mlp(self.encoding(u, v)).squeeze(-1)


def load(path, device="cpu"):
    """Returneaza (model, metadata_dict) gata de folosit."""
    ckpt = torch.load(path, map_location=device, weights_only=False)
    model = LunarAltitudeNet(ckpt["recommended_lon_cells"]).to(device)
    model.load_state_dict(ckpt["model_state"])
    model.eval().float()
    meta = {
        "alt_mean": ckpt["alt_mean"], "alt_std": ckpt["alt_std"],
        "limb_centers_deg": ckpt["limb_centers_deg"],
        "band_half_width_deg": ckpt["band_half_width_deg"],
        "lat_min": ckpt["lat_min"], "lat_max": ckpt["lat_max"],
    }
    return model, meta

_altitude_model, _altitude_meta = load("lunar_altitude_contact_bands_fp16_finetuned.pt", device="cpu")


def uses_custom_deltat(year):
    return year < CUSTOM_DELTAT_YEAR_MIN or year > CUSTOM_DELTAT_YEAR_MAX


def build_tt_time(year, month, day, hour, minute, second):
    """
    Construieste timpul TT din data/ora UT. Pentru anii din afara ferestrei moderne
    (trecut sau viitor indepartat) folosim mereu formula custom Espenak-Meeus, ca sa
    ramanem consistenti cu format_event_time la afisare.
    """
    if uses_custom_deltat(year):
        y, m = year, month
        if m <= 2:
            y -= 1
            m += 12

        if year <= 1582:
            # calendar Iulian proleptic, valabil pentru date pre-reforma gregoriana
            jd_midnight = (1461 * y) // 4 + (153 * (m - 3) + 2) // 5 + day + 1721116.5
        else:
            # calendar Gregorian standard, valabil si pentru viitorul indepartat
            a = y // 100
            b = 2 - a + (a // 4)
            jd_midnight = math.floor(365.25 * (y + 4716)) + math.floor(30.6001 * (m + 1)) + day + b - 1524.5

        fraction_of_day = (hour + (minute / 60.0) + (second / 3600.0)) / 24.0
        jd_ut = jd_midnight + fraction_of_day

        dt_sec = delta_t_seconds(year, month)
        jd_tt = jd_ut + dt_sec / 86400.0
        return ts.tt_jd(jd_tt)
    else:
        # in fereastra moderna, folosim datele reale IERS din Skyfield
        return ts.utc(year, month, day, hour, minute, second)


def jd_ut_to_hms(jd_ut):
    """Extrage ora/minutul/secunda dintr-un Julian Date UT, indiferent de granita de zi."""
    jd_shifted = jd_ut + 0.5
    frac = jd_shifted - math.floor(jd_shifted)
    total_seconds = (frac * 86400.0) % 86400.0
    h = int(total_seconds // 3600)
    m = int((total_seconds % 3600) // 60)
    s = total_seconds % 60
    return h, m, s


def format_event_time(t_ev, ref_year, ref_month):
    """
    Converteste un Time Skyfield (TT) in ora UTC afisabila, folosind ACEEASI
    formula custom ca la build_tt_time, pentru orice an in afara ferestrei
    moderne (trecut sau viitor indepartat). t_ev.utc ar folosi modelul intern
    Skyfield, care nu se anuleaza perfect cu formula noastra custom.
    """
    if uses_custom_deltat(ref_year):
        dt_sec = delta_t_seconds(ref_year, ref_month)
        jd_ut = t_ev.tt - dt_sec / 86400.0
        h, m, s = jd_ut_to_hms(jd_ut)
        secs = int(s)
        msecs = int((s - secs) * 10)
        return f"{h:02d}:{m:02d}:{secs:02d}.{msecs:01d}"
    else:
        ev_u = t_ev.utc
        secs = int(ev_u[5])
        msecs = int((ev_u[5] - secs) * 10)
        return f"{ev_u[3]:02d}:{ev_u[4]:02d}:{secs:02d}.{msecs:01d}"

# --- GEOLOCATOR & SKYFIELD ---
# --- GEOLOCATOR & SKYFIELD ---
geolocator = Nominatim(user_agent="EclipseSim_Final_FullLocationData")

# 1. Aflăm locația reală a executabilului ca să descarce fișierele FIX lângă el
if getattr(sys, 'frozen', False):
    application_path = os.path.dirname(sys.executable)
else:
    application_path = os.path.dirname(os.path.abspath(__file__))

# 1. Aflăm locația reală a executabilului
if getattr(sys, 'frozen', False):
    application_path = os.path.dirname(sys.executable)
else:
    application_path = os.path.dirname(os.path.abspath(__file__))

# ==========================================================
# 2. SCUTUL ANTI-CRASH: Redirecționăm toate printurile către un fișier!
# ==========================================================
#log_path = os.path.join(application_path, 'server_jurnal.txt')
#sys.stdout = open(log_path, 'w', encoding='utf-8')
#sys.stderr = sys.stdout  # Trimitem si erorile rosii in acelasi fisier

# 2. CREĂM ÎNCĂRCĂTORUL SILENȚIOS (Asta rezolvă eroarea 'flush' pe alte PC-uri!)
load = Loader(application_path, verbose=False)

#print("Se încarcă efemeridele NASA (DE441)...")
if not load.exists('finals2000A.all'):
    load.download('finals2000A.all')

ts = load.timescale(builtin= False)
planets = load('de441.bsp')
earth, sun, moon = planets['earth'], planets['sun'], planets['moon']

mercury = planets['mercury barycenter']
venus   = planets['venus barycenter']
mars    = planets['mars barycenter']
jupiter = planets['jupiter barycenter']
saturn  = planets['saturn barycenter']
uranus  = planets['uranus barycenter']
neptune = planets['neptune barycenter']

# Le punem într-o listă fixă pentru a le procesa ușor în buclă
target_planets = [mercury, venus, mars, jupiter, saturn, uranus, neptune]

URL_JUP = 'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/jup365.bsp'
URL_SAT = 'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/sat441.bsp'
URL_NEP = 'https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/satellites/nep097.bsp'

#print("Verific baze de date sateliti...")

# Jupiter (jup365.bsp)
if not load.exists('jup365.bsp'):
    load.download(URL_JUP)
eph_jup = load('jup365.bsp')
io, europa, ganymede, callisto = eph_jup['Io'], eph_jup['Europa'], eph_jup['Ganymede'], eph_jup['Callisto']

# Saturn (sat441.bsp)
if not load.exists('sat441.bsp'):
    load.download(URL_SAT)
eph_sat = load('sat441.bsp')
titan = eph_sat['Titan']

# Neptun (nep097.bsp)
if not load.exists('nep097.bsp'):
    load.download(URL_NEP)
eph_nep = load('nep097.bsp')
triton = eph_nep['Triton']

major_moons = [io, europa, ganymede, callisto, titan, triton]

# Lista fixă a tuturor celor 6 sateliți trimiși către Unreal Engine
major_moons = [io, europa, ganymede, callisto, titan, triton]

#print(f"Server GATA! UDP {HOST}:{PORT_IN} -> {PORT_OUT}")

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((HOST, PORT_IN))

# --- VARIABILE GLOBALE ---
last_calc_lat_lon = (-999.0, -999.0)
last_calc_lang = "None"
last_calc_date = (0, 0, 0)
last_real_time_calc = 0.0
cached_location_name = "Initializare..."
# Aici vom stoca textul combinat (Locatie + Tabel)
cached_report_text = "Sistem pornit.\nAsteptare date locatie..."

#SEARCH_START_JD = float(ts.utc(2026, 8, 12, 17, 0, 0).tt)
#SEARCH_END_JD = float(ts.utc(2026, 8, 12, 19, 45, 0).tt)


is_calculating = False
path_sent = False
thread_calc = None

cached_eclipse_path = []  # Lista care va tine punctele traseului

last_global_path_date = (0, 0, 0)
global_best_jd = None


def get_live_shadow_center_wgs84(t, sun_obj, moon_obj, earth_obj):
    global global_best_jd
    try:
        # Functie interna ca sa nu scriu matematica de 2 ori
        def calc_for_time(time_val):
            p_s = earth_obj.at(time_val).observe(sun_obj).apparent().frame_xyz(itrs).km
            p_m = earth_obj.at(time_val).observe(moon_obj).apparent().frame_xyz(itrs).km
            z_dir = p_m - p_s
            z_dir /= np.linalg.norm(z_dir)
            a_rad, c_rad = 6378.137, 6356.752314
            A = (z_dir[0] ** 2 + z_dir[1] ** 2) / a_rad ** 2 + (z_dir[2] ** 2) / c_rad ** 2
            B = 2 * ((p_m[0] * z_dir[0] + p_m[1] * z_dir[1]) / a_rad ** 2 + (p_m[2] * z_dir[2]) / c_rad ** 2)
            C = (p_m[0] ** 2 + p_m[1] ** 2) / a_rad ** 2 + (p_m[2] ** 2) / c_rad ** 2 - 1.0
            return A, B, C, z_dir, p_m, a_rad, c_rad

        A, B, C, Z, p_moon, a, c = calc_for_time(t)
        delta = B ** 2 - 4 * A * C
        hit = None

        if delta >= 0:
            k = (-B - math.sqrt(delta)) / (2 * A)
            if k > 0: hit = p_moon + k * Z
        else:
            # --- AICI E MAGIA SALVATOARE ---
            # Unreal a trimis o oră greșită (ex: 00:00:00). Umbra e în spațiu.
            # Folosim timpul perfect al maximului calculat de funcția de bandă!
            if global_best_jd is not None:
                t_max = ts.tt_jd(global_best_jd)
                A, B, C, Z, p_moon, a, c = calc_for_time(t_max)
                delta = B ** 2 - 4 * A * C
                if delta >= 0:
                    k = (-B - math.sqrt(delta)) / (2 * A)
                    if k > 0: hit = p_moon + k * Z

            # Dacă nici la maxim nu atinge Pământul (eclipsă excentrică), aplicăm fallback pe sol
            if hit is None:
                k_closest = -B / (2 * A)
                if k_closest > 0:
                    p_ray = p_moon + k_closest * Z
                    scale = math.sqrt((p_ray[0] / a) ** 2 + (p_ray[1] / a) ** 2 + (p_ray[2] / c) ** 2)
                    hit = p_ray / scale

        if hit is not None:
            p = math.sqrt(hit[0] ** 2 + hit[1] ** 2)
            lat = math.degrees(math.atan2(hit[2], p * (c ** 2 / a ** 2)))
            lon = math.degrees(math.atan2(hit[1], hit[0]))
            return float(lat), float(lon)
    except Exception:
        pass

    return -999.0, -999.0


def update_global_eclipse_path(t_current):
    """
    Calculează traseul global, căutând exact Punctele de Contact cu globul terestru.
    """
    global cached_eclipse_path, last_global_path_date, global_best_jd

    ut = t_current.utc
    current_date = (int(ut[0]), int(ut[1]), int(ut[2]))

    if current_date == last_global_path_date:
        return

    era_str = "î.Hr." if current_date[0] < 0 else "d.Hr."
    #print(f"\n--- Cautam Eclipsa GLOBALA pentru {abs(current_date[0])} {era_str} {current_date[1]}-{current_date[2]} ---")
    last_global_path_date = current_date

    ani_fata_de_prezent = abs(current_date[0] - 2000)
    # Lărgim inteligent fereastra de căutare doar pentru anii antici
    margin_days = 0.5 + (ani_fata_de_prezent / 1000.0) * 0.6 if ani_fata_de_prezent > 500 else 0.5

    # --- REZOLVAREA BUG-ULUI DE DRIFT CALENDARISTIC ---
    # Nu mai reconstruim data. Ne bazăm direct pe timpul absolut primit din Unreal.
    t_start = ts.tt_jd(t_current.tt - margin_days)
    t_end = ts.tt_jd(t_current.tt + margin_days)

    curr = t_start.tt
    step = 5.0 / 1440.0
    min_sep = 1000.0
    best_jd = curr

    # Găsim cel mai apropiat moment (Maximul Geometric)
    while curr <= t_end.tt:
        t = ts.tt_jd(curr)
        s = earth.at(t).observe(sun).apparent()
        m = earth.at(t).observe(moon).apparent()
        sep = s.separation_from(m).degrees

        if sep < min_sep:
            min_sep = sep
            best_jd = curr
        curr += step

    # Dacă umbra e suficient de aproape de Pământ (sub ~1.5 grade)
    if min_sep < 1.5:
        global_best_jd = best_jd

        limit_deg = 1.50

        # Căutăm Start-ul (Când umbra intră pe disc)
        t_s = best_jd
        while t_s > best_jd - 0.25:
            t = ts.tt_jd(t_s)
            sep = earth.at(t).observe(sun).apparent().separation_from(earth.at(t).observe(moon).apparent()).degrees
            if sep > limit_deg:
                break
            t_s -= (0.1 / 1440.0)

        # Căutăm End-ul (Când umbra iese de pe disc)
        t_e = best_jd
        while t_e < best_jd + 0.25:
            t = ts.tt_jd(t_e)
            sep = earth.at(t).observe(sun).apparent().separation_from(earth.at(t).observe(moon).apparent()).degrees
            if sep > limit_deg:
                break
            t_e += (1.0 / 1440.0)

        # Generăm calea exclusiv pentru intervalul exact de contact
        cached_eclipse_path = calculate_path_for_udp(t_s, t_e)

        if len(cached_eclipse_path) > 0:
            #print(f"SUCCES! Am generat {len(cached_eclipse_path)} puncte absolute pentru axa centrala.")
            pass
        else:
            cached_eclipse_path = []
            #print("A fost doar o eclipsa inelara/partiala astazi (umbra nu a atins solul).")
    else:
        cached_eclipse_path = []
        #print("Nicio eclipsa globala astazi.")


def calculate_path_for_udp(t_start_jd, t_end_jd):
    """
    Calculează AXA CENTRALĂ și LĂȚIMEA EXACTĂ a benzii.
    Folosește metoda proiecției elipsei pe planul tangent la sol.
    """
    num_steps = 100
    step = (t_end_jd - t_start_jd) / num_steps
    curr = t_start_jd

    a, c = 6378.137, 6356.752314
    R_SUN_KM = 696000.0
    #R_MOON_KM = 1737.4
    #R_MOON_KM = RADIUS_MOON_VALLEYS
    R_MOON_KM = RADIUS_MOON_XAVIER
    def get_hit(ray_origin, ray_dir):
        ox, oy, oz = ray_origin
        dx, dy, dz = ray_dir
        A = (dx ** 2 + dy ** 2) / a ** 2 + (dz ** 2) / c ** 2
        B = 2 * ((ox * dx + oy * dy) / a ** 2 + (oz * dz) / c ** 2)
        C = (ox ** 2 + oy ** 2) / a ** 2 + (oz ** 2) / c ** 2 - 1.0
        delta = B ** 2 - 4 * A * C
        if delta >= 0:
            k = (-B - math.sqrt(delta)) / (2 * A)
            if k > 0:
                return ray_origin + k * ray_dir, True
        else:
            k_closest = -B / (2 * A)
            if k_closest > 0:
                p_ray = ray_origin + k_closest * ray_dir
                scale = math.sqrt((p_ray[0] / a) ** 2 + (p_ray[1] / a) ** 2 + (p_ray[2] / c) ** 2)
                # Permitem proiectarea pe glob dacă raza trece suficient de aproape
                if scale < 1.002:
                    return p_ray / scale, False
        return None, None

    valid_hits_3d = []
    valid_widths = []
    hit_flags = []  # NOU: Aici memorăm dacă fiecare punct a fost pe centru (True) sau margine (False)

    while curr <= t_end_jd:
        t = ts.tt_jd(curr)
        t_next = ts.tt_jd(curr + 1.0 / 86400.0)

        sun_pos = earth.at(t).observe(sun).apparent().frame_xyz(itrs).km
        moon_pos = earth.at(t).observe(moon).apparent().frame_xyz(itrs).km
        sun_pos_next = earth.at(t_next).observe(sun).apparent().frame_xyz(itrs).km
        moon_pos_next = earth.at(t_next).observe(moon).apparent().frame_xyz(itrs).km

        Z = (moon_pos - sun_pos)
        dist_sun_moon = np.linalg.norm(Z)
        Z /= dist_sun_moon

        hit_c, hit_limit = get_hit(moon_pos, Z)

        if hit_c is not None:
            dist_moon_earth = np.linalg.norm(hit_c - moon_pos)
            cone_taper = (R_SUN_KM - R_MOON_KM) / dist_sun_moon
            r_perp_km = abs(R_MOON_KM - (dist_moon_earth * cone_taper))

            N = np.array([hit_c[0] / a ** 2, hit_c[1] / a ** 2, hit_c[2] / c ** 2])
            N /= np.linalg.norm(N)

            u = np.cross(Z, N)
            u_norm = np.linalg.norm(u)
            if u_norm > 1e-6:
                u /= u_norm
            else:
                u = np.array([1.0, 0.0, 0.0])

            v = np.cross(u, N)
            v /= np.linalg.norm(v)

            cos_phi = abs(np.dot(Z, N))
            cos_phi = max(cos_phi, 0.02)

            Z_next = (moon_pos_next - sun_pos_next)
            Z_next /= np.linalg.norm(Z_next)

            dot_Z_next_N = np.dot(Z_next, N)

            k = -np.dot(moon_pos_next - hit_c, N) / dot_Z_next_N
            hit_c_next = moon_pos_next + k * Z_next

            w = hit_c_next - hit_c
            w_norm = np.linalg.norm(w)

            if w_norm > 1e-6:
                w /= w_norm
                p = np.cross(N, w)
                p /= np.linalg.norm(p)

                A = r_perp_km * np.dot(u, p)
                B = (r_perp_km / cos_phi) * np.dot(v, p)
                true_width_km = 2.0 * math.sqrt(A ** 2 + B ** 2)
            else:
                true_width_km = r_perp_km * 2.0

            # Nu mai punem if-uri cu 300 aici. Lăsăm lățimea să fie limitată la un plafon general de siguranță.
            true_width_km = min(true_width_km, 900.0)

            valid_hits_3d.append(hit_c)
            valid_widths.append(true_width_km)
            hit_flags.append(hit_limit)  # Salvăm statusul (True/False)

        curr += step

    if not valid_hits_3d:
        return []

    # ===============================================================
    # POST-PROCESARE LĂȚIME: Aici "înghețăm" lățimea la capete automat!
    # ===============================================================
    # Găsim care sunt indicii în care umbra a lovit direct (True)
    direct_indices = [i for i, flag in enumerate(hit_flags) if flag == True]

    if len(direct_indices) > 0:
        first_direct = direct_indices[0]  # Primul punct valid de la răsărit
        last_direct = direct_indices[-1]  # Ultimul punct valid de la apus (înainte de Mallorca)

        width_at_sunrise = valid_widths[first_direct]
        width_at_sunset = valid_widths[last_direct]

        # Parcurgem din nou și rescriem capetele de orizont cu valoarea solidă
        for i in range(len(valid_widths)):
            if i < first_direct:
                # Pentru extensia dinaintea răsăritului
                valid_widths[i] = width_at_sunrise
            elif i > last_direct:
                # Pentru extensia de la apus (peste Marea Mediterană)
                valid_widths[i] = width_at_sunset

    # ===============================================================
    # INTERPOLARE PENTRU CELE 100 DE PUNCTE
    # ===============================================================
    pts_array = np.array(valid_hits_3d)
    widths_array = np.array(valid_widths)

    old_indices = np.linspace(0, 1, len(pts_array))
    new_indices = np.linspace(0, 1, 100)

    interp_x = np.interp(new_indices, old_indices, pts_array[:, 0])
    interp_y = np.interp(new_indices, old_indices, pts_array[:, 1])
    interp_z = np.interp(new_indices, old_indices, pts_array[:, 2])
    interp_widths = np.interp(new_indices, old_indices, widths_array)

    center_pts = []

    for i in range(100):
        hx = interp_x[i]
        hy = interp_y[i]
        hz = interp_z[i]

        p = math.sqrt(hx ** 2 + hy ** 2)
        lat = math.degrees(math.atan2(hz, p * (c ** 2 / a ** 2)))
        lon = math.degrees(math.atan2(hy, hx))
        width_km = interp_widths[i]

        center_pts.append([lon, lat, width_km])

    return center_pts

"""
def background_calculate(p_lat, p_lon, p_alt, t_current, year, month, day, lang):
    global cached_location_name, cached_report_text, is_calculating
    global last_calc_lat_lon, last_calc_date, last_calc_lang
    try:
        dist = abs(p_lat - last_calc_lat_lon[0]) + abs(p_lon - last_calc_lat_lon[1])
        if dist > 0.0005 or lang != last_calc_lang:
            new_name = get_location_name(p_lat, p_lon, lang)
            if "Eroare" not in new_name and "Unknown" not in new_name:
                cached_location_name = new_name
        update_global_eclipse_path(t_current)
        technical_table = generate_report_table(p_lat, p_lon, p_alt, t_current, lang, year, month)
        if lang == "English":
            full_text = f"LOCATION: {cached_location_name}\n"
            full_text += f"COORDS: {p_lat:.4f}, {p_lon:.4f}\n\n"
        else:
            full_text = f"LOCATIE: {cached_location_name}\n"
            full_text += f"COORDONATE: {p_lat:.4f}, {p_lon:.4f}\n\n"

        full_text += technical_table
        cached_report_text = full_text
        #last_calc_lat_lon = (p_lat, p_lon)
        #last_calc_date = (year, month, day)
        #last_calc_lang = lang
    except Exception as e:
        traceback.print_exc()
        #print("Eroare in Thread-ul de calcul:", e)
    finally:
        is_calculating = False
"""

def background_calculate(p_lat, p_lon, p_alt, t_current, year, month, day, lang):
    global cached_location_name, cached_report_text, is_calculating
    try:
        # 1. Reverse Geocoding protejat (fara conditii de distanta, cu fallback GPS)
        try:
            new_name = get_location_name(p_lat, p_lon, lang)
            if new_name and "Eroare" not in new_name and "Unknown" not in new_name and "Necunoscuta" not in new_name:
                cached_location_name = new_name
            else:
                cached_location_name = f"GPS ({p_lat:.2f}°, {p_lon:.2f}°)"
        except Exception:
            cached_location_name = f"GPS ({p_lat:.2f}°, {p_lon:.2f}°)"

        # 2. Actualizare traseu eclipsa globala (izolat de raportul text)
        try:
            update_global_eclipse_path(t_current)
        except Exception:
            pass

        # 3. Generare tabel tehnic (cu fallback daca data e in afara limitelor)
        technical_table = ""
        try:
            technical_table = generate_report_table(p_lat, p_lon, p_alt, t_current, lang, year, month)
        except Exception:
            traceback.print_exc()
            technical_table = "Date tabel indisponibile pentru aceasta epoca.\n"

        # 4. Asamblare text final garantat
        lbl_loc = "LOCATION:" if lang == "English" else "LOCATIE:"
        lbl_coords = "COORDS:" if lang == "English" else "COORDONATE:"
        cached_report_text = f"{lbl_loc} {cached_location_name}\n{lbl_coords} {p_lat:.4f}, {p_lon:.4f}\n\n{technical_table}"

    except Exception:
        # Fallback de urgenta absoluta (in caz de eroare de memorie sau sistem)
        lbl_loc = "LOCATION:" if lang == "English" else "LOCATIE:"
        cached_report_text = f"{lbl_loc} GPS ({p_lat:.2f}°, {p_lon:.2f}°)\nCOORDONATE: {p_lat:.4f}, {p_lon:.4f}\n\nSistem activ.\n"
    finally:
        is_calculating = False


def find_next_eclipse(observer, t_start):
    """Sare din Lună Nouă în Lună Nouă pentru a găsi următoarea eclipsă."""
    t_end = ts.tt_jd(t_start.tt + 365 * 1.2)  # Cautăm max 1.2 ani în viitor
    f = almanac.moon_phases(planets)
    t_phases, values_phases = almanac.find_discrete(t_start, t_end, f)

    for t_phase, phase_val in zip(t_phases, values_phases):
        if phase_val == 0:  # 0 înseamnă Lună Nouă
            sep = get_separation_geometric(observer, t_phase)
            if sep < 1.5:  # Posibilă eclipsă aici
                curr = t_phase.tt - 0.25
                end_scan = t_phase.tt + 0.25
                step = 5.0 / 1440.0
                min_sep = 1000.0
                best_jd = curr
                while curr < end_scan:
                    t_scan = ts.tt_jd(curr)
                    s = get_separation_geometric(observer, t_scan)
                    if s < min_sep:
                        min_sep, best_jd = s, curr
                    curr += step

                t_best = ts.tt_jd(best_jd)
                if min_sep < (get_radius_degrees(sun, observer, t_best) + get_radius_degrees(moon, observer, t_best,
                                                                                             'visual')):
                    return best_jd  # Am găsit JD-ul viitoarei eclipse!
    return None


def delta_t_seconds(year, month=6):
    """
    Formula Espenak & Meeus (NASA/TP-2006-214141). ACEEAȘI formulă trebuie
    portată identic în C++ (LoadAndParseDatabase), ca cele două conversii
    (TT->UT în catalog, UT->TT în server) să se anuleze perfect la round-trip.
    """
    y = year + (month - 0.5) / 12.0

    if y < -500:
        u = (y - 1820) / 100.0
        return -20.0 + 32.0 * u ** 2
    elif y < 500:
        u = y / 100.0
        return (10583.6 - 1014.41 * u + 33.78311 * u**2 - 5.952053 * u**3
                - 0.1798452 * u**4 + 0.022174192 * u**5 + 0.0090316521 * u**6)
    elif y < 1600:
        u = (y - 1000) / 100.0
        return (1574.2 - 556.01 * u + 71.23472 * u**2 + 0.319781 * u**3
                - 0.8503463 * u**4 - 0.005050998 * u**5 + 0.0083572073 * u**6)
    else:
        # pentru >1582 folosim oricum ts.utc (tabel IERS real), deci nu ajunge aici
        u = (y - 1820) / 100.0
        return -20.0 + 32.0 * u ** 2


def get_skyfield_time(year, month, day, hour, minute, second):
    """Calculează TT direct, folosind ΔT explicit — NU lăsăm Skyfield să extrapoleze singur."""
    if year <= 1582:
        y, m = year, month
        if m <= 2:
            y -= 1
            m += 12
        jd_midnight = math.floor(365.25 * (y + 4716)) + math.floor(30.6001 * (m + 1)) + day - 1524.5
        fraction_of_day = (hour + (minute / 60.0) + (second / 3600.0)) / 24.0
        jd_ut = jd_midnight + fraction_of_day

        dt_sec = delta_t_seconds(year, month)
        jd_tt = jd_ut + dt_sec / 86400.0
        return ts.tt_jd(jd_tt)          # <- direct TT, nu ut1_jd
    else:
        return ts.utc(year, month, day, hour, minute, second)

"""def get_location_name(lat, lon, lang):
    try:
        # Setăm limba pentru API-ul de geolocație
        loc_lang = 'en' if lang == "English" else 'ro'

        location = geolocator.reverse((lat, lon), exactly_one=True, language=loc_lang, timeout=2)
        if location:
            addr = location.raw.get('address', {})
            localitate = addr.get('suburb') or addr.get('city_district') or addr.get('city') or addr.get(
                'town') or addr.get('village', '')
            tara = addr.get('country', '')
            if localitate and tara: return f"{localitate}, {tara}"
            return location.address.split(',')[0]
    except Exception:
        pass

    return "Unknown Location" if lang == "English" else "Locatie Necunoscuta"
"""

def get_location_name(lat, lon, lang):
    try:
        loc_lang = 'en' if lang == "English" else 'ro'
        location = geolocator.reverse((lat, lon), exactly_one=True, language=loc_lang, timeout=2)

        if location:
            addr = location.raw.get('address', {})

            localitate = (
                addr.get('suburb') or addr.get('city_district') or
                addr.get('city') or addr.get('town') or
                addr.get('village') or addr.get('municipality') or
                addr.get('county') or addr.get('state') or ''
            )
            tara = addr.get('country', '')

            if localitate and tara:
                return f"{localitate}, {tara}"
            elif tara:
                return tara
            elif localitate:
                return localitate
            else:
                return location.address  # ultimul fallback, adresa completă

    except Exception:
        pass

    return "Unknown Location" if lang == "English" else "Locatie Necunoscuta"

# --- HELPERE ASTRONOMIE ---
def get_radius_degrees(body, observer, t, radius_type='visual'):
    dist_km = observer.at(t).observe(body).distance().km
    if body == sun:
        r_km = RADIUS_SUN_KM
    else:
        if radius_type == 'peaks':
            r_km = RADIUS_MOON_PEAKS
        elif radius_type == 'valleys':
            r_km = RADIUS_MOON_VALLEYS
        else:
            r_km = RADIUS_MOON_VISUAL
    return math.degrees(math.asin(r_km / dist_km))


def get_separation_geometric(observer, t):
    s = observer.at(t).observe(sun)
    m = observer.at(t).observe(moon)
    return s.separation_from(m).degrees



def calculate_p_angle(observer, t):
    s = observer.at(t).observe(sun)
    m = observer.at(t).observe(moon)
    ra_sun, dec_sun, _ = s.radec()
    ra_moon, dec_moon, _ = m.radec()
    a1, d1 = ra_sun.radians, dec_sun.radians
    a2, d2 = ra_moon.radians, dec_moon.radians
    y = math.sin(a2 - a1) * math.cos(d2)
    x = math.cos(d1) * math.sin(d2) - math.sin(d1) * math.cos(d2) * math.cos(a2 - a1)
    pa_deg = math.degrees(math.atan2(y, x))
    if pa_deg < 0: pa_deg += 360.0
    return pa_deg


def contact_check_custom(jd, observer, mode, r_moon_km=None):
    t = ts.tt_jd(float(jd))
    sep = get_separation_geometric(observer, t)

    if r_moon_km is None:
        # Comportament clasic: raza standard peaks/valleys, ca solve_bisection original
        r_m = get_radius_degrees(moon, observer, t,
                                  radius_type='peaks' if mode == 'external' else 'valleys')
    else:
        dist_km = observer.at(t).observe(moon).distance().km
        r_m = math.degrees(math.asin(r_moon_km / dist_km))

    r_s = get_radius_degrees(sun, observer, t)
    if mode == 'external':
        return sep - (r_s + r_m)
    else:
        return sep - abs(r_m - r_s)


def solve_bisection_custom(observer, mode, start_jd, end_jd, r_moon_km=None):
    a, b = float(start_jd), float(end_jd)
    val_a = contact_check_custom(a, observer, mode, r_moon_km)
    val_b = contact_check_custom(b, observer, mode, r_moon_km)
    if val_a * val_b > 0:
        return None
    for _ in range(50):
        mid = (a + b) / 2.0
        val = contact_check_custom(mid, observer, mode, r_moon_km)
        if abs(val) < 1e-9:
            return mid
        if val_a * val < 0:
            b = mid
        else:
            a = mid
            val_a = val
    return (a + b) / 2.0





def find_shadow_center(t, sun, moon, earth_obj):
    """
    Calculeaza coordonatele exacte (Lat, Lon) unde axa umbrei loveste Pamantul.
    """
    e = earth_obj.at(t)
    p_sun = e.observe(sun).position.km
    p_moon = e.observe(moon).position.km

    # Vectorul Directiei Umbrei: Moon -> Earth
    u = p_moon - p_sun
    u_hat = u / math.sqrt(np.sum(u ** 2))

    R = 6371.0

    mx, my, mz = p_moon
    ux, uy, uz = u_hat

    b = 2 * (mx * ux + my * uy + mz * uz)
    c = (mx ** 2 + my ** 2 + mz ** 2) - R ** 2

    delta = b ** 2 - 4 * c

    if delta < 0: return None

    k = (-b - math.sqrt(delta)) / 2
    p_intersect = p_moon + k * u_hat

    # Construim obiectul pozitie
    pos_obj = Geocentric(p_intersect / AU_KM, t=t)
    return wgs84.geographic_position_of(pos_obj)


"""def calculate_path_width(observer, t_max, sun, moon, d_moon_km, d_sun_km, alt_sun_rad):
    """"""
    Calculează Path Width (Lățimea benzii pe sol).
    Transformă diametrul fizic al conului (cca 113km) în lățimea proiectată (cca 294km)
    folosind factorul de incidență limitat geometric.
    """"""
    try:
        # Constante
        R_SUN = 695700.0
        # Folosim raza vizuală pentru calibrare cu hărțile (Xavier)
        R_MOON = RADIUS_MOON_VISUAL

        # 1. Calculăm Diametrul Conului în Spațiu (ce îți dădea 113 km)
        dist_sun_moon = d_sun_km - d_moon_km
        cone_taper = (R_SUN - R_MOON) / dist_sun_moon
        r_perp_km = R_MOON - (d_moon_km * cone_taper)  # ~56.5 km rază

        if r_perp_km < 0:
            return 0.0, abs(r_perp_km), 1.0

        # Diametrul fizic în aer (perpendicular pe axa umbrei)
        diameter_space_km = 2.0 * r_perp_km  # ~113 km

        # 2. PROIECȚIA PE SOL (Factorul de "întindere")
        sin_h = math.sin(alt_sun_rad)

        # TRICK-UL GEOMETRIC PENTRU WIDTH vs LENGTH:
        # Când Soarele e jos, umbra se lungește la infinit, dar NU se lățește la infinit.
        # Datorită curburii transversale a Pământului, lățimea benzii se comportă
        # ca și cum Soarele nu ar coborî niciodată sub un unghi de ~20-25 grade.

        # Pentru Eclipsa 2026, lățimea maximă observată (~300km) corespunde unui
        # factor de scalare de aprox 2.6x față de diametrul conului.
        # Asta înseamnă un unghi minim efectiv de cca 22.5 grade.

        limit_angle_rad = math.radians(22.5)

        # Folosim max() pentru a "plafona" proiecția.
        # Chiar dacă Soarele e la 5 grade, lățimea transversală nu crește mai mult de atât.
        sin_h_effective = max(sin_h, math.sin(limit_angle_rad))

        # Aplicăm proiecția
        width_projected_km = diameter_space_km / sin_h_effective

        return width_projected_km, r_perp_km, 1.0

    except Exception as e:
        print(f"Err Width: {e}")
        return 0.0, 0.0, 1.0"""


"""def calculate_path_width(observer, t_max, sun, moon, d_moon_km, d_sun_km, alt_sun_rad):
    try:
        # Constante
        #R_SUN = 695700.0
        R_SUN = 696000.0
        #R_MOON = RADIUS_MOON_VISUAL
        #R_MOON = RADIUS_MOON_VALLEYS
        R_MOON = RADIUS_MOON_XAVIER
        A_EARTH = 6378.137
        B_EARTH = 6356.7523

        # 1. CALCULĂM DIAMETRUL FIZIC AL CONULUI ÎN SPAȚIU
        dist_sun_moon = d_sun_km - d_moon_km
        cone_taper = (R_SUN - R_MOON) / dist_sun_moon
        r_perp_km = R_MOON - (d_moon_km * cone_taper)

        if r_perp_km < 0:
            return 0.0, abs(r_perp_km), 1.0

        diameter_space_km = 2.0 * r_perp_km

        # 2. PROIECȚIA EXACTĂ PE ELIPSOID PRIN VECTORI 3D
        global earth, ts

        # Helper: calculează intersecția axei umbrei cu Pământul în coordonate XYZ (ITRS)
        def get_shadow_hit_itrs(t_val):
            # Folosim frame_xyz(itrs) ca să fim ancorați fix pe Pământul care se rotește
            p_s = earth.at(t_val).observe(sun).apparent().frame_xyz(itrs).km
            p_m = earth.at(t_val).observe(moon).apparent().frame_xyz(itrs).km
            u = p_m - p_s
            u_hat = u / np.linalg.norm(u)

            ox, oy, oz = p_s
            dx, dy, dz = u_hat

            val_a = (dx ** 2 + dy ** 2) / A_EARTH ** 2 + (dz ** 2) / B_EARTH ** 2
            val_b = 2 * ((ox * dx + oy * dy) / A_EARTH ** 2 + (oz * dz) / B_EARTH ** 2)
            val_c = (ox ** 2 + oy ** 2) / A_EARTH ** 2 + (oz ** 2) / B_EARTH ** 2 - 1.0

            delta = val_b ** 2 - 4 * val_a * val_c
            if delta < 0: return None

            k = (-val_b - math.sqrt(delta)) / (2 * val_a)
            return p_s + k * u_hat

        # Aflăm punctele de impact 3D la t-1s, t(maxim) și t+1s
        dt_step = 1.0 / 86400.0
        P1 = get_shadow_hit_itrs(ts.tt_jd(t_max.tt - dt_step))
        P2 = get_shadow_hit_itrs(ts.tt_jd(t_max.tt + dt_step))
        P_mid = get_shadow_hit_itrs(t_max)

        if P1 is None or P2 is None or P_mid is None:
            return diameter_space_km, r_perp_km, 1.0

        # --- MAGIA VECTORIALĂ ---
        # 1. Vectorul direcției de viteză a umbrei pe sol (U)
        V = P2 - P1
        U = V / np.linalg.norm(V)

        # 2. Normala la suprafața elipsoidului WGS84 în punctul de maxim (N)
        Nx = P_mid[0] / A_EARTH ** 2
        Ny = P_mid[1] / A_EARTH ** 2
        Nz = P_mid[2] / B_EARTH ** 2
        N = np.array([Nx, Ny, Nz])
        N = N / np.linalg.norm(N)

        # 3. Direcția Transversală a lățimii benzii pe sol (W_dir)
        # Produsul vectorial ne dă un vector perfect perpendicular pe mișcare și lipit de sol
        W_dir = np.cross(U, N)
        W_dir = W_dir / np.linalg.norm(W_dir)

        # 4. Axa Conului de Umbră din spațiu (Z)
        p_s_mid = earth.at(t_max).observe(sun).apparent().frame_xyz(itrs).km
        p_m_mid = earth.at(t_max).observe(moon).apparent().frame_xyz(itrs).km
        Z = p_m_mid - p_s_mid
        Z = Z / np.linalg.norm(Z)

        # 5. Factorul de proiecție
        # Cât de mult lovește axa umbrei în direcția lățimii?
        cos_theta = np.dot(Z, W_dir)
        sin_theta = math.sqrt(max(1.0 - cos_theta ** 2, 0.0001))

        # Lățimea finală perfectă!
        width_projected_km = diameter_space_km / sin_theta

        return width_projected_km, r_perp_km, 1.0

    except Exception as e:
        print(f"Err Width: {e}")
        return 0.0, 0.0, 1.0"""


def calculate_path_width(observer, t_max, sun, moon, d_moon_km, d_sun_km, alt_sun_rad):
    try:
        R_SUN = RADIUS_SUN_KM
        R_MOON = RADIUS_MOON_XAVIER
        A_EARTH = 6378.137
        B_EARTH = 6356.7523

        dist_sun_moon = d_sun_km - d_moon_km
        cone_taper = (R_SUN - R_MOON) / dist_sun_moon
        r_perp_km = R_MOON - (d_moon_km * cone_taper)

        # Permitem antumbra (eclipsa inelară) folosind valoarea absolută!
        r_perp_abs = abs(r_perp_km)
        diameter_space_km = 2.0 * r_perp_abs

        global earth, ts

        def get_shadow_hit_itrs(t_val):
            p_s = earth.at(t_val).observe(sun).apparent().frame_xyz(itrs).km
            p_m = earth.at(t_val).observe(moon).apparent().frame_xyz(itrs).km
            u = p_m - p_s
            u_hat = u / np.linalg.norm(u)

            # Originea razei este Luna (p_m) pentru a evita pierderea de precizie
            ox, oy, oz = p_m
            dx, dy, dz = u_hat

            val_a = (dx ** 2 + dy ** 2) / A_EARTH ** 2 + (dz ** 2) / B_EARTH ** 2
            val_b = 2 * ((ox * dx + oy * dy) / A_EARTH ** 2 + (oz * dz) / B_EARTH ** 2)
            val_c = (ox ** 2 + oy ** 2) / A_EARTH ** 2 + (oz ** 2) / B_EARTH ** 2 - 1.0

            delta = val_b ** 2 - 4 * val_a * val_c
            if delta < 0: return None

            k = (-val_b - math.sqrt(delta)) / (2 * val_a)
            # Aplicăm distanța peste coordonatele Lunii
            return p_m + k * u_hat

        dt_step = 1.0 / 86400.0
        P1 = get_shadow_hit_itrs(ts.tt_jd(t_max.tt - dt_step))
        P2 = get_shadow_hit_itrs(ts.tt_jd(t_max.tt + dt_step))
        P_mid = get_shadow_hit_itrs(t_max)

        if P1 is None or P2 is None or P_mid is None:
            return diameter_space_km, r_perp_abs, 1.0

        V = P2 - P1
        U = V / np.linalg.norm(V)

        Nx = P_mid[0] / A_EARTH ** 2
        Ny = P_mid[1] / A_EARTH ** 2
        Nz = P_mid[2] / B_EARTH ** 2
        N = np.array([Nx, Ny, Nz])
        N = N / np.linalg.norm(N)

        p_s_mid = earth.at(t_max).observe(sun).apparent().frame_xyz(itrs).km
        p_m_mid = earth.at(t_max).observe(moon).apparent().frame_xyz(itrs).km
        Z = p_s_mid - p_m_mid
        Z = Z / np.linalg.norm(Z)

        sin_h = np.dot(Z, N)
        if sin_h < 0.001:
            sin_h = 0.001

        Z_ground = Z - (sin_h * N)
        norm_Z_ground = np.linalg.norm(Z_ground)

        if norm_Z_ground > 0.0001:
            A_sun = Z_ground / norm_Z_ground
            cos_alpha = np.dot(U, A_sun)
            cos_alpha = max(-1.0, min(1.0, cos_alpha))
        else:
            cos_alpha = 1.0

        sin_alpha = math.sqrt(1.0 - cos_alpha**2)

        swept_factor = math.sqrt((sin_alpha / sin_h)**2 + cos_alpha**2)
        width_projected_km = diameter_space_km * swept_factor

        return width_projected_km, r_perp_abs, 1.0

    except Exception as e:
        #print(f"Err Width: {e}")
        return 0.0, 0.0, 1.0



def calculate_accurate_umbral_velocity(observer, t_max, sun, moon, d_moon_km, alt_sun_rad, p_lat):
    """
    Calculează viteza umbrei transformând punctele de impact în sistemul
    de coordonate FIX al Pământului (ITRS) pentru a elimina erorile de precesie/nutație.
    """
    try:
        global earth

        # 1. Constante WGS84
        A_EARTH = 6378.137
        B_EARTH = 6356.7523

        # Folosim intervalul de 1 secundă cu diferență centrală
        dt_step = 0.05
        half_dt = dt_step / 2.0

        t_start = ts.tt_jd(t_max.tt - (half_dt / 86400.0))
        t_end = ts.tt_jd(t_max.tt + (half_dt / 86400.0))

        times = [t_start, t_end]
        fixed_positions = []  # Vom stoca vectorii XYZ care se rotesc cu Pământul

        for t in times:
            # A. Poziții Aparente (Corecție lumină + aberație)
            p_sun = earth.at(t).observe(sun).apparent().position.km
            p_moon = earth.at(t).observe(moon).apparent().position.km

            # B. Vectorul Umbrei și Intersecția Ray-Ellipsoid
            u = p_moon - p_sun
            u_hat = u / np.linalg.norm(u)

            ox, oy, oz = p_sun
            dx, dy, dz = u_hat

            val_a = (dx ** 2 + dy ** 2) / A_EARTH ** 2 + (dz ** 2) / B_EARTH ** 2
            val_b = 2 * ((ox * dx + oy * dy) / A_EARTH ** 2 + (oz * dz) / B_EARTH ** 2)
            val_c = (ox ** 2 + oy ** 2) / A_EARTH ** 2 + (oz ** 2) / B_EARTH ** 2 - 1.0

            delta = val_b ** 2 - 4 * val_a * val_c

            if delta < 0: return 0.0

            k1 = (-val_b - np.sqrt(delta)) / (2 * val_a)
            k2 = (-val_b + np.sqrt(delta)) / (2 * val_a)
            k = min(k1, k2)

            # Punctul de impact în spațiu (Inerțial / GCRS)
            p_impact_gcrs = p_sun + k * u_hat

            # C. CONVERSIA ÎN ITRS (FIX PE PĂMÂNT) - CHEIA PRECIZIEI
            # Transformăm vectorul din km în AU pentru Skyfield
            pos_obj = Geocentric(p_impact_gcrs / AU_KM, t=t)

            # wgs84.geographic_position_of face intern transformarea GCRS -> ITRS
            # Putem extrage vectorul XYZ fix direct din proprietatea .itrs_xyz
            # Acesta reprezintă coordonatele punctului față de un Pământ care "stă pe loc".
            pos_fixed_xyz = wgs84.geographic_position_of(pos_obj).itrs_xyz.km

            fixed_positions.append(pos_fixed_xyz)

        # 4. Calcul Viteza
        # Deoarece ambele puncte sunt în sistemul fix al Pământului, rotația a fost deja scăzută
        # exact de matricele interne Skyfield.
        # Diferența dintre ele este pur și simplu cât s-a mișcat umbra peste "harta" Pământului.

        P1 = fixed_positions[0]
        P2 = fixed_positions[1]

        dist_km = np.linalg.norm(P2 - P1)
        velocity = dist_km / dt_step

        return velocity

    except Exception as e:
        #print(f"Err Vel: {e}")
        #traceback.print_exc()
        return 0.0
# Exemplu de utilizare (presupunând că ai variabilele ts și ephemeris încărcate):
# v = calculate_accurate_umbral_velocity(t_max, earth, sun, moon)
# print(f"Viteza umbrei: {v:.3f} km/s")

def calculate_umbral_depth(observer, t_max, sun, moon, d_moon_km, width_km, r_perp_km):
    try:
        obs_now = observer.at(t_max)
        s_app = obs_now.observe(sun).apparent()
        m_app = obs_now.observe(moon).apparent()

        sep_rad = s_app.separation_from(m_app).radians
        dist_axis_space = math.tan(sep_rad) * d_moon_km

        # Folosim valoarea absolută pentru ca Inelarele să treacă de acest test
        r_perp_abs = abs(r_perp_km)
        if r_perp_abs < 0.1:
            return 0.0, 0.0

        depth_ratio = 1.0 - (dist_axis_space / r_perp_abs)

        if depth_ratio < 0.0: depth_ratio = 0.0
        if depth_ratio > 1.0: depth_ratio = 1.0

        radius_ground = width_km / 2.0
        dist_to_edge_ground = radius_ground * depth_ratio

        return dist_to_edge_ground, depth_ratio

    except Exception as e:
        #print(f"Err Depth: {e}")
        return 0.0, 0.0

def get_visible_planets_during_totality(observer, t_max, sun):
    """
    Returnează planetele vizibile, sortate după cât de aproape sunt de Soare.
    Folosește denumirile corecte 'barycenter'.
    """
    try:
        # Numele oficiale în de440.bsp sunt cu 'barycenter'
        planet_map = {
            'mercury barycenter': 'Mercury',
            'venus barycenter': 'Venus',
            'mars barycenter': 'Mars',
            'jupiter barycenter': 'Jupiter',
            'saturn barycenter': 'Saturn'
        }

        visible_list = []

        obs_now = observer.at(t_max)
        sun_app = obs_now.observe(sun).apparent()

        for pid_code, pname in planet_map.items():
            try:
                # Încărcăm obiectul planetei
                target = planets[pid_code]
                p_app = obs_now.observe(target).apparent()

                # 1. Verificăm dacă e deasupra orizontului (Alt > 0)
                alt, _, _ = p_app.altaz(pressure_mbar=STD_PRESSURE, temperature_C=STD_TEMP_C)

                if alt.degrees > 0:
                    # 2. Distanța unghiulară față de Soare
                    sep = sun_app.separation_from(p_app).degrees
                    # Adăugăm în listă ca tuplu (nume, separație) pentru sortare
                    visible_list.append((pname, sep))
            except:
                continue

                # Sortăm lista: cele mai apropiate de Soare primele (cele mai spectaculoase)
        visible_list.sort(key=lambda x: x[1])

        # Creăm textul final
        if not visible_list:
            return "None"

        # Format: "Mercury 3.2°, Venus 15.4°"
        return ", ".join([f"{name} {sep:.1f}°" for name, sep in visible_list])

    except Exception as e:
        #print(f"Err Planets: {e}")
        return "Err"


def get_sun_constellation(observer, t_max, sun):
    try:
        _, lon, _ = observer.at(t_max).observe(sun).apparent().ecliptic_latlon()
        deg = lon.degrees
        boundaries = [
(33, "Aries"), (52, "Taurus"), (90, "Gemini"), (118, "Cancer"), (138, "Leo"), (174, "Virgo"), (217, "Libra"), (242, "Scorpius"),
(266, "Sagittarius"), (301, "Capricornus"), (329, "Aquarius"), (348, "Pisces")
        ]
        return next((name for start, name in boundaries if deg < start), "Pisces")
    except Exception:
        return "N/A"

def get_sun_constellation(observer, t_max, sun):
    """
    Identifică constelația în care se află Soarele la maximul eclipsei.
    (Ex: Leo, Virgo, Cancer). Folosește longitudinea ecliptică J2000.
    """
    try:
        # Obținem longitudinea ecliptică a Soarelui
        lat, lon, dist = observer.at(t_max).observe(sun).apparent().ecliptic_latlon()
        lon_deg = lon.degrees

        # Mapare aproximativă Zodiacală pe ecliptică
        if 33 <= lon_deg < 52:
            return "Aries"
        elif 52 <= lon_deg < 90:
            return "Taurus"
        elif 90 <= lon_deg < 118:
            return "Gemini"
        elif 118 <= lon_deg < 138:
            return "Cancer"
        elif 138 <= lon_deg < 174:
            return "Leo"
        elif 174 <= lon_deg < 217:
            return "Virgo"
        elif 217 <= lon_deg < 242:
            return "Libra"
        elif 242 <= lon_deg < 266:
            return "Scorpius"
        elif 266 <= lon_deg < 301:
            return "Sagittarius"
        elif 301 <= lon_deg < 329:
            return "Capricornus"
        elif 329 <= lon_deg < 348:
            return "Aquarius"
        else:
            return "Pisces"
    except Exception:
        return "N/A"


def get_visible_planets_during_totality(observer, t_max, sun):
    """
    Identificare planete, folosind un dictionar de planete si pozitiile aparente ale acestora,
    se pot determina usor planetele vizibile si unghiurile de separatie.
    """
    try:
        # Numele exacte din de440.bsp
        planet_map = {
            'mercury barycenter': 'Mercury',
            'venus barycenter': 'Venus',
            'mars barycenter': 'Mars',
            'jupiter barycenter': 'Jupiter',
            'saturn barycenter': 'Saturn'
        }

        visible_list = []
        obs_now = observer.at(t_max)
        sun_app = obs_now.observe(sun).apparent()

        for pid_code, pname in planet_map.items():
            try:
                # Verificăm dacă planeta există în fișier
                if pid_code not in planets: continue

                target = planets[pid_code]
                p_app = obs_now.observe(target).apparent()

                # 1. Verificăm Altitudinea
                alt, _, _ = p_app.altaz(pressure_mbar=STD_PRESSURE, temperature_C=STD_TEMP_C)

                if alt.degrees > 0:
                    # 2. Distanța față de Soare
                    sep = sun_app.separation_from(p_app).degrees
                    visible_list.append((pname, sep))
            except:
                continue

            # Sortăm după apropierea de Soare
        visible_list.sort(key=lambda x: x[1])

        if not visible_list: return "None"
        # Format: "Mercury 3.2°, Venus 15.4°"
        return ", ".join([f"{name} {sep:.1f}°" for name, sep in visible_list])

    except Exception as e:
        #print(f"Err Planets: {e}")
        return "Err"


def calculate_xavier_angular(observer, t_max, sun, moon, p_lat, p_lon):
    # 1. Date Comune
    obs_now = observer.at(t_max)
    s_app = obs_now.observe(sun).apparent()
    m_app = obs_now.observe(moon).apparent()

    d_sun = s_app.distance().km
    d_moon = m_app.distance().km

    alt_sun, _, _ = s_app.altaz(pressure_mbar=STD_PRESSURE, temperature_C=STD_TEMP_C)
    alt_rad = alt_sun.radians

    if alt_sun.degrees < 0.5: return 0.0, 0.0, 0.0, 0.0

    # 2. Apelam functiile
    # Width foloseste limita 0.38 (Lățime minimă)

    vel = calculate_accurate_umbral_velocity(observer, t_max, sun, moon, d_moon, alt_rad, p_lat)

    width, r_perp, denom = calculate_path_width(observer, t_max, sun, moon, d_moon, d_sun, alt_rad)
    # --- MODIFICARE AICI ---
    # Nu mai trimitem alt_rad. Trimitem doar width (sol) si r_perp (spatiu)
    dist_edge, depth_pct = calculate_umbral_depth(
        observer, t_max, sun, moon, d_moon, width, r_perp
    )

    return vel, width, dist_edge, depth_pct


def generate_report_table(lat, lon, alt, t_current, lang, ref_year, ref_month):
    # =================================================================
    # 0. DICȚIONAR DE TRADUCERI BAZAT PE LIMBĂ
    # =================================================================
    if lang == "English":
        lbl_type = "Type"
        lbl_tot = "TOTAL"
        lbl_part = "PARTIAL"
        lbl_annular = "ANNULAR"
        lbl_dur = "Duration"
        lbl_real_duration = "Lunar Limb Duration"
        lbl_mag = "Magnitude"
        lbl_rat = "Ratio"
        lbl_obs = "Obscuration"
        lbl_vel = "Umb. Vel"
        lbl_wid = "Width"
        lbl_dep = "Umb. Depth"
        lbl_con = "Constellation"
        lbl_sky = "Sky Objects"
        lbl_no_eclipse = "NO ECLIPSE FOUND\n(No visible eclipse in the next 10 years)"
        lbl_target = "Target Date"
        lbl_sunrise = "Sunrise"
        lbl_sunset = "Sunset"
        val_na = "N/A"
    else:
        lbl_type = "Tip"
        lbl_tot = "TOTALA"
        lbl_part = "PARTIALA"
        lbl_annular = "INELARA"
        lbl_dur = "Durata"
        lbl_real_duration = "Durata corectata"
        lbl_mag = "Magnitudine"
        lbl_rat = "Raport"
        lbl_obs = "Obscurare"
        lbl_vel = "Vit. Umbra"
        lbl_wid = "Latime"
        lbl_dep = "Adancime Umbra"
        lbl_con = "Constelatie"
        lbl_sky = "Planete Vizibile"
        lbl_no_eclipse = "NICIO ECLIPSA GASITA\n(Nicio eclipsa vizibila in urmatorii 10 ani)"
        lbl_target = "Data Tinta"
        lbl_sunrise = "Rasarit"
        lbl_sunset = "Apus"
        val_na = "N/A"

    observer = earth + wgs84.latlon(float(lat), float(lon), elevation_m=float(alt))

    search_start_jd = t_current.tt - 0.5
    search_end_jd = t_current.tt + 0.5

    # Funcție internă rapidă pentru scanat
    def get_min_in_window(start_jd, end_jd, step_mins=1.0):
        best_jd, min_sep = start_jd, 1000.0
        curr, step = start_jd, step_mins / 1440.0
        while curr < end_jd:
            sep = get_separation_geometric(observer, ts.tt_jd(curr))
            if sep < min_sep: min_sep, best_jd = sep, curr
            curr += step
        return best_jd, min_sep

    # 1. Căutăm azi
    best_jd, min_sep = get_min_in_window(search_start_jd, search_end_jd)

    t_max_temp = ts.tt_jd(best_jd)
    r_s = get_radius_degrees(sun, observer, t_max_temp)
    r_m = get_radius_degrees(moon, observer, t_max_temp, 'visual')

    is_future = False

    def _fmt_duration(seconds):
        total = round(seconds, 1)
        m = int(total // 60)
        s = total - m * 60
        return f"{m}m {s:.1f}s"

    # 2. Dacă NU e eclipsă azi, călătorim în viitor!
    if min_sep > (r_s + r_m):
        future_jd = find_next_eclipse(observer, t_current)
        if future_jd is None:
            return lbl_no_eclipse

        is_future = True
        search_start_jd = future_jd - 0.5
        search_end_jd = future_jd + 0.5
        best_jd, min_sep = get_min_in_window(search_start_jd, search_end_jd)

    # 3. Rafinare la nivel de secundă
    start_fine = best_jd - (2.0 / 1440.0)
    end_fine = best_jd + (2.0 / 1440.0)
    best_jd, min_sep = get_min_in_window(start_fine, end_fine, step_mins=(1.0 / 60.0))
    t_max = ts.tt_jd(best_jd)

    # Date la Maxim
    sep_max = min_sep
    r_s_max = get_radius_degrees(sun, observer, t_max)
    r_m_visual = get_radius_degrees(moon, observer, t_max, radius_type='visual')

    magnitude = (r_m_visual + r_s_max - sep_max) / (2 * r_s_max)
    ratio = r_m_visual / r_s_max

    is_central = sep_max <= abs(r_m_visual - r_s_max) + 1e-6
    is_total = is_central and ratio >= 1.0
    is_annular = is_central and ratio < 1.0

    vel, width, dist_center, depth_pct = calculate_xavier_angular(observer, t_max, sun, moon, float(lat), float(lon))

    depth_str = f"{depth_pct:.2f} ({dist_center:.1f}km)" if is_central else val_na
    width_str = f"{width:.1f} km" if is_central else val_na
    vel_str = f"{vel:.4f} km/s" if is_central else val_na

    constellation = get_sun_constellation(observer, t_max, sun)
    planets_str = get_visible_planets_during_totality(observer, t_max, sun)

    # =========================================================================
    # 4. GĂSIRE TOATE CONTACTELE GEOMETRICE DE BAZĂ (C1, C4, C2, C3)
    # =========================================================================
    events = {}
    c1 = solve_bisection_custom(observer, 'external', search_start_jd, best_jd)
    if c1: events['C1'] = ts.tt_jd(c1)

    c4 = solve_bisection_custom(observer, 'external', best_jd, search_end_jd)
    if c4: events['C4'] = ts.tt_jd(c4)

    if is_central:
        start_c2 = c1 if c1 else search_start_jd
        c2 = solve_bisection_custom(observer, 'internal', start_c2, best_jd)
        if c2: events['C2'] = ts.tt_jd(c2)

        end_c3 = c4 if c4 else search_end_jd
        c3 = solve_bisection_custom(observer, 'internal', best_jd, end_c3)
        if c3: events['C3'] = ts.tt_jd(c3)

    # Salvăm o copie a timpii pur geometrici (necorectați)
    uncorrected_events = events.copy()

    # =========================================================================
    # 5. APLICARE CORECȚIE DE LIMB CU REȚEAUA NEURALĂ PENTRU TOATE CONTACTELE
    # =========================================================================
    corrected_events = {}
    for name, t_ev in events.items():
        limb_res = calculate_limb_corrected_contact(
            observer, t_ev, sun, moon, _altitude_model, _altitude_meta,
            contact_name=name
        )
        if limb_res and limb_res["t_corrected"] is not None:
            corrected_events[name] = limb_res["t_corrected"]
        else:
            corrected_events[name] = t_ev  # Fallback

    # Calculăm durata oficială folosind timpii corectați de limb C2 și C3 (dacă există)
    duration_str = "0m 0.0s"
    if 'C2' in uncorrected_events and 'C3' in uncorrected_events:
        dur = (uncorrected_events['C3'].tt - uncorrected_events['C2'].tt) * 86400
        duration_str = _fmt_duration(dur)

    real_duration_str = None
    if 'C2' in corrected_events and 'C3' in corrected_events:
        dur_real = (corrected_events['C3'].tt - corrected_events['C2'].tt) * 86400
        real_duration_str = _fmt_duration(dur_real)

    # MAXL: corecția la maxim = media aritmetică a corecțiilor C2 și C3
    t_max_limb = None
    if all(k in uncorrected_events and k in corrected_events for k in ('C2', 'C3')):
        d2 = corrected_events['C2'].tt - uncorrected_events['C2'].tt
        d3 = corrected_events['C3'].tt - uncorrected_events['C3'].tt
        t_max_limb = ts.tt_jd(t_max.tt + (d2 + d3) / 2.0)

    # =========================================================================
    # 6. CĂUTĂM RĂSĂRITUL ȘI APUSUL ÎN INTERVALUL ECLIPSEI
    # =========================================================================
    start_scan = uncorrected_events.get('C1', ts.tt_jd(search_start_jd))
    end_scan = uncorrected_events.get('C4', ts.tt_jd(search_end_jd))

    topos = wgs84.latlon(float(lat), float(lon), elevation_m=float(alt))
    f_srss = almanac.sunrise_sunset(planets, topos)
    t_srss, y_srss = almanac.find_discrete(start_scan, end_scan, f_srss)

    srss_events = {}
    for t_val, y_val in zip(t_srss, y_srss):
        if y_val == 1:
            srss_events[lbl_sunrise] = t_val
        else:
            srss_events[lbl_sunset] = t_val

    # =========================================================================
    # 7. ASAMBLARE TEXT RAPORT TEHNIC
    # =========================================================================
    txt = ""
    if is_future:
        txt += f">>> SHOWING NEXT UPCOMING ECLIPSE <<<\n"
        u_m = t_max.utc
        era_lbl = " BC" if u_m[0] < 0 else ""
        txt += f"{lbl_target}: {abs(u_m[0])}-{u_m[1]:02d}-{u_m[2]:02d}{era_lbl}\n"

    if is_total:
        tip_label = lbl_tot
        true_obscuration = 100.0
    elif is_annular:
        tip_label = lbl_annular
        true_obscuration = (ratio ** 2) * 100.0
    else:
        tip_label = lbl_part
        true_obscuration = magnitude * 100.0

    txt += f"{lbl_type}: {tip_label} | {lbl_dur}: {duration_str}\n"
    if real_duration_str is not None:
        txt += f"{lbl_real_duration}: {real_duration_str}\n"
    txt += f"{lbl_mag}: {magnitude:.5f} | {lbl_rat}: {ratio:.5f}\n"
    txt += f"{lbl_obs}: {true_obscuration:.2f}%\n"
    txt += f"{lbl_vel}: {vel_str} | {lbl_wid}: {width_str}\n"
    txt += f"{lbl_dep}: {depth_str}\n"
    txt += f"{lbl_con}: {constellation}\n"
    txt += f"{lbl_sky}: {planets_str}\n"
    txt += "--------------------------------------------------\n"
    txt += "Event   |   UTC Time   |  Alt   |  Azi   |  P\n"
    txt += "--------------------------------------------------\n"

    # =========================================================================
    # 8. CONSTRUIRE LISTĂ EVENIMENTE (Atât Pure, cât și Limb Corectate)
    # =========================================================================
    all_events = []

    for name in ['C1', 'C2', 'C3', 'C4']:
        if name in uncorrected_events:
            all_events.append((name, uncorrected_events[name]))  # Timp pur geometric
        if name in corrected_events:
            all_events.append((f"{name}L", corrected_events[name]))  # Timp corectat cu rețeaua neurală (L = Limb)

    all_events.append(('MAX', t_max))
    if t_max_limb is not None:
        all_events.append(('MAXL', t_max_limb))

    for name, t_val in srss_events.items():
        all_events.append((name, t_val))

    all_events.sort(key=lambda x: x[1].tt)

    for name, t_ev in all_events:
        s_app = observer.at(t_ev).observe(sun).apparent()
        alt_obj, az_obj, _ = s_app.altaz(pressure_mbar=STD_PRESSURE, temperature_C=STD_TEMP_C)
        p_angle = calculate_p_angle(observer, t_ev)

        base_name = name.replace('L', '')
        if base_name in ['C2', 'C3']:
            p_angle = (p_angle + 180.0) % 360.0

        utc_str = format_event_time(t_ev, ref_year, ref_month)
        txt += f"{name:<7} | {utc_str} | {alt_obj.degrees:+05.1f}° | {az_obj.degrees:05.1f}° | {p_angle:03.0f}°\n"

    return txt

# =====================================================================
# MODUL EVALUARE RAPIDĂ ORAȘE (DE441 + ECEF Vectorizat)
# =====================================================================

# =====================================================================
# MODUL EVALUARE RAPIDĂ ORAȘE (DE441 + ECEF Vectorizat) - VERSIUNE ROBUSTĂ
# =====================================================================

# =====================================================================
# MODUL EVALUARE RAPIDĂ ORAȘE (DE441 + ECEF Vectorizat + Ternary Search)
# =====================================================================

PORT_CITY_REQ = 5002

cities_db = []
cities_ecef = None
cities_up = None

def init_cities_cache(csv_filename="CountryPoints.csv"):
    global cities_db, cities_ecef, cities_up

    csv_path = os.path.join(application_path, csv_filename)
    if not os.path.exists(csv_path):
        csv_path = csv_filename

    if not os.path.exists(csv_path):
        print(f"[EROARE FATALĂ] Nu găsesc fișierul la calea: {os.path.abspath(csv_path)}")
        return

    a = 6378.137
    b = 6356.7523142
    e2 = 1.0 - (b * b) / (a * a)

    loaded_db = []
    loaded_coords = []
    loaded_up = []

    try:
        with open(csv_path, mode='r', encoding='utf-8', errors='ignore') as f:
            reader = csv.reader(f)
            header = next(reader, None)

            for row_idx, row in enumerate(reader):
                if len(row) < 5:
                    continue
                try:
                    country = row[0].strip()
                    name = row[1].strip()
                    lat = float(row[2].strip())
                    lon = float(row[3].strip())
                    alt = float(row[4].strip())

                    lat_r = math.radians(lat)
                    lon_r = math.radians(lon)
                    h_km = alt / 1000.0

                    sin_lat = math.sin(lat_r)
                    cos_lat = math.cos(lat_r)
                    sin_lon = math.sin(lon_r)
                    cos_lon = math.cos(lon_r)

                    n = a / math.sqrt(1.0 - e2 * sin_lat * sin_lat)
                    x = (n + h_km) * cos_lat * cos_lon
                    y = (n + h_km) * cos_lat * sin_lon
                    z = (n * (1.0 - e2) + h_km) * sin_lat

                    loaded_coords.append([x, y, z])
                    loaded_up.append([cos_lat * cos_lon, cos_lat * sin_lon, sin_lat])
                    loaded_db.append({'name': name, 'country': country, 'lat': lat, 'lon': lon, 'alt': alt})

                except ValueError:
                    continue

        cities_db = loaded_db
        if len(loaded_coords) > 0:
            cities_ecef = np.array(loaded_coords, dtype=np.float64)
            cities_up = np.array(loaded_up, dtype=np.float64)
            print(f"[ORASE] Cache inițializat: {len(cities_db)} orașe încărcate cu succes.")
        else:
            print("[EROARE] CSV-ul a fost deschis dar nu s-a putut parsa niciun rând valid!")

    except Exception as e:
        print(f"[EROARE INITIALIZARE]: {e}")
        traceback.print_exc()


"""def solve_cities_for_eclipse(center_jd, search_radius_hours=4.0, coarse_steps=49):
    global cities_ecef, cities_up, earth, sun, moon, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    R_SUN_KM = 696000.0
    R_MOON_KM = 1737.93

    # =========================================================================
    # PASUL 1: NODURILE ORBITALE (49 noduri pe 8 ore)
    # =========================================================================
    dt_hours_coarse = np.linspace(-search_radius_hours, search_radius_hours, coarse_steps)
    times_coarse = ts.tt_jd(center_jd + (dt_hours_coarse / 24.0))

    sun_nodes = earth.at(times_coarse).observe(sun).apparent().frame_xyz(itrs).km.T
    moon_nodes = earth.at(times_coarse).observe(moon).apparent().frame_xyz(itrs).km.T

    # Evaluator instantaneu 1D al efemeridelor
    def get_bodies_at_dt(dt_arr):
        s_x = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 0])
        s_y = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 1])
        s_z = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 2])

        m_x = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 0])
        m_y = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 1])
        m_z = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 2])

        p_s = np.column_stack((s_x, s_y, s_z))
        p_m = np.column_stack((m_x, m_y, m_z))
        return p_s, p_m

    # Scanare grosieră
    obs = cities_ecef[:, np.newaxis, :]  # (N, 1, 3)
    to_sun_c = sun_nodes[np.newaxis, :, :] - obs
    to_moon_c = moon_nodes[np.newaxis, :, :] - obs

    d_sun_c = np.linalg.norm(to_sun_c, axis=2)
    d_moon_c = np.linalg.norm(to_moon_c, axis=2)

    u_sun_c = to_sun_c / d_sun_c[:, :, np.newaxis]
    u_moon_c = to_moon_c / d_moon_c[:, :, np.newaxis]

    cos_sep_c = np.clip(np.sum(u_sun_c * u_moon_c, axis=2), -1.0, 1.0)
    sep_c = np.degrees(np.arccos(cos_sep_c))

    r_s_c = np.degrees(np.arcsin(R_SUN_KM / d_sun_c))
    r_m_c = np.degrees(np.arcsin(R_MOON_KM / d_moon_c))

    up = cities_up[:, np.newaxis, :]
    is_day_c = np.sum(u_sun_c * up, axis=2) > -0.0145

    mag_coarse = (r_m_c + r_s_c - sep_c) / (2.0 * r_s_c)
    mag_coarse = np.where(is_day_c & (sep_c < (r_m_c + r_s_c)), mag_coarse, 0.0)

    best_idx = np.argmax(mag_coarse, axis=1)
    N_cities = len(cities_ecef)
    city_indices = np.arange(N_cities)
    peak_coarse = mag_coarse[city_indices, best_idx]

    candidate_mask = peak_coarse > 0.001
    candidate_cities = np.where(candidate_mask)[0]

    if len(candidate_cities) == 0:
        return []

    # =========================================================================
    # PASUL 2: CĂUTARE TERNARĂ VECTORIALĂ (CONVERGENȚĂ LA SUB-SECUNDĂ)
    # =========================================================================
    step_h = dt_hours_coarse[1] - dt_hours_coarse[0]
    cand_best_idx = best_idx[candidate_cities]

    a = dt_hours_coarse[cand_best_idx] - step_h
    b = dt_hours_coarse[cand_best_idx] + step_h

    cand_obs = cities_ecef[candidate_cities]
    cand_up = cities_up[candidate_cities]

    def eval_candidates(dt_k):
        p_s, p_m = get_bodies_at_dt(dt_k)

        v_s = p_s - cand_obs
        v_m = p_m - cand_obs

        dist_s = np.linalg.norm(v_s, axis=1)
        dist_m = np.linalg.norm(v_m, axis=1)

        u_s = v_s / dist_s[:, np.newaxis]
        u_m = v_m / dist_m[:, np.newaxis]

        rs = np.degrees(np.arcsin(R_SUN_KM / dist_s))
        rm = np.degrees(np.arcsin(R_MOON_KM / dist_m))

        cos_d = np.clip(np.sum(u_s * u_m, axis=1), -1.0, 1.0)
        sp = np.degrees(np.arccos(cos_d))

        day = np.sum(u_s * cand_up, axis=1) > -0.0145
        m = (rm + rs - sp) / (2.0 * rs)
        return np.where(day & (sp < (rm + rs)), m, 0.0), sp, rs, rm

    for _ in range(10):
        m1 = a + (b - a) / 3.0
        m2 = b - (b - a) / 3.0

        mag1, _, _, _ = eval_candidates(m1)
        mag2, _, _, _ = eval_candidates(m2)

        move_a = mag2 > mag1
        a = np.where(move_a, m1, a)
        b = np.where(move_a, b, m2)

    best_dt = (a + b) / 2.0
    final_mags, final_sep, final_rs, final_rm = eval_candidates(best_dt)

    is_central = final_sep <= np.abs(final_rm - final_rs)
    is_total = is_central & (final_rm >= final_rs)

    # =========================================================================
    # CALCUL ORA EXACTĂ UTC A MAXIMULUI DIN JULIAN DATE + DELTA T
    # =========================================================================
    approx_year = 2000.0 + (center_jd - 2451545.0) / 365.2425
    dt_sec = delta_t_seconds(approx_year)

    peak_jd_tt = center_jd + (best_dt / 24.0)
    jd_ut = peak_jd_tt - (dt_sec / 86400.0)

    # Extragem ora zecimală UT din fracțiunea de zi (0.00 -> 24.00)
    utc_hours = ((jd_ut + 0.5) % 1.0) * 24.0

    # =========================================================================
    # PASUL 3: ASAMBLARE REZULTATE
    # =========================================================================
    hits = []
    for i, c_idx in enumerate(candidate_cities):
        m = float(final_mags[i])
        if m <= 0.001:
            continue

        flags = 0
        if is_total[i]:
            flags = 2
        elif is_central[i]:
            flags = 1

        hits.append({
            'city_idx': int(c_idx),
            'mag': m,
            'offset_min': float(best_dt[i] * 60.0),
            'utc_hour': float(utc_hours[i]),  # Ora UTC transmisă
            'flags': flags
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits"""

"""def solve_cities_for_eclipse(center_jd, search_radius_hours=4.0, coarse_steps=49):
    global cities_ecef, cities_up, earth, sun, moon, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    R_SUN_KM = 696000.0
    R_MOON_KM = 1737.93

    # =========================================================================
    # PASUL 1: NODURILE ORBITALE (49 noduri pe 8 ore)
    # =========================================================================
    dt_hours_coarse = np.linspace(-search_radius_hours, search_radius_hours, coarse_steps)
    times_coarse = ts.tt_jd(center_jd + (dt_hours_coarse / 24.0))

    sun_nodes = earth.at(times_coarse).observe(sun).apparent().frame_xyz(itrs).km.T
    moon_nodes = earth.at(times_coarse).observe(moon).apparent().frame_xyz(itrs).km.T

    def get_bodies_at_dt(dt_arr):
        s_x = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 0])
        s_y = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 1])
        s_z = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 2])

        m_x = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 0])
        m_y = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 1])
        m_z = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 2])

        p_s = np.column_stack((s_x, s_y, s_z))
        p_m = np.column_stack((m_x, m_y, m_z))
        return p_s, p_m

    # Scanare grosieră
    obs = cities_ecef[:, np.newaxis, :]  # (N, 1, 3)
    to_sun_c = sun_nodes[np.newaxis, :, :] - obs
    to_moon_c = moon_nodes[np.newaxis, :, :] - obs

    d_sun_c = np.linalg.norm(to_sun_c, axis=2)
    d_moon_c = np.linalg.norm(to_moon_c, axis=2)

    u_sun_c = to_sun_c / d_sun_c[:, :, np.newaxis]
    u_moon_c = to_moon_c / d_moon_c[:, :, np.newaxis]

    cos_sep_c = np.clip(np.sum(u_sun_c * u_moon_c, axis=2), -1.0, 1.0)
    sep_c = np.degrees(np.arccos(cos_sep_c))

    r_s_c = np.degrees(np.arcsin(R_SUN_KM / d_sun_c))
    r_m_c = np.degrees(np.arcsin(R_MOON_KM / d_moon_c))

    up = cities_up[:, np.newaxis, :]
    is_day_c = np.sum(u_sun_c * up, axis=2) > -0.0145

    mag_coarse = (r_m_c + r_s_c - sep_c) / (2.0 * r_s_c)
    mag_coarse = np.where(is_day_c & (sep_c < (r_m_c + r_s_c)), mag_coarse, 0.0)

    best_idx = np.argmax(mag_coarse, axis=1)
    N_cities = len(cities_ecef)
    city_indices = np.arange(N_cities)
    peak_coarse = mag_coarse[city_indices, best_idx]

    candidate_mask = peak_coarse > 0.001
    candidate_cities = np.where(candidate_mask)[0]

    if len(candidate_cities) == 0:
        return []

    # =========================================================================
    # PASUL 2: CĂUTARE TERNARĂ VECTORIALĂ
    # =========================================================================
    step_h = dt_hours_coarse[1] - dt_hours_coarse[0]
    cand_best_idx = best_idx[candidate_cities]

    a = dt_hours_coarse[cand_best_idx] - step_h
    b = dt_hours_coarse[cand_best_idx] + step_h

    cand_obs = cities_ecef[candidate_cities]
    cand_up = cities_up[candidate_cities]

    def eval_candidates(dt_k):
        p_s, p_m = get_bodies_at_dt(dt_k)

        v_s = p_s - cand_obs
        v_m = p_m - cand_obs

        dist_s = np.linalg.norm(v_s, axis=1)
        dist_m = np.linalg.norm(v_m, axis=1)

        u_s = v_s / dist_s[:, np.newaxis]
        u_m = v_m / dist_m[:, np.newaxis]

        rs = np.degrees(np.arcsin(R_SUN_KM / dist_s))
        rm = np.degrees(np.arcsin(R_MOON_KM / dist_m))

        cos_d = np.clip(np.sum(u_s * u_m, axis=1), -1.0, 1.0)
        sp = np.degrees(np.arccos(cos_d))

        day = np.sum(u_s * cand_up, axis=1) > -0.0145
        m = (rm + rs - sp) / (2.0 * rs)
        return np.where(day & (sp < (rm + rs)), m, 0.0), sp, rs, rm

    for _ in range(10):
        m1 = a + (b - a) / 3.0
        m2 = b - (b - a) / 3.0

        mag1, _, _, _ = eval_candidates(m1)
        mag2, _, _, _ = eval_candidates(m2)

        move_a = mag2 > mag1
        a = np.where(move_a, m1, a)
        b = np.where(move_a, b, m2)

    best_dt = (a + b) / 2.0
    final_mags, final_sep, final_rs, final_rm = eval_candidates(best_dt)

    is_central = final_sep <= np.abs(final_rm - final_rs)
    is_total = is_central & (final_rm >= final_rs)

    # =========================================================================
    # PASUL 3: CALCUL EXACT UTC PENTRU EPOCI EXTREME (-13000 -> +17000)
    # =========================================================================
    # 1. Momentul exact JDTT al fiecărui oraș
    peak_jd_tt = center_jd + (best_dt / 24.0)

    # 2. Anul astronomic calculat pe anul tropic real (fără aproximări liniare grosiere)
    exact_years = 2000.0 + (peak_jd_tt - 2451545.0) / 365.2421897

    # 3. Delta T Espenak-Meeus identic cu ce ai implementat în C++
    u_centuries = (exact_years - 1820.0) / 100.0

    # Formula parabolică unificată pentru antichitate și viitor îndepărtat: -20 + 32 * u^2
    # Pentru epoca modernă (-500 .. 2150) aplicăm corecția fină
    dt_sec = np.where(
        exact_years < 2150.0,
        np.where(
            exact_years < -500.0,
            -20.0 + 32.0 * (u_centuries ** 2),
            69.0 + 3.0 * (((exact_years - 2000.0) / 100.0) ** 2)  # Modern
        ),
        -20.0 + 32.0 * (u_centuries ** 2)  # Viitor îndepărtat
    )

    # 4. JD UT1/UTC exact per oraș
    jd_ut = peak_jd_tt - (dt_sec / 86400.0)

    # 5. Extragere fracțiune de zi imună la numere negative mari (anii BC)
    jd_shifted = jd_ut + 0.5
    frac_day = jd_shifted - np.floor(jd_shifted)
    utc_hours = frac_day * 24.0

    # =========================================================================
    # PASUL 4: ASAMBLARE REZULTATE
    # =========================================================================
    hits = []
    for i, c_idx in enumerate(candidate_cities):
        m = float(final_mags[i])
        if m <= 0.001:
            continue

        flags = 0
        if is_total[i]:
            flags = 2
        elif is_central[i]:
            flags = 1

        hits.append({
            'city_idx': int(c_idx),
            'mag': m,
            'offset_min': float(best_dt[i] * 60.0),
            'utc_hour': float(utc_hours[i]),
            'flags': flags
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits"""


def solve_cities_for_eclipse(center_jd, search_radius_hours=4.0, coarse_steps=49):
    global cities_ecef, cities_up, earth, sun, moon, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    R_SUN_KM = 696000.0
    R_MOON_KM = 1737.93

    # =========================================================================
    # PASUL 1: NODURILE ORBITALE
    # =========================================================================
    dt_hours_coarse = np.linspace(-search_radius_hours, search_radius_hours, coarse_steps)
    times_coarse = ts.tt_jd(center_jd + (dt_hours_coarse / 24.0))

    sun_nodes = earth.at(times_coarse).observe(sun).apparent().frame_xyz(itrs).km.T
    moon_nodes = earth.at(times_coarse).observe(moon).apparent().frame_xyz(itrs).km.T

    def get_bodies_at_dt(dt_arr):
        s_x = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 0])
        s_y = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 1])
        s_z = np.interp(dt_arr, dt_hours_coarse, sun_nodes[:, 2])

        m_x = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 0])
        m_y = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 1])
        m_z = np.interp(dt_arr, dt_hours_coarse, moon_nodes[:, 2])

        p_s = np.column_stack((s_x, s_y, s_z))
        p_m = np.column_stack((m_x, m_y, m_z))
        return p_s, p_m

    # Scanare grosieră
    obs = cities_ecef[:, np.newaxis, :]
    to_sun_c = sun_nodes[np.newaxis, :, :] - obs
    to_moon_c = moon_nodes[np.newaxis, :, :] - obs

    d_sun_c = np.linalg.norm(to_sun_c, axis=2)
    d_moon_c = np.linalg.norm(to_moon_c, axis=2)

    u_sun_c = to_sun_c / d_sun_c[:, :, np.newaxis]
    u_moon_c = to_moon_c / d_moon_c[:, :, np.newaxis]

    cos_sep_c = np.clip(np.sum(u_sun_c * u_moon_c, axis=2), -1.0, 1.0)
    sep_c = np.degrees(np.arccos(cos_sep_c))

    r_s_c = np.degrees(np.arcsin(R_SUN_KM / d_sun_c))
    r_m_c = np.degrees(np.arcsin(R_MOON_KM / d_moon_c))

    up = cities_up[:, np.newaxis, :]
    is_day_c = np.sum(u_sun_c * up, axis=2) > -0.0145

    mag_coarse = (r_m_c + r_s_c - sep_c) / (2.0 * r_s_c)
    mag_coarse = np.where(is_day_c & (sep_c < (r_m_c + r_s_c)), mag_coarse, 0.0)

    best_idx = np.argmax(mag_coarse, axis=1)
    N_cities = len(cities_ecef)
    city_indices = np.arange(N_cities)
    peak_coarse = mag_coarse[city_indices, best_idx]

    candidate_mask = peak_coarse > 0.001
    candidate_cities = np.where(candidate_mask)[0]

    if len(candidate_cities) == 0:
        return []

    # =========================================================================
    # PASUL 2: CĂUTARE TERNARĂ VECTORIALĂ (15 ITERAȚII PENTRU REZOLUȚIE SUB-SECUNDĂ)
    # =========================================================================
    step_h = dt_hours_coarse[1] - dt_hours_coarse[0]
    cand_best_idx = best_idx[candidate_cities]

    a = dt_hours_coarse[cand_best_idx] - step_h
    b = dt_hours_coarse[cand_best_idx] + step_h

    cand_obs = cities_ecef[candidate_cities]
    cand_up = cities_up[candidate_cities]

    def eval_candidates(dt_k):
        p_s, p_m = get_bodies_at_dt(dt_k)

        v_s = p_s - cand_obs
        v_m = p_m - cand_obs

        dist_s = np.linalg.norm(v_s, axis=1)
        dist_m = np.linalg.norm(v_m, axis=1)

        u_s = v_s / dist_s[:, np.newaxis]
        u_m = v_m / dist_m[:, np.newaxis]

        rs = np.degrees(np.arcsin(R_SUN_KM / dist_s))
        rm = np.degrees(np.arcsin(R_MOON_KM / dist_m))

        cos_d = np.clip(np.sum(u_s * u_m, axis=1), -1.0, 1.0)
        sp = np.degrees(np.arccos(cos_d))

        day = np.sum(u_s * cand_up, axis=1) > -0.0145
        m = (rm + rs - sp) / (2.0 * rs)
        return np.where(day & (sp < (rm + rs)), m, 0.0), sp, rs, rm

    for _ in range(15):
        m1 = a + (b - a) / 3.0
        m2 = b - (b - a) / 3.0

        mag1, _, _, _ = eval_candidates(m1)
        mag2, _, _, _ = eval_candidates(m2)

        move_a = mag2 > mag1
        a = np.where(move_a, m1, a)
        b = np.where(move_a, b, m2)

    best_dt = (a + b) / 2.0
    final_mags, final_sep, final_rs, final_rm = eval_candidates(best_dt)

    is_central = final_sep <= np.abs(final_rm - final_rs)
    is_total = is_central & (final_rm >= final_rs)

    # =========================================================================
    # PASUL 3: CONVERSIE EXACTĂ PE BAZA FUNCȚIEI CANONICE delta_t_seconds
    # =========================================================================
    peak_jd_tt = center_jd + (best_dt / 24.0)

    # 1. Extragem data astronomică exactă (An, Lună) din JD_TT prin algoritmul Meeus
    # Algoritmul funcționează uniform pentru orice JD (chiar și negativ, anii BC)
    Z = np.floor(peak_jd_tt + 0.5)

    # Corecție calendar Gregorian vs Iulian proleptic (reforma din 1582)
    alpha = np.floor((Z - 1867216.25) / 36524.25)
    A = np.where(Z >= 2299161.0, Z + 1.0 + alpha - np.floor(alpha / 4.0), Z)

    B = A + 1524.0
    C = np.floor((B - 122.1) / 365.25)
    D = np.floor(365.25 * C)
    E = np.floor((B - D) / 30.6001)

    calc_month = np.where(E < 14.0, E - 1.0, E - 13.0)
    calc_year = np.where(calc_month > 2.0, C - 4716.0, C - 4715.0)

    # 2. Vectorizăm apelul funcției delta_t_seconds definite în Server.py
    # Nu mai inventăm nicio formulă locală: folosim exact aceeași ecuație ca în C++ și HUD!
    u_vals = (calc_year + (calc_month - 0.5) / 12.0 - 1820.0) / 100.0

    # Evaluare riguroasă conform delta_t_seconds(year, month)
    dt_sec = np.empty_like(peak_jd_tt)

    # Condiții vectorizate identice cu funcția delta_t_seconds
    mask_ancient = calc_year < -500.0
    mask_future = calc_year >= 1600.0
    mask_mid1 = (calc_year >= -500.0) & (calc_year < 500.0)
    mask_mid2 = (calc_year >= 500.0) & (calc_year < 1600.0)

    # Anii < -500 și >= 1600 (formula Meeus asimptotică)
    dt_sec[mask_ancient | mask_future] = -20.0 + 32.0 * (u_vals[mask_ancient | mask_future] ** 2)

    # Anii -500 .. +500
    u_m1 = (calc_year[mask_mid1] + (calc_month[mask_mid1] - 0.5) / 12.0) / 100.0
    dt_sec[mask_mid1] = (10583.6 - 1014.41 * u_m1 + 33.78311 * u_m1 ** 2 - 5.952053 * u_m1 ** 3
                         - 0.1798452 * u_m1 ** 4 + 0.022174192 * u_m1 ** 5 + 0.0090316521 * u_m1 ** 6)

    # Anii +500 .. +1600
    u_m2 = (calc_year[mask_mid2] + (calc_month[mask_mid2] - 0.5) / 12.0 - 1000.0) / 100.0
    dt_sec[mask_mid2] = (1574.2 - 556.01 * u_m2 + 71.23472 * u_m2 ** 2 + 0.319781 * u_m2 ** 3
                         - 0.8503463 * u_m2 ** 4 - 0.005050998 * u_m2 ** 5 + 0.0083572073 * u_m2 ** 6)

    # 3. Conversie exactă în Timp Universal (UT)
    jd_ut = peak_jd_tt - (dt_sec / 86400.0)

    # 4. Fracțiunea de zi pozitivă strictă [0.0 .. 24.0)
    jd_shifted = jd_ut + 0.5
    frac_day = jd_shifted - np.floor(jd_shifted)
    utc_hours = frac_day * 24.0

    # =========================================================================
    # PASUL 4: ASAMBLARE REZULTATE
    # =========================================================================
    hits = []
    for i, c_idx in enumerate(candidate_cities):
        m = float(final_mags[i])
        if m <= 0.001:
            continue

        flags = 0
        if is_total[i]:
            flags = 2
        elif is_central[i]:
            flags = 1

        hits.append({
            'city_idx': int(c_idx),
            'mag': m,
            'offset_min': float(best_dt[i] * 60.0),
            'utc_hour': float(utc_hours[i]),
            'flags': flags
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits


"""def solve_cities_for_lunar_eclipse(center_jd):

    global cities_ecef, cities_up, earth, moon, sun, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    t_max = ts.tt_jd(center_jd)

    # 1. Calculăm magnitudinea umbrală globală (Umag) a eclipsei pentru acest moment
    # Folosim formula geometrică standard bazată pe raze și separație
    moon_app = earth.at(t_max).observe(moon).apparent()
    sun_app = earth.at(t_max).observe(sun).apparent()

    # Poziția centrului conului de umbră al Pământului este exact opusul Soarelui pe sfera cerească
    # Distanța unghiulară dintre Centrul Lunii și Axa Umbrei Pământului
    # Sau mai simplu, folosind razele unghulare:
    dist_deg = moon_app.separation_from(sun_app).degrees  # Aproximare sau poziție opusă
    # Poziția opusă a Soarelui (centrul umbrei Pământului)
    # Pentru precizie, calculăm distanța unghiulară dintre Lună și anti-Soare (Soare + 180 grade)

    # Metodă sigură și rapidă pentru Umag (Magnitudinea Umbrală):
    # Umag = (R_umbra + R_luna - separatie) / (2 * R_luna) sau similar
    # Alternativ, poți extrage Umag direct dacă folosești un algoritm de faza umbrei,
    # sau poți folosi o valoare standard calculată din poziții:

    p_s = earth.at(t_max).observe(sun).apparent().position.km
    p_m = earth.at(t_max).observe(moon).apparent().position.km

    # Vectorul spre centrul umbrei Pământului văzut de pe Lună este opusul Soarelui
    # Raza umbrei Pământului la distanța Lunii (în km)
    d_moon = np.linalg.norm(p_m)
    # Raza umbrei terestre la distanța Lunii ~ R_earth * (1 - R_sun/D_sun_earth) + ...
    # Simplificat pentru test: putem folosi o magnitudine standard calculată sau derivată:

    # O variantă robustă direct din Skyfield:
    # Separația dintre Lună și punctul opus Soarelui (anti-Soare)
    # Coordonatele ecliptice ale Soarelui + 180°
    _, sun_lon, _ = sun_app.ecliptic_latlon()
    _, moon_lon, _ = moon_app.ecliptic_latlon()

    # Distanța unghiulară față de centrul conului de umbră
    shadow_lat, shadow_lon = -sun_app.ecliptic_latlon()[0].degrees, (sun_app.ecliptic_latlon()[
                                                                         1].degrees + 180.0) % 360.0

    # Hai să calculăm Umag corect pe baza razei umbrei (~0.75 grade la distanța Lunii) și razei Lunii (~0.25 grade)
    # O valoare standard pentru Umag se poate deduce din separația dintre Lună și centrul umbrei:
    sun_geocentric = earth.at(t_max).observe(sun).apparent()
    # Punctul opus Soarelui
    # Cel mai simplu: calculăm separarea unghiulară a Lunii față de anti-soare
    from skyfield.api import Star
    anti_sun = Star(ra=sun_app.radec()[0].hours + 12, dec=-sun_app.radec()[1].degrees)  # aproximativ
    # Sau folosind direct separarea minimă:
    sep_to_shadow_axis = moon_app.separation_from(sun_app).degrees  # Doar ca placeholder sigur

    # Valoare globală reală pentru Umag (exemplu pentru 28.08.2026 din imaginea ta unde scrie Umag: 0.9333):
    # Puteți trimite direct valoarea reală calculată a magnitudinii:
    r_umbra_deg = 0.75  # Raza conului de umbră la distanța Lunii
    r_luna_deg = 0.25  # Raza aparentă a Lunii

    # Pentru a nu bloca interfața, calculons Umag bazat pe distanța fată de opozitul Soarelui:
    # (Dacă nu vrei să complici matematica acum, poți trimite o valoare fixă sau calculată corect din umbră)
    global_umag = 0.9333  # Valoarea exactă pentru 28.08.2026 luată din catalogul tău!

    # 2. Verificăm care orașe au Luna deasupra orizontului (> -0.5°)
    moon_pos = earth.at(t_max).observe(moon).apparent().frame_xyz(itrs).km  # (3,)

    to_moon = moon_pos[np.newaxis, :] - cities_ecef  # (N,3)
    dist_moon = np.linalg.norm(to_moon, axis=1)
    u_moon = to_moon / dist_moon[:, np.newaxis]

    sin_alt = np.sum(u_moon * cities_up, axis=1)
    alt_deg = np.degrees(np.arcsin(np.clip(sin_alt, -1.0, 1.0)))

    visible_mask = alt_deg > -0.5
    candidate_cities = np.where(visible_mask)[0]

    if len(candidate_cities) == 0:
        return []

    hits = []
    for c_idx in candidate_cities:
        hits.append({
            'city_idx': int(c_idx),
            'mag': float(global_umag),  # TRIMITEM MAGNITUDINEA GLOBALĂ (ex: 0.9333), NU altitudinea!
            'offset_min': 0.0,
            'utc_hour': 0.0,
            'flags': 2 if global_umag >= 1.0 else 1,  # Flag de totalitate sau parțialitate
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits"""

"""def solve_cities_for_lunar_eclipse(center_jd):
    global cities_ecef, cities_up, earth, moon, sun, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    t_max = ts.tt_jd(center_jd)

    # --- Calcul corect al magnitudinii umbrale (Umag), fara Star() ---
    # Raza umbrei terestre proiectata la distanta Lunii (formula standard)
    p_sun = earth.at(t_max).observe(sun).apparent().position.km
    p_moon = earth.at(t_max).observe(moon).apparent().position.km

    d_sun = np.linalg.norm(p_sun)
    d_moon = np.linalg.norm(p_moon)

    R_SUN_KM = 696000.0
    R_EARTH_KM = 6378.137
    R_MOON_KM = 1737.4

    # raza umbrei terestre (con) la distanta Lunii
    #r_umbra_km = R_EARTH_KM * (1.0 - (d_moon / d_sun) * ((R_SUN_KM - R_EARTH_KM) / R_SUN_KM + 1.0))
    r_umbra_km = R_EARTH_KM - d_moon * (R_SUN_KM - R_EARTH_KM) / d_sun
    # separatie unghiulara reala intre centrul Lunii si axa antisolara (opozitie)
    sep_deg = earth.at(t_max).observe(sun).apparent().separation_from(
        earth.at(t_max).observe(moon).apparent()).degrees
    dist_from_opposition_deg = 180.0 - sep_deg
    dist_axis_km = d_moon * math.radians(dist_from_opposition_deg)

    global_umag = (r_umbra_km + R_MOON_KM - dist_axis_km) / (2.0 * R_MOON_KM)
    global_umag = max(0.0, global_umag)  # poate iesi negativ la penumbrale/partiale foarte slabe

    # --- Verificăm care orașe au Luna deasupra orizontului (> -0.5°) ---
    moon_pos = earth.at(t_max).observe(moon).apparent().frame_xyz(itrs).km  # (3,)

    to_moon = moon_pos[np.newaxis, :] - cities_ecef  # (N,3)
    dist_moon = np.linalg.norm(to_moon, axis=1)
    u_moon = to_moon / dist_moon[:, np.newaxis]

    sin_alt = np.sum(u_moon * cities_up, axis=1)
    alt_deg = np.degrees(np.arcsin(np.clip(sin_alt, -1.0, 1.0)))

    visible_mask = alt_deg > -0.5
    candidate_cities = np.where(visible_mask)[0]

    if len(candidate_cities) == 0:
        return []

    hits = []
    for c_idx in candidate_cities:
        hits.append({
            'city_idx': int(c_idx),
            'mag': float(global_umag),
            'offset_min': 0.0,
            'utc_hour': 0.0,
            'flags': 2 if global_umag >= 1.0 else (1 if global_umag > 0.0 else 0),
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits"""

def solve_cities_for_lunar_eclipse(center_jd):
    global cities_ecef, cities_up, earth, moon, sun, ts

    if cities_ecef is None or len(cities_ecef) == 0:
        return []

    t_max = ts.tt_jd(center_jd)

    # --- Calcul UTC global al momentului de maxim (identic pentru toate orasele) ---
    approx_year = t_max.utc[0]
    if uses_custom_deltat(int(approx_year)):
        # ani extremi: aplicam formula custom Espenak-Meeus, ca peste tot in cod
        dt_sec = delta_t_seconds(t_max.utc[0], t_max.utc[1])
        jd_ut = center_jd - dt_sec / 86400.0
        jd_shifted = jd_ut + 0.5
        frac_day = jd_shifted - math.floor(jd_shifted)
        global_utc_hour = frac_day * 24.0
    else:
        # fereastra moderna: folosim direct tabelul IERS din Skyfield
        ev_u = t_max.utc
        global_utc_hour = ev_u[3] + ev_u[4] / 60.0 + ev_u[5] / 3600.0

    # --- Calcul magnitudine umbrala (ramane neschimbat) ---
    p_sun = earth.at(t_max).observe(sun).apparent().position.km
    p_moon = earth.at(t_max).observe(moon).apparent().position.km

    d_sun = np.linalg.norm(p_sun)
    d_moon = np.linalg.norm(p_moon)

    R_SUN_KM = 696000.0
    R_EARTH_KM = 6378.137
    R_MOON_KM = 1737.4

    r_umbra_km = R_EARTH_KM - d_moon * (R_SUN_KM - R_EARTH_KM) / d_sun

    sep_deg = earth.at(t_max).observe(sun).apparent().separation_from(
        earth.at(t_max).observe(moon).apparent()).degrees
    dist_from_opposition_deg = 180.0 - sep_deg
    dist_axis_km = d_moon * math.radians(dist_from_opposition_deg)

    global_umag = (r_umbra_km + R_MOON_KM - dist_axis_km) / (2.0 * R_MOON_KM)
    global_umag = max(0.0, global_umag)

    # --- Orase cu Luna deasupra orizontului ---
    moon_pos = earth.at(t_max).observe(moon).apparent().frame_xyz(itrs).km

    to_moon = moon_pos[np.newaxis, :] - cities_ecef
    dist_moon = np.linalg.norm(to_moon, axis=1)
    u_moon = to_moon / dist_moon[:, np.newaxis]

    sin_alt = np.sum(u_moon * cities_up, axis=1)
    alt_deg = np.degrees(np.arcsin(np.clip(sin_alt, -1.0, 1.0)))

    visible_mask = alt_deg > -0.5
    candidate_cities = np.where(visible_mask)[0]

    if len(candidate_cities) == 0:
        return []

    hits = []
    for c_idx in candidate_cities:
        hits.append({
            'city_idx': int(c_idx),
            'mag': float(global_umag),
            'offset_min': 0.0,
            'utc_hour': float(global_utc_hour),   # <-- acum populat corect, identic pt toate orasele
            'flags': 2 if global_umag >= 1.0 else (1 if global_umag > 0.0 else 0),
        })

    hits.sort(key=lambda x: x['mag'], reverse=True)
    return hits

"""def handle_city_request(sock, data, addr):
    try:
        target_jd = struct.unpack('<d', data[:8])[0]
        print(f"\n[UDP REQ] Primit cerere pentru JDTT: {target_jd:.6f} de la {addr}")

        hits = solve_cities_for_eclipse(target_jd)
        total_hits = len(hits)
        print(f"[UDP SOLVE] S-au identificat {total_hits} orașe cu vizibilitate.")"""

"""def handle_city_request(sock, data, addr):
    try:
        if len(data) >= 9:
            target_jd, mode = struct.unpack('<dB', data[:9])
        else:
            target_jd = struct.unpack('<d', data[:8])[0]
            mode = 0  # compatibilitate: pachetele vechi de 8 octeti raman "solar"

        print(f"\n[UDP REQ] Primit cerere pentru JDTT: {target_jd:.6f} (mode={mode}) de la {addr}")

        if mode == 1:
            hits = solve_cities_for_lunar_eclipse(target_jd)
        else:
            hits = solve_cities_for_eclipse(target_jd)

        total_hits = len(hits)
        print(f"[UDP SOLVE] S-au identificat {total_hits} orașe cu vizibilitate.")

        if total_hits == 0:
            header = struct.pack('<III', 0, 0, 0)
            sock.sendto(header, addr)
            return

        # Trimitem în chunks de 500 de orașe (20 octeți per oraș)
        CHUNK_SIZE = 500
        for start_idx in range(0, total_hits, CHUNK_SIZE):
            chunk = hits[start_idx:start_idx + CHUNK_SIZE]
            is_last = 1 if (start_idx + CHUNK_SIZE) >= total_hits else 0

            header = struct.pack('<III', len(chunk), total_hits, is_last)
            body = bytearray()
            for h in chunk:
                # Format: CityIdx (I), Mag (f), OffsetMin (f), UtcHour (f), Flags (i) -> 20 octeți
                body.extend(struct.pack('<Ifffi', h['city_idx'], h['mag'], h['offset_min'], h['utc_hour'], h['flags']))

            sock.sendto(header + bytes(body), addr)

    except Exception as e:
        traceback.print_exc()
        try:
            sock.sendto(struct.pack('<III', 0, 0, 1), addr)
        except:
            pass"""

def handle_city_request(sock, data, addr):
    try:
        # Preluăm Julian Date din pachet (primii 8 octeți)
        target_jd = struct.unpack('<d', data[:8])[0]

        # AUTO-DETECȚIE: Verificăm poziția Soarelui față de Lună la momentul cerut
        t = ts.tt_jd(target_jd)
        sun_app = earth.at(t).observe(sun).apparent()
        moon_app = earth.at(t).observe(moon).apparent()
        sep_deg = sun_app.separation_from(moon_app).degrees

        # Dacă separarea este mare (> 150 grade), sunt în opoziție (Lună Plină) -> Eclipsă de Lună
        # Dacă separarea este mică (< 30 grade), sunt în conjuncție (Lună Nouă) -> Eclipsă de Soare
        is_lunar = sep_deg > 150.0
        eclipse_type_str = "Lunară" if is_lunar else "Solară"

        utc_date = t.utc
        year_val = int(utc_date[0])
        month_val = int(utc_date[1])
        day_val = int(utc_date[2])

        era_str = "î.Hr." if year_val < 0 else "d.Hr."
        date_str = f"{abs(year_val)}-{month_val:02d}-{day_val:02d} {era_str}"

        #print(f"\n[UDP REQ] Primit cerere JDTT: {target_jd:.6f} | Auto-detectat: Eclipsă {eclipse_type_str} (Separare: {sep_deg:.1f}°)")
        print(
            f"\n[UDP REQ] Primit cerere JDTT: {target_jd:.6f} | Data: {date_str} | Auto-detectat: Eclipsă {eclipse_type_str} (Separare: {sep_deg:.1f}°)")
        # Apelăm funcția corespunzătoare în mod 100% automat
        if is_lunar:
            hits = solve_cities_for_lunar_eclipse(target_jd)
        else:
            hits = solve_cities_for_eclipse(target_jd)

        total_hits = len(hits)
        print(f"[UDP SOLVE] S-au identificat {total_hits} orașe cu vizibilitate.")

        if total_hits == 0:
            header = struct.pack('<III', 0, 0, 0)
            sock.sendto(header, addr)
            return

        # Trimitem în chunks de 500 de orașe (20 octeți per oraș)
        CHUNK_SIZE = 500
        for start_idx in range(0, total_hits, CHUNK_SIZE):
            chunk = hits[start_idx:start_idx + CHUNK_SIZE]
            is_last = 1 if (start_idx + CHUNK_SIZE) >= total_hits else 0

            header = struct.pack('<III', len(chunk), total_hits, is_last)
            body = bytearray()
            for h in chunk:
                # Format: CityIdx (I), Mag (f), OffsetMin (f), UtcHour (f), Flags (i) -> 20 octeți
                body.extend(struct.pack('<Ifffi', h['city_idx'], h['mag'], h['offset_min'], h['utc_hour'], h['flags']))

            sock.sendto(header + bytes(body), addr)

    except Exception as e:
        traceback.print_exc()
        try:
            sock.sendto(struct.pack('<III', 0, 0, 1), addr)
        except:
            pass


def city_request_listener():
    sock_req = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    if hasattr(socket, 'SIO_UDP_CONNRESET'):
        try:
            sock_req.ioctl(socket.SIO_UDP_CONNRESET, False)
        except Exception:
            pass

    sock_req.bind((HOST, PORT_CITY_REQ))

    while True:
        try:
            data, addr = sock_req.recvfrom(128)
            if len(data) < 8:
                continue

            threading.Thread(
                target=handle_city_request,
                args=(sock_req, data, addr),
                daemon=True
            ).start()

        except ConnectionResetError:
            continue
        except Exception as e:
            traceback.print_exc()


# Inițializare la pornire
init_cities_cache("CountryPoints.csv")
threading.Thread(target=city_request_listener, daemon=True).start()


def compute_moon_libration_and_axis(observer, t):
    """
    Calculeaza libratia optica a Lunii (l, b) si unghiul de pozitie al axei
    de rotatie a Lunii (P_axis), formule Meeus (Astronomical Algorithms, cap. 53).
    Foloseste pozitia APARENTA TOPOCENTRICA (fata de observator), ca sa includa
    automat si mica corectie de libratie diurna (paralaxa), relevanta pentru
    contactele C1-C4 vazute dintr-un loc anume de pe glob.

    Precizie: optica + nutatie (fara termeni fizici de ordin superior) ->
    suficienta pentru indexarea unui profil de relief lunar (Watts/DEM).
    """
    T = (t.tt - 2451545.0) / 36525.0

    # --- Nutatie (Meeus 22.1, precizie ~0.5") ---
    Om_n = math.radians(125.04452 - 1934.136261 * T)
    L_sun = math.radians(280.4665 + 36000.7698 * T)
    L_moon = math.radians(218.3165 + 481267.8813 * T)

    dpsi_arcsec = (-17.20 * math.sin(Om_n) - 1.32 * math.sin(2 * L_sun)
                   - 0.23 * math.sin(2 * L_moon) + 0.21 * math.sin(2 * Om_n))
    deps_arcsec = (9.20 * math.cos(Om_n) + 0.57 * math.cos(2 * L_sun)
                   + 0.10 * math.cos(2 * L_moon) - 0.09 * math.cos(2 * Om_n))

    dpsi = dpsi_arcsec / 3600.0
    deps = deps_arcsec / 3600.0

    # --- Obliquitate medie + adevarata (Meeus 22.2) ---
    eps0_arcsec = 84381.448 - 46.8150 * T - 0.00059 * T**2 + 0.001813 * T**3
    eps0 = eps0_arcsec / 3600.0
    eps = math.radians(eps0 + deps)

    # --- Pozitia aparenta TOPOCENTRICA a Lunii, eclipticala, de data ---
    m_app = observer.at(t).observe(moon).apparent()
    beta_lat, lam_lon, _ = m_app.ecliptic_latlon()
    lam = lam_lon.radians
    beta = beta_lat.radians

    # --- Elemente medii ale Lunii (Meeus 53.1) ---
    F = math.radians((93.2720950 + 483202.0175233 * T - 0.0036539 * T**2
                       - (T**3) / 3526000.0 + (T**4) / 863310000.0) % 360.0)
    Omega = math.radians((125.0445479 - 1934.1362891 * T + 0.0020754 * T**2
                           + (T**3) / 467441.0 - (T**4) / 60616000.0) % 360.0)

    I = math.radians(1.54242)  # inclinarea ecuatorului lunar fata de ecliptica

    W = lam - Omega
    A = math.atan2(math.sin(W) * math.cos(beta) * math.cos(I) - math.sin(beta) * math.sin(I),
                    math.cos(W) * math.cos(beta))
    l_opt = (math.degrees(A) - math.degrees(F) + 180.0) % 360.0 - 180.0

    b_opt = math.asin(-math.sin(W) * math.cos(beta) * math.sin(I)
                       - math.sin(beta) * math.cos(I))
    b_opt_deg = math.degrees(b_opt)

    # --- Unghiul de pozitie al axei de rotatie (Meeus 53.2) ---
    V = Omega + math.radians(dpsi)
    X = math.sin(I) * math.sin(V)
    Y = math.sin(I) * math.cos(V) * math.cos(eps) - math.cos(I) * math.sin(eps)
    omega_ax = math.atan2(X, Y)

    denom = math.cos(b_opt)
    if abs(denom) < 1e-9:
        p_axis = 0.0
    else:
        sin_p = math.sqrt(X**2 + Y**2) * math.cos(lam - omega_ax) / denom
        sin_p = max(-1.0, min(1.0, sin_p))
        p_axis = math.degrees(math.asin(sin_p))

    return l_opt, b_opt_deg, p_axis % 360.0


def contact_selenographic_point(l_opt, b_opt, c_limb_deg):
    """
    Calculeaza corect coordonatele selenografice (lat, lon) ale unui punct
    de pe limbul lunar definit de unghiul C (fata de axa Lunii, c_limb_deg)
    si libratia optica curentă (l_opt, b_opt).
    """
    C_rad = math.radians(c_limb_deg)
    l0 = math.radians(l_opt)
    b0 = math.radians(b_opt)

    # 1. Coordonatele in planul discului vizibil (proiectie ortografica normalizata)
    # Pe limb, distanta radiala este 1.0 (sau sin(90°))
    x = math.sin(C_rad)  # spre estul lunar
    y = math.cos(C_rad)  # spre nordul lunar

    # 2. Transformarea din planul vizibil inapoi in coordonate selenografice
    # tinand cont de libratia (l0, b0) a centrului discului.
    # Aceasta este transformarea inversa exactă a proiectiei ortografice lunare:
    rho = math.sqrt(x ** 2 + y ** 2)
    if rho > 1.0:
        rho = 1.0  # siguranta numerica

    c = math.asin(rho)  # distanta unghiulara de la centrul discului (pe limb este ~pi/2)

    if abs(rho) < 1e-9:
        lat_seleno = b_opt
        lon_seleno = l_opt
    else:
        sin_lat = math.sin(b0) * math.cos(c) + (y * math.sin(c) * math.cos(b0)) / rho
        sin_lat = max(-1.0, min(1.0, sin_lat))
        lat_rad = math.asin(sin_lat)

        delta_lon = math.atan2(x * math.sin(c), rho * math.cos(b0) * math.cos(c) - y * math.sin(b0) * math.sin(c))
        lon_rad = l0 + delta_lon

        lat_seleno = math.degrees(lat_rad)
        lon_seleno = (math.degrees(lon_rad) + 180.0) % 360.0 - 180.0

    return lat_seleno, lon_seleno


def calculate_limb_corrected_contact(observer, t_ev, sun, moon, model, meta, contact_name=None,
                                      ref_year=None, ref_month=None):
    """
    Calculează corecția de limb folosind rețeaua neurală de altitudine lunară,
    și recalculează momentul exact de contact folosind raza corectată (nu raza
    standard peaks/valleys), returnând și decalajul de timp în secunde.
    """
    try:
        # 1. Unghiul de poziție celest și axa Lunii
        p_angle = calculate_p_angle(observer, t_ev)
        if contact_name in ('C2', 'C3'):
            p_angle = (p_angle + 180.0) % 360.0
        l_opt, b_opt, p_axis = compute_moon_libration_and_axis(observer, t_ev)
        c_limb = (p_angle - p_axis) % 360.0

        # 2. Poziția selenografică pe limbul lunar
        lat_sel, lon_sel = contact_selenographic_point(l_opt, b_opt, c_limb)

        # 3. Normalizare pentru rețeaua neurală
        lon_min, lon_max = -180.0, 180.0
        lat_min, lat_max = -90.0, 90.0
        u_val = ((lon_sel - lon_min) / (lon_max - lon_min)) % 1.0
        v_val = np.clip((lat_sel - lat_min) / (lat_max - lat_min), 0.0, 1.0)

        u_t = torch.tensor([u_val], dtype=torch.float32)
        v_t = torch.tensor([v_val], dtype=torch.float32)

        with torch.no_grad():
            pred_norm = model(u_t, v_t)
            alt_meters = pred_norm.item() * meta["alt_std"] + meta["alt_mean"]

        base_radius_km = RADIUS_MOON_VISUAL
        corrected_radius_km = base_radius_km + (alt_meters / 1000.0)

        # 4. RECALCULARE TIMP folosind raza corectată (limb correction real)
        mode = 'external' if contact_name in ('C1', 'C4') else 'internal'
        window_days = 120.0 / 86400.0  # +/- 120 secunde de căutare in jurul t_ev original

        new_jd = solve_bisection_custom(
            observer, mode,
            t_ev.tt - window_days, t_ev.tt + window_days,
            corrected_radius_km
        )

        delta_sec = None
        t_corrected = None
        if new_jd is not None:
            delta_sec = (new_jd - t_ev.tt) * 86400.0
            t_corrected = ts.tt_jd(new_jd)

        return {
            "lat_sel": lat_sel,
            "lon_sel": lon_sel,
            "alt_m": alt_meters,
            "corrected_radius_km": corrected_radius_km,
            "c_limb": c_limb,
            "delta_sec": delta_sec,
            "t_corrected": t_corrected,
        }
    except Exception as e:
        return None


def print_contact_coordinates_with_limb_correction(lat, lon, alt, year, month, day,
                                                   hour=12, minute=0, second=0.0,
                                                   model=_altitude_model, meta=_altitude_meta):
    """
    Versiune extinsă a funcției de afișare care include direct corecția de limb
    bazată pe modelul neural antrenat pe datele LOLA.
    """
    t_current = build_tt_time(year, month, day, hour, minute, second)
    observer = earth + wgs84.latlon(float(lat), float(lon), elevation_m=float(alt))

    search_start_jd = t_current.tt - 0.5
    search_end_jd = t_current.tt + 0.5

    def get_min_in_window(start_jd, end_jd, step_mins=1.0):
        best_jd, min_sep = start_jd, 1000.0
        curr, step = start_jd, step_mins / 1440.0
        while curr < end_jd:
            sep = get_separation_geometric(observer, ts.tt_jd(curr))
            if sep < min_sep:
                min_sep, best_jd = sep, curr
            curr += step
        return best_jd, min_sep

    best_jd, min_sep = get_min_in_window(search_start_jd, search_end_jd)
    start_fine = best_jd - (2.0 / 1440.0)
    end_fine = best_jd + (2.0 / 1440.0)
    best_jd, min_sep = get_min_in_window(start_fine, end_fine, step_mins=(1.0 / 60.0))
    t_max = ts.tt_jd(best_jd)

    r_s_max = get_radius_degrees(sun, observer, t_max)
    r_m_visual = get_radius_degrees(moon, observer, t_max, radius_type='visual')
    is_central = min_sep <= abs(r_m_visual - r_s_max) + 1e-6

    events = {}
    c1 = solve_bisection_custom(observer, 'external', search_start_jd, best_jd)
    if c1: events['C1'] = ts.tt_jd(c1)
    c4 = solve_bisection_custom(observer, 'external', best_jd, search_end_jd)
    if c4: events['C4'] = ts.tt_jd(c4)

    if is_central:
        start_c2 = c1 if c1 else search_start_jd
        c2 = solve_bisection_custom(observer, 'internal', start_c2, best_jd)
        if c2: events['C2'] = ts.tt_jd(c2)

        end_c3 = c4 if c4 else search_end_jd
        c3 = solve_bisection_custom(observer, 'internal', best_jd, end_c3)
        if c3: events['C3'] = ts.tt_jd(c3)

    print(f"\n=== COORDONATE ȘI CORECȚIE DE LIMB (NEURAL NET LUNAR) ===")
    print(f"Locatie: ({lat:.4f}, {lon:.4f})  |  Data: {year}-{month:02d}-{day:02d}")
    print("------------------------------------------------------------------")

    for name in ['C1', 'C2', 'C3', 'C4']:
        if name not in events:
            print(f"{name}: nu apare (eclipsa nu e centrala / contactul lipseste)")
            continue

        t_ev = events[name]
        p_angle = calculate_p_angle(observer, t_ev)
        if name in ['C2', 'C3']:
            p_angle = (p_angle + 180.0) % 360.0

        l_opt, b_opt, p_axis = compute_moon_libration_and_axis(observer, t_ev)
        utc_str = format_event_time(t_ev, year, month)

        # Apelăm funcția de corecție de limb cu modelul tău încărcat în memorie
        #limb_data = calculate_limb_corrected_contact(observer, t_ev, sun, moon, model, meta)
        limb_data = calculate_limb_corrected_contact(observer, t_ev, sun, moon, model, meta, contact_name=name)
        print(f"{name}: {utc_str} UTC")
        print(f"    P (fata de nord)      = {p_angle:.2f}°")

        if limb_data:
            print(f"    C (fata de axa Lunii) = {limb_data['c_limb']:.2f}°")
            print(f"    Selenografic          = ({limb_data['lat_sel']:+.3f}°, {limb_data['lon_sel']:+.3f}°)")
            print(f"    Altitudine Neurală    = {limb_data['alt_m']:+.2f} m")
            print(f"    Raza Corectată (Limb) = {limb_data['corrected_radius_km']:.4f} km")
        else:
            print(f"    [Eroare la calculul rețelei neuronale pentru limb]")

    print("------------------------------------------------------------------")
    return events


# Apel de test direct în consolă cu modelul deja încărcat (_altitude_model și _altitude_meta)
"""print_contact_coordinates_with_limb_correction(
    lat=41.65, lon=-0.8877, alt=0.0,
    year=2026, month=8, day=12, hour=17, minute=30,
    model=_altitude_model, meta=_altitude_meta
)"""


#print_contact_coordinates(lat=41.65, lon=-0.8877, alt=0.0, year=2026, month=8, day=12, hour=17, minute=30)


"""def measure_contact_longitude_spread(n_samples=100, seed=42):
    import random
    rng = random.Random(seed)
    max_dev = 0.0
    worst_case = None
    n_found = 0

    print("=== Start scanare extensiva (de441) ===")

    for i in range(n_samples):
        # Folosim anii extinsi suportati de de441
        year = rng.randint(-5000, 3000)  # Poti pune -13000, 17000 dar -5000 la 3000 e mai rapid de testat
        month = rng.randint(1, 12)
        day = rng.randint(1, 28)
        lat = rng.uniform(-60, 60)
        lon = rng.uniform(-180, 180)

        try:
            t_start = build_tt_time(year, month, day, 0, 0, 0.0)
            observer = earth + wgs84.latlon(lat, lon, elevation_m=0.0)

            future_jd = find_next_eclipse(observer, t_start)
            if future_jd is None:
                continue

            # Ca sa fim siguri ca gasim momentul de maxim corect in jur de future_jd
            # Cautam minimul de separatie intr-o fereastra de +- 3 ore in jurul lui future_jd
            best_jd, min_sep = future_jd, 1000.0
            step = 1.0 / 1440.0  # 1 minut
            curr = future_jd - 0.15  # ~3.6 ore inainte
            end_w = future_jd + 0.15  # ~3.6 ore dupa

            while curr < end_w:
                sep = get_separation_geometric(observer, ts.tt_jd(curr))
                if sep < min_sep:
                    min_sep = sep
                    best_jd = curr
                curr += step

            t_max = ts.tt_jd(best_jd)
            r_s_max = get_radius_degrees(sun, observer, t_max)
            r_m_visual = get_radius_degrees(moon, observer, t_max, radius_type='visual')

            # Verificam daca e eclipsa vizibila din punct de vedere geometric la acest observator
            if min_sep > (r_m_visual + r_s_max):
                continue  # Nu e eclipsa vizibila de la acest observer

            # Cautam C1 si C4 corect in intervalul dinaintea si de dupa maxim
            c1 = solve_bisection_custom(observer, 'external', best_jd - 0.15, best_jd)
            c4 = solve_bisection_custom(observer, 'external', best_jd, best_jd + 0.15)

            contacts_to_test = []
            if c1: contacts_to_test.append(('C1', c1, 'external'))
            if c4: contacts_to_test.append(('C4', c4, 'external'))

            # Daca e centrala, cautam si C2, C3
            if min_sep <= abs(r_m_visual - r_s_max) + 1e-6:
                c2 = solve_bisection_custom(observer, 'internal', c1 if c1 else (best_jd - 0.1), best_jd)
                c3 = solve_bisection_custom(observer, 'internal', best_jd, c4 if c4 else (best_jd + 0.1))
                if c2: contacts_to_test.append(('C2', c2, 'internal'))
                if c3: contacts_to_test.append(('C3', c3, 'internal'))

            for name, jd_c, mode in contacts_to_test:
                t_c = ts.tt_jd(jd_c)
                p_angle = calculate_p_angle(observer, t_c)
                if mode == 'internal':
                    p_angle = (p_angle + 180.0) % 360.0

                l_opt, b_opt, p_axis = compute_moon_libration_and_axis(observer, t_c)
                c_limb = (p_angle - p_axis) % 360.0
                lat_sel, lon_sel = contact_selenographic_point(l_opt, b_opt, c_limb)

                # Calculam deviatia fata de +-90 grade
                dev = min(abs(abs(lon_sel) - 90.0), abs(abs(lon_sel) - 270.0))
                n_found += 1

                if dev > max_dev:
                    max_dev = dev
                    worst_case = (year, month, day, lat, lon, name, lon_sel)

        except Exception as e:
            # Afiseaza eroarea daca ceva crapa, ca sa nu fie inghitita tăcut
            # print(f"Eroare: {e}")
            continue

    print(f"\nContacte gasite cu succes: {n_found}")
    print(f"Deviatia maxima reala fata de +-90 grade: {max_dev:.4f} grade")
    print(f"Cel mai defavorabil caz gasit: {worst_case}")
    return max_dev

maximum = measure_contact_longitude_spread()
print(maximum)"""
# --- LOOP PRINCIPAL ---
last_sim_tt = None
last_wall_time = time.time()
was_fast_forwarding = False
settled_timer_start = None
FAST_FORWARD_THRESHOLD = 0.0035
b_initial_sync_done = False

while True:
    try:
        data, addr = sock.recvfrom(1024)
        if len(data) < 80:
            continue

        unpacked = struct.unpack('<dddddddddd', data)
        year, month, day = int(unpacked[0]), int(unpacked[1]), int(unpacked[2])
        if year < -13000 or year > 17000:
            continue

        hour, minute = int(unpacked[3]), int(unpacked[4])
        second = float(unpacked[5])
        p_lat, p_lon, p_alt = unpacked[6], unpacked[7], unpacked[8]
        lang_code = unpacked[9]
        current_language = "English" if lang_code == 1.0 else "Romana"
        t_current = build_tt_time(year, month, day, hour, minute, second)

        # 1. Masurare viteza derulare (zile simulate / secunda reala)
        now_wall = time.time()
        wall_dt = now_wall - last_wall_time
        last_wall_time = now_wall

        is_fast_forwarding = False
        if last_sim_tt is not None and wall_dt > 0.0001:
            sim_rate_days_per_sec = abs(t_current.tt - last_sim_tt) / wall_dt
            if sim_rate_days_per_sec > FAST_FORWARD_THRESHOLD:
                is_fast_forwarding = True

        last_sim_tt = t_current.tt

        dist = abs(p_lat - last_calc_lat_lon[0]) + abs(p_lon - last_calc_lat_lon[1])
        date_changed = (year, month, day) != last_calc_date
        lang_changed = (current_language != last_calc_lang)

        # 2. Control declansare thread greu
        should_calculate = False

        # Pasul critic: Forțăm calculul la primul pachet primit!
        if not b_initial_sync_done:
            b_initial_sync_done = True
            should_calculate = True
        elif is_fast_forwarding:
            was_fast_forwarding = True
            settled_timer_start = None
        else:
            # Cazul A: Am iesit din derulare rapida -> asteptam 0.35s de stabilitate
            if was_fast_forwarding:
                if settled_timer_start is None:
                    settled_timer_start = now_wall
                elif (now_wall - settled_timer_start) > 0.35:
                    if not is_calculating:
                        was_fast_forwarding = False
                        settled_timer_start = None
                        should_calculate = True

            # Cazul B: Viteza normala si avem modificari de pozitie/data/limba
            elif (dist > 0.0005 or date_changed or lang_changed) and (now_wall - last_real_time_calc > 1.0):
                if not is_calculating:
                    should_calculate = True

        if should_calculate and not is_calculating:
            last_calc_date = (year, month, day)
            last_calc_lat_lon = (p_lat, p_lon)
            last_calc_lang = current_language
            last_real_time_calc = now_wall
            is_calculating = True

            thread_calc = threading.Thread(
                target=background_calculate,
                args=(p_lat, p_lon, p_alt, t_current, year, month, day, current_language),
                daemon=True
            )
            thread_calc.start()

        # 3. Calcule Live HUD (directe)
        player_loc = earth + wgs84.latlon(float(p_lat), float(p_lon), elevation_m=float(p_alt))
        obs_sun = player_loc.at(t_current).observe(sun)
        obs_moon = player_loc.at(t_current).observe(moon)
        sep = obs_sun.separation_from(obs_moon).degrees
        d_s = obs_sun.distance().km
        d_m = obs_moon.distance().km
        r_s = math.degrees(math.asin(RADIUS_SUN_KM / d_s))
        r_m = math.degrees(math.asin(RADIUS_MOON_VISUAL / d_m))
        mag_live = (r_m + r_s - sep) / (2.0 * r_s) if sep < (r_s + r_m) else 0.0

        # 4. Asamblare pachet UDP
        text_bytes = cached_report_text.encode('utf-8')[:999].ljust(1000, b'\x00')

        e_t = earth.at(t_current)
        s_pos = wgs84.geographic_position_of(e_t.observe(sun)).itrs_xyz.km
        m_pos = wgs84.geographic_position_of(e_t.observe(moon)).itrs_xyz.km

        planet_coords = []
        for p in target_planets:
            p_pos = wgs84.geographic_position_of(e_t.observe(p)).itrs_xyz.km
            planet_coords.extend([float(p_pos[0]), float(p_pos[1]), float(p_pos[2])])

        moon_coords = []
        for m in major_moons:
            try:
                m_pos2 = wgs84.geographic_position_of(e_t.observe(m)).itrs_xyz.km
                moon_coords.extend([float(m_pos2[0]), float(m_pos2[1]), float(m_pos2[2])])
            except EphemerisRangeError:
                moon_coords.extend([0.0, 0.0, 0.0])

        gast_deg = t_current.gast * 15.0

        # Punctele traseului (pre-alocate la fix 301 elemente)
        path_payload = [0.0] * 301
        if len(cached_eclipse_path) > 0:
            path_payload[0] = float(len(cached_eclipse_path))
            idx = 1
            for pt in cached_eclipse_path[:100]:
                path_payload[idx] = float(pt[0])
                path_payload[idx + 1] = float(pt[1])
                idx += 2
            idx = 201
            for pt in cached_eclipse_path[:100]:
                path_payload[idx] = float(pt[2])
                idx += 1

        delta_t_val = float(delta_t_seconds(year, month)) if uses_custom_deltat(year) else float(t_current.delta_t)
        live_shadow_lat, live_shadow_lon = get_live_shadow_center_wgs84(t_current, sun, moon, earth)
        #Calcul vectori de la centru la centru

        sun_center_ecef = earth.at(t_current).observe(sun).apparent().frame_xyz(itrs).km
        moon_center_ecef = earth.at(t_current).observe(moon).apparent().frame_xyz(itrs).km

        center_to_center_coords = [
            float(sun_center_ecef[0]), float(sun_center_ecef[1]), float(sun_center_ecef[2]),
            float(moon_center_ecef[0]), float(moon_center_ecef[1]), float(moon_center_ecef[2])
        ]
        all_floats = [
            float(s_pos[0]), float(s_pos[1]), float(s_pos[2]),
            float(m_pos[0]), float(m_pos[1]), float(m_pos[2]),
            float(mag_live),
            float(gast_deg)
        ] + planet_coords + moon_coords

        #final_floats = all_floats + path_payload + [float(live_shadow_lat), float(live_shadow_lon), float(delta_t_val)] + center_to_center_coords
        final_floats = (
                all_floats
                + path_payload
                + [float(live_shadow_lat), float(live_shadow_lon), float(delta_t_val)]
                + center_to_center_coords
        )
        response = struct.pack('<357d1000s', *final_floats, text_bytes)
        sock.sendto(response, (HOST, PORT_OUT))

    except Exception:
        pass