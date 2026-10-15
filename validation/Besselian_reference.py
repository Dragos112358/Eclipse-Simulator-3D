#!/usr/bin/env python3
"""
Referinta independenta pentru circumstantele locale ale eclipselor de Soare,
calculata din elementele Besselian ale catalogului (Eclipse_de_soare.txt),
FARA Skyfield. Metoda: Meeus, Astronomical Algorithms, cap. 54 / Espenak.

Rulare: doar Run din PyCharm, fara argumente. Editeaza caile de mai jos.

Scrie besselian_reference.csv si afiseaza diferentele (simulator - referinta).
Foloseste acelasi DeltaT ca simulatorul (din simulator_results.json), deci
diferentele reflecta geometria si razele lunare, nu DeltaT.
Doar biblioteca standard.
"""
import csv
import json
import math
import os
from datetime import datetime, timedelta

J2000 = datetime(2000, 1, 1, 12)

# ===================== EDITEAZA AICI CAILE =====================
CATALOG_PATH = r"..\Content\Encoder\Eclipse_de_soare.txt"
SIM_PATH = "simulator_results.json"       # relativ la folderul acestui script (sau cale completa)
OUT_PATH = "besselian_reference.csv"      # relativ la folderul acestui script
# ===============================================================
HERE = os.path.dirname(os.path.abspath(__file__))


def resolve(p):
    return p if os.path.isabs(p) else os.path.join(HERE, p)


def poly(c, t):
    return c[0] + t * (c[1] + t * (c[2] + t * c[3]))


def jd_of(date_str, hours):
    y, m, d = map(int, date_str.split("-"))
    return datetime(y, m, d).toordinal() + 1721424.5 + hours / 24.0


