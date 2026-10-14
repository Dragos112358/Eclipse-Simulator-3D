#!/usr/bin/env python3
"""
Merge simulator_results.json (from collect_simulator.py) with cases.csv and
jubier_reference.csv and print the Markdown tables for validation/README.md.

    python run_validation.py
    python run_validation.py --use-limb
    python run_validation.py --out summary.md --json summary.json
"""
import argparse
import csv
import json
import math
import re
import sys
from pathlib import Path

SECONDARY = [("magnitude", "Magnitude at maximum"), ("ratio", "Moon/Sun size ratio"),
             ("duration_s", "Totality duration (s)"), ("width_km", "Path width (km)"),
             ("speed_kms", "Umbral velocity (km/s)"), ("depth_km", "Umbral depth (km)")]
NEAR_REGIMES = {"core"}          # everything else is reported separately


def num(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def to_sec(t):
    """'HH:MM:SS(.s)' or 'YYYY-MM-DD HH:MM:SS(.s)' -> seconds of day."""
    m = re.search(r"(\d{1,2}):(\d{2}):(\d{2}(?:\.\d+)?)", t or "")
    if not m:
        return None
    return int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))


def delta(a, b):
    """a - b in seconds, wrapped to [-12h, 12h) so midnight crossings work."""
    return (a - b + 43200) % 86400 - 43200


def read_csv(path, key="case"):
    with open(path, newline="", encoding="utf-8") as f:
        return {r[key].strip(): r for r in csv.DictReader(f)}


def stats(vals):
    if not vals:
        return None
    return {"n": len(vals), "mean": sum(vals) / len(vals),
            "mean_abs": sum(abs(v) for v in vals) / len(vals),
            "max_abs": max(abs(v) for v in vals),
            "rms": math.sqrt(sum(v * v for v in vals) / len(vals))}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", default="simulator_results.json")
    ap.add_argument("--cases", default="cases.csv")
    ap.add_argument("--ref", default="jubier_reference.csv")
    ap.add_argument("--use-limb", action="store_true", help="compare C1L-C4L instead of C1-C4")
    ap.add_argument("--out")
    ap.add_argument("--json")
    a = ap.parse_args()

    sim = json.loads(Path(a.sim).read_text(encoding="utf-8"))
    cases = read_csv(a.cases)
    refs = read_csv(a.ref)
    suffix = "L" if a.use_limb else ""

    rows, settings, secondary = [], [], []
    for c in sim["cases"]:
        name = c["case"]
        meta_c, ref = cases.get(name, {}), refs.get(name)
        if not c.get("ok") or not ref:
            print(f"[warning] {name}: no simulator result or no reference row", file=sys.stderr)
            continue
        location = (meta_c.get("location") or c.get("location") or name).strip()
        regime = (meta_c.get("regime") or c.get("regime") or "core").strip()
        for k in ("C1", "C2", "C3", "C4"):
            s_ev = c["events"].get(k + suffix)
            j_t = (ref.get(k) or "").strip()
            if not s_ev or not j_t:
                continue
            s, j = to_sec(s_ev["utc"]), to_sec(j_t)
            if s is None or j is None:
                continue
            rows.append({"case": name, "location": location, "regime": regime, "contact": k,
                         "project": s_ev["utc"], "jubier": j_t.split()[-1], "delta": delta(s, j)})
        dts, dtj = c.get("delta_t_s"), num(ref.get("delta_t_s"))
        settings.append((name, c.get("alt_m"), num(ref.get("alt_m")), dts, dtj,
                         None if dtj is None or dts is None else dts - dtj,
                         (ref.get("read_on") or ref.get("retrieved_on") or "?")))
        for key, label in SECONDARY:
            sv, jv = c.get("meta", {}).get(key), num(ref.get(key))
            if sv is not None and jv is not None:
                secondary.append((name, label, sv, jv))

    if not rows:
        sys.exit("No comparable contacts. Check jubier_reference.csv (C1..C4).")

    near = [r for r in rows if r["regime"] in NEAR_REGIMES]
    far = [r for r in rows if r["regime"] not in NEAR_REGIMES]
    order = list(dict.fromkeys(r["case"] for r in rows))

    out = [f"Compared quantity: {'limb-corrected C1L-C4L' if a.use_limb else 'geometric C1-C4'}. "
           "Δ = this project − Jubier, in seconds.", "", "## Summary", "",
           "| Case | Location | Regime | max \\|Δ\\| on C1–C4 |", "|---|---|---|---|"]
    for n in order:
        rs = [r for r in rows if r["case"] == n]
        out.append(f"| {n} | {rs[0]['location']} | {rs[0]['regime']} | "
                   f"{max(abs(r['delta']) for r in rs):.1f} s |")

    sn = stats([r["delta"] for r in near])
    if sn:
        out += ["", f"Near-present regime ({sn['n']} contacts): **max |Δ| = {sn['max_abs']:.1f} s, "
                    f"mean |Δ| = {sn['mean_abs']:.2f} s, RMS = {sn['rms']:.2f} s.**"]
    sf = stats([r["delta"] for r in far])
    if sf:
        out += [f"Other regimes ({sf['n']} contacts, reported separately): max |Δ| = {sf['max_abs']:.1f} s, "
                f"mean Δ = {sf['mean']:+.1f} s."]

    out += ["", "## Reference settings", "",
            "| Case | Alt. simulator (m) | Alt. Jubier (m) | ΔT simulator (s) | ΔT Jubier (s) | Δ ΔT (sim − Jubier) | Reference read on |",
            "|---|---|---|---|---|---|---|"]
    f1 = lambda v, fmt: "" if v is None else format(v, fmt)
    for n, als, alj, dts, dtj, ddt, got in settings:
        out.append(f"| {n} | {f1(als, 'g')} | {f1(alj, 'g')} | {f1(dts, '.2f')} | {f1(dtj, '.1f')} | "
                   f"{f1(ddt, '+.2f')} | {got} |")

    out += ["", "## Per-contact comparison"]
    for n in order:
        out += ["", f"### {n}", "", "| Contact | This project | Jubier | Δ (s) |", "|---|---|---|---|"]
        out += [f"| {r['contact']} | {r['project']} | {r['jubier']} | {r['delta']:+.1f} |"
                for r in rows if r["case"] == n]

    if near:
        out += ["", "## Systematic pattern (near-present regime)", "",
                "| Contact | mean Δ (s) | min | max |", "|---|---|---|---|"]
        for k in ("C1", "C2", "C3", "C4"):
            v = [r["delta"] for r in near if r["contact"] == k]
            if v:
                out.append(f"| {k} | {sum(v)/len(v):+.1f} | {min(v):+.1f} | {max(v):+.1f} |")
        tot = []
        for n in order:
            d = {r["contact"]: r["delta"] for r in near if r["case"] == n}
            if "C2" in d and "C3" in d:
                tot.append(d["C3"] - d["C2"])
        if tot:
            out += ["", f"Totality duration Δ (C3−C2 difference): min {min(tot):+.1f} s, max {max(tot):+.1f} s."]

    if secondary:
        out += ["", "## Secondary quantities", "",
                "| Case | Quantity | Simulator | Jubier | Diff | Rel. diff |", "|---|---|---|---|---|---|"]
        for n, label, sv, jv in secondary:
            rel = f"{abs(sv - jv) / abs(jv) * 100:.3f} %" if jv else ""
            out.append(f"| {n} | {label} | {sv:g} | {jv:g} | {sv - jv:+.4g} | {rel} |")

    md = "\n".join(out) + "\n"
    print(md)
    if a.out:
        Path(a.out).write_text(md, encoding="utf-8")
    if a.json:
        Path(a.json).write_text(json.dumps({"near": sn, "other": sf, "rows": rows}, indent=2),
                                encoding="utf-8")


if __name__ == "__main__":
    main()