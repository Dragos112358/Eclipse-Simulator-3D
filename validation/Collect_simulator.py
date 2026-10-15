#!/usr/bin/env python3
"""
Collect contact times from the 3D Eclipse Simulator backend, exactly the way
Unreal does it: send the 80-byte request ('<10d') to UDP :5000 and read the
3,856-byte response ('<357d1200s') from UDP :5001.

For every row of cases.csv it asks the server for the eclipse at that place,
waits until the *fresh* report arrives (the server computes it on a background
thread), parses the report table and saves everything to simulator_results.json.

Works for total, annular, hybrid and partial eclipses. cases.csv may contain the
optional columns `type` (total/annular/hybrid/partial) and `type_at_location`
(for hybrid eclipses: what the observer actually sees there). They are used to
(a) know which contacts to expect (a partial site has only C1 and C4) and
(b) compare the eclipse type with the one reported by the server.

Usage
-----
    # 1) server already running (Server.exe or `python Server.py`), Unreal CLOSED
    python collect_simulator.py

    # 2) let the script start the server for you
    cd validation
    python collect_simulator.py --server-dir ../Content/Encoder

    # 3) only some rows (useful for a first test)
    python collect_simulator.py --only 2030_istanbul,2022_bucharest_partial

Close Unreal first: it listens on the same port (5001).
"""
import argparse
import csv
import json
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

HOST = "127.0.0.1"
PORT_SERVER = 5000        # Unreal -> Python
PORT_LISTEN = 5001        # Python -> Unreal
REQ_FMT = "<10d"          # y, m, d, H, M, S, lat, lon, alt, lang   (80 B)
RESP_FMT = "<357d1200s"   # 357 doubles + 1200-byte report text     (3,856 B)
RESP_SIZE = struct.calcsize(RESP_FMT)
IDX_DELTA_T = 350         # 8 + 21 + 18 = 47 | +301 path = 348 | lat, lon, DeltaT = 348, 349, 350
LANG_ENGLISH = 1.0

EVENT_RE = re.compile(
    r"^(C[1-4]L?|MAXL?|Sunrise|Sunset)\s*\|\s*(\d{2}:\d{2}:\d{2}\.\d)\s*\|\s*"
    r"([+-]?\d+\.\d)\D*\|\s*(\d+\.\d)\D*\|\s*(\d+)", re.M)
COORDS_RE = re.compile(r"COORDS:\s*(-?\d+\.\d+),\s*(-?\d+\.\d+)")
# Type / Duration / Magnitude / Ratio are parsed independently so that a partial
# eclipse (no Ratio, no Duration) does not make the whole line fail.
TYPE_RE = re.compile(r"Type:\s*(\w+)")
DUR_RE = re.compile(r"Duration:\s*([^\n|]+)")
LIMBDUR_RE = re.compile(r"Lunar Limb Duration:\s*([^\n]+)")
MAG_RE = re.compile(r"Magnitude:\s*(-?[\d.]+)")
RATIO_RE = re.compile(r"Ratio:\s*([\d.]+)")
OBS_RE = re.compile(r"Obscuration:\s*([\d.]+)\s*%")
VELW_RE = re.compile(r"Umb\. Vel:\s*([^\n|]+?)\s*\|\s*Width:\s*([^\n]+)")
DEPTH_RE = re.compile(r"Umb\. Depth:\s*([^\n]+)")
LOC_RE = re.compile(r"LOCATION:\s*([^\n]+)")


def dur_to_s(txt):
    m = re.search(r"(\d+)m\s*([\d.]+)s", txt or "")
    if m:
        return int(m.group(1)) * 60 + float(m.group(2))
    m = re.search(r"^\s*([\d.]+)s", txt or "")
    return float(m.group(1)) if m else None


def first_float(txt):
    m = re.search(r"-?\d+(?:\.\d+)?", txt or "")
    return float(m.group(0)) if m else None


def parse_report(text):
    events = {}
    for m in EVENT_RE.finditer(text):
        events[m.group(1)] = {"utc": m.group(2), "alt": float(m.group(3)),
                              "az": float(m.group(4)), "p": int(m.group(5))}
    meta = {}
    if (m := TYPE_RE.search(text)):
        meta["type"] = m.group(1)
    if (m := DUR_RE.search(text)):
        meta["duration_s"] = dur_to_s(m.group(1))
    if (m := LIMBDUR_RE.search(text)):
        meta["limb_duration_s"] = dur_to_s(m.group(1))
    if (m := MAG_RE.search(text)):
        meta["magnitude"] = float(m.group(1))
    if (m := RATIO_RE.search(text)):
        meta["ratio"] = float(m.group(1))
    if (m := OBS_RE.search(text)):
        meta["obscuration_pct"] = float(m.group(1))
    if (m := VELW_RE.search(text)):
        meta["speed_kms"], meta["width_km"] = first_float(m.group(1)), first_float(m.group(2))
    if (m := DEPTH_RE.search(text)):
        km = re.search(r"\(([\d.]+)\s*km\)", m.group(1))
        meta["depth_km"] = float(km.group(1)) if km else None
    if (m := LOC_RE.search(text)):
        meta["location_name"] = m.group(1).strip()
    c = COORDS_RE.search(text)
    coords = (float(c.group(1)), float(c.group(2))) if c else None
    return events, meta, coords


def decode_response(data):
    if len(data) != RESP_SIZE:
        return None
    vals = struct.unpack(RESP_FMT, data)
    raw = vals[357].rstrip(b"\x00")
    truncated = len(raw) >= 999
    text = raw.decode("utf-8", errors="replace")
    return {"text": text, "truncated": truncated, "delta_t_s": vals[IDX_DELTA_T]}


def make_request(date, hint, lat, lon, alt):
    y, mo, d = (int(x) for x in date.split("-"))
    hh, mm = (int(x) for x in hint.split(":"))
    return struct.pack(REQ_FMT, y, mo, d, hh, mm, 0.0, lat, lon, alt, LANG_ENGLISH)


def ask(sock, packet, want_coords, timeout, need_table=True):
    """Resend the same packet until a report for `want_coords` shows up."""
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        sock.sendto(packet, (HOST, PORT_SERVER))
        try:
            data, _ = sock.recvfrom(8192)
        except socket.timeout:
            continue
        resp = decode_response(data)
        if not resp:
            continue
        events, meta, coords = parse_report(resp["text"])
        last = (resp, events, meta, coords)
        if coords and abs(coords[0] - want_coords[0]) < 6e-5 and abs(coords[1] - want_coords[1]) < 6e-5:
            if not need_table or "Event" in resp["text"] or "NO ECLIPSE" in resp["text"]:
                return last
        time.sleep(0.15)
    return None


def collect_case(sock, case, timeout, state):
    lat, lon, alt = case["lat"], case["lon"], case["alt_m"]
    hints = []
    for h in [case["utc_hint"], "12:00", "00:00", "06:00", "18:00"]:
        if h and h not in hints:
            hints.append(h)
    for hint in hints:
        # freshness: if the previous report was for the same coordinates, first move away
        prev = state.get("coords")
        if prev and abs(prev[0] - lat) < 6e-5 and abs(prev[1] - lon) < 6e-5:
            s_lat = lat + 1.0 if lat < 80 else lat - 1.0
            ask(sock, make_request(case["date"], hint, s_lat, lon, alt), (s_lat, lon), timeout, need_table=False)
        res = ask(sock, make_request(case["date"], hint, lat, lon, alt), (lat, lon), timeout)
        state["coords"] = (lat, lon)
        if not res:
            continue
        resp, events, meta, _ = res
        if "NO ECLIPSE" in resp["text"] or "UPCOMING" in resp["text"]:
            continue                       # window did not contain this eclipse -> next hint
        if "C1" not in events and "C4" not in events:
            continue
        return {"hint_used": hint, "events": events, "meta": meta,
                "delta_t_s": resp["delta_t_s"], "truncated": resp["truncated"], "raw_report": resp["text"]}
    return None


def load_cases(path, only=None):
    cases, bad = [], []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            name = (r.get("case") or "").strip()
            if not name:
                continue
            if only and name not in only:
                continue
            try:
                lat, lon = float(r["lat"]), float(r["lon"])
            except (TypeError, ValueError, KeyError):
                bad.append(f"{name}: lat/lon empty - fill them in cases.csv")
                continue
            alt = (r.get("alt_m") or "").strip()
            if alt == "":
                bad.append(f"{name}: alt_m empty, using 0 (set it equal to Jubier's elevation)")
                alt = 0.0
            cases.append({"case": name, "location": (r.get("location") or "").strip(),
                          "regime": (r.get("regime") or "core").strip(),
                          "type": (r.get("type") or "").strip().lower(),
                          "type_at_location": (r.get("type_at_location") or "").strip().lower(),
                          "date": r["date"].strip(),
                          "utc_hint": (r.get("utc_hint") or "").strip(),
                          "lat": lat, "lon": lon, "alt_m": float(alt)})
    return cases, bad


def expected_contacts(case):
    """A partial site has only C1 and C4; every other type has all four."""
    local = case["type_at_location"] or case["type"]
    return ("C1", "C4") if local == "partial" else ("C1", "C2", "C3", "C4")


def wait_for_server(sock, seconds):
    t0 = time.time()
    pkt = make_request("2026-08-12", "18:00", 41.649, -0.889, 0.0)
    while time.time() - t0 < seconds:
        sock.sendto(pkt, (HOST, PORT_SERVER))
        try:
            data, _ = sock.recvfrom(8192)
            if decode_response(data):
                return True
        except socket.timeout:
            pass
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cases", default="cases.csv")
    ap.add_argument("--out", default="simulator_results.json")
    ap.add_argument("--timeout", type=float, default=90, help="seconds to wait per request")
    ap.add_argument("--only", help="comma-separated case names to run (default: all)")
    ap.add_argument("--server-dir", help="folder with Server.py (Content/Encoder); starts the server for you")
    ap.add_argument("--server-script", default="Server.py")
    a = ap.parse_args()

    only = {x.strip() for x in a.only.split(",")} if a.only else None
    cases, warnings = load_cases(a.cases, only)
    for w in warnings:
        print(f"[warning] {w}", file=sys.stderr)
    if not cases:
        sys.exit("No usable rows in cases.csv")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind((HOST, PORT_LISTEN))
    except OSError:
        sys.exit(f"Cannot bind UDP {PORT_LISTEN}: close Unreal / any other client first.")
    sock.settimeout(0.3)

    proc = None
    if not wait_for_server(sock, 3):
        if not a.server_dir:
            sys.exit("Server does not answer on UDP 5000. Start it, or pass --server-dir ../Content/Encoder")
        print("Starting server (loading DE441 can take a minute)...")
        proc = subprocess.Popen([sys.executable, a.server_script], cwd=a.server_dir)
        if not wait_for_server(sock, 240):
            proc.terminate()
            sys.exit("Server did not come up in 240 s.")

    state, results = {}, []
    try:
        for c in cases:
            print(f"-> {c['case']} {c['location']} ...", end=" ", flush=True)
            r = collect_case(sock, c, a.timeout, state)
            if not r:
                print("FAILED (no report for this eclipse / timeout)")
                results.append({**c, "ok": False})
                continue

            expected = expected_contacts(c)
            missing = [k for k in expected if k not in r["events"]]
            note = " [report hit the 1000-byte cap]" if r["truncated"] else ""
            print(f"ok  events={sorted(r['events'])} dT={r['delta_t_s']:.2f}s{note}")

            if missing:
                why = "likely truncated report" if r["truncated"] else "not found in report"
                print(f"   [warning] missing {missing} ({why})")

            # free check of the eclipse classification
            srv_type = (r["meta"].get("type") or "").lower()
            want = c["type_at_location"] or c["type"]
            if want and srv_type and want not in srv_type and srv_type not in want:
                print(f"   [warning] type mismatch: cases.csv says '{want}', server says '{srv_type}'")
            if not srv_type:
                print("   [warning] could not parse 'Type:' from the report (check raw_report)")

            results.append({**c, "ok": True, **r, "expected_contacts": list(expected),
                            "missing_contacts": missing})
    finally:
        sock.close()
        if proc:
            proc.terminate()

    Path(a.out).write_text(json.dumps({"collected_at": time.strftime("%Y-%m-%d %H:%M:%S"),
                                       "cases": results}, indent=2, ensure_ascii=False), encoding="utf-8")
    failed = [r["case"] for r in results if not r.get("ok")]
    print(f"Saved {a.out} ({len(results) - len(failed)} ok, {len(failed)} failed)")
    if failed:
        print("Failed:", ", ".join(failed))


if __name__ == "__main__":
    main()