def load_catalog(path):
    """Aceleasi coloane ca parserul din OrbitalManager.cpp."""
    rows = []
    with open(path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            p = line.split()
            if len(line) < 100 or len(p) < 46 or "-" not in p[0]:
                continue
            try:
                def c(a):
                    return [float(v) for v in p[a:a + 4]]
                rows.append(dict(t0=float(p[10]), x=c(11), y=c(15), d=c(23), mu=c(27),
                                 l1=c(31), l2=c(35), f1=c(39), f2=c(43)))
            except ValueError:
                continue
    return rows


def pick(rows, jd_tt):
    best = min(rows, key=lambda r: abs(r["t0"] - jd_tt))
    return best if abs(best["t0"] - jd_tt) < 0.6 else None


def circ(el, t, lat, lon, h_m, dT):
    """Returneaza (m, L1, L2) la t ore fata de T0 (TT)."""
    x, y = poly(el["x"], t), poly(el["y"], t)
    d = math.radians(poly(el["d"], t))
    mu = poly(el["mu"], t)
    l1, l2 = poly(el["l1"], t), poly(el["l2"], t)
    f1, f2 = poly(el["f1"], t), poly(el["f2"], t)

    phi = math.radians(lat)
    u = math.atan(0.99664719 * math.tan(phi))
    rs = 0.99664719 * math.sin(u) + (h_m / 6378140.0) * math.sin(phi)   # rho sin(phi')
    rc = math.cos(u) + (h_m / 6378140.0) * math.cos(phi)                # rho cos(phi')
    H = math.radians(mu + lon - 0.00417807 * dT)                        # lon est pozitiva

    xi = rc * math.sin(H)
    eta = rs * math.cos(d) - rc * math.cos(H) * math.sin(d)
    zeta = rs * math.sin(d) + rc * math.cos(H) * math.cos(d)
    m = math.hypot(x - xi, y - eta)
    return m, l1 - zeta * f1, l2 - zeta * f2


def solve(el, lat, lon, h_m, dT):
    f = lambda t: circ(el, t, lat, lon, h_m, dT)
    ts = [i / 60.0 for i in range(-180, 181)]          # +-3 h, pas 1 min
    ms = [f(t)[0] for t in ts]
    i = min(range(len(ts)), key=lambda k: ms[k])
    a, b = ts[max(i - 1, 0)], ts[min(i + 1, len(ts) - 1)]
    for _ in range(60):                                 # cautare ternara pentru maxim
        m1, m2 = a + (b - a) / 3, b - (b - a) / 3
        if f(m1)[0] < f(m2)[0]:
            b = m2
        else:
            a = m1
    tm = (a + b) / 2
    m, L1, L2 = f(tm)

    def root(g, lo, hi):
        glo = g(lo)
        if glo * g(hi) > 0:
            return None
        for _ in range(60):
            mid = (lo + hi) / 2
            gm = g(mid)
            if glo * gm <= 0:
                hi = mid
            else:
                lo, glo = mid, gm
        return (lo + hi) / 2

    g1 = lambda t: f(t)[0] - f(t)[1]
    g2 = lambda t: f(t)[0] - abs(f(t)[2])
    out = {"MAX": tm}
    if g1(tm) < 0:
        out["C1"] = root(g1, tm - 3, tm)
        out["C4"] = root(g1, tm, tm + 3)
    if g2(tm) < 0:
        out["C2"] = root(g2, tm - 0.6, tm)
        out["C3"] = root(g2, tm, tm + 0.6)

    rS, rM = (L1 + L2) / 2, (L1 - L2) / 2
    mag = (L1 - m) / (L1 + L2)
    return out, mag, obscuration(rS, rM, m)


def obscuration(rS, rM, d):
    if d >= rS + rM:
        return 0.0
    if d <= abs(rS - rM):
        return min(1.0, (rM / rS) ** 2) * 100.0
    a = rS ** 2 * math.acos((d * d + rS ** 2 - rM ** 2) / (2 * d * rS))
    b = rM ** 2 * math.acos((d * d + rM ** 2 - rS ** 2) / (2 * d * rM))
    c = 0.5 * math.sqrt((-d + rS + rM) * (d + rS - rM) * (d - rS + rM) * (d + rS + rM))
    return (a + b - c) / (math.pi * rS ** 2) * 100.0


def to_utc(t0, t, dT):
    return J2000 + timedelta(days=t0 + t / 24.0 - dT / 86400.0 - 2451545.0)


def sec_of_day(dt):
    return dt.hour * 3600 + dt.minute * 60 + dt.second + dt.microsecond / 1e6


def hms_to_s(s):
    h, m, sec = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(sec)


def main():
    catalog, sim_path, out_path = CATALOG_PATH, resolve(SIM_PATH), resolve(OUT_PATH)
    for p in (catalog, sim_path):
        if not os.path.exists(p):
            raise SystemExit(f"Nu gasesc fisierul: {p}\nEditeaza CATALOG_PATH / SIM_PATH la inceputul scriptului.")

    rows = load_catalog(catalog)
    print(f"Catalog: {len(rows)} eclipse incarcate")
    sim = json.load(open(sim_path, encoding="utf-8"))

    keys = ("C1", "C2", "MAX", "C3", "C4")
    out_rows = []
    print(f"{'case':28s} " + " ".join(f"d{k:>4s}" for k in keys) + "   dmag    dobs%")
    for c in sim["cases"]:
        if not c.get("ok"):
            continue
        dT = c["delta_t_s"]
        hh = int(c["utc_hint"][:2]) + int(c["utc_hint"][3:5]) / 60.0
        jd_tt = jd_of(c["date"], hh) + dT / 86400.0
        el = pick(rows, jd_tt)
        if el is None:
            print(f"{c['case']:28s} nu gasesc eclipsa in catalog")
            continue
        res, mag, obs = solve(el, c["lat"], c["lon"], c["alt_m"], dT)
        row = {"case": c["case"], "type": c["type"], "dT_s": round(dT, 3),
               "magnitude": round(mag, 5), "obscuration_pct": round(obs, 2)}
        deltas = []
        for k in keys:
            t = res.get(k)
            if t is None:
                row[k] = ""
                deltas.append("    -")
                continue
            u = to_utc(el["t0"], t, dT)
            row[k] = u.strftime("%Y-%m-%d %H:%M:%S.") + f"{u.microsecond // 100000}"
            s = c["events"].get(k, {}).get("utc")
            if s:
                d = (hms_to_s(s) - sec_of_day(u) + 43200) % 86400 - 43200
                deltas.append(f"{d:+5.1f}")
            else:
                deltas.append("    ?")
        out_rows.append(row)
        dm = c["meta"]["magnitude"] - mag
        do = c["meta"]["obscuration_pct"] - obs
        print(f"{c['case']:28s} " + " ".join(f"{d:>5s}" for d in deltas) + f"  {dm:+.5f} {do:+7.2f}")

    cols = ["case", "type", "dT_s", "C1", "C2", "MAX", "C3", "C4", "magnitude", "obscuration_pct"]
    with open(out_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(out_rows)
    print(f"Salvat {out_path}")


if __name__ == "__main__":
    main()