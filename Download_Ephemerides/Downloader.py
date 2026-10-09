#!/usr/bin/env python3
"""
Download the ephemeris / Earth-orientation files required by Server.exe
(3D Eclipse Simulator backend).

By default the files go into <project>/Content/Encoder:
  - if this script lives in Content/Encoder, that folder is used;
  - otherwise (script in the project root) <script folder>/Content/Encoder
    is used and created if missing.

Usage (Linux / WSL / Windows):
    python3 download_ephemerides.py                 # -> Content/Encoder
    python3 download_ephemerides.py --dir /path/to/Content/Encoder
    python3 download_ephemerides.py --skip-de441    # only the small files

WSL example (project on the Windows drive):
    cd /mnt/c/Users/<you>/Desktop/Eclipse-Simulator-3D
    python3 download_ephemerides.py

Features: standard library only (no pip install), skips files that are
already complete, resumes interrupted downloads (.part files).
"""
import argparse
import shutil
import sys
import urllib.error
import urllib.request
from pathlib import Path

NAIF = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/"

# file name -> (url, minimum expected size in MB, used to detect truncated files)
FILES = {
    "finals2000A.all": ("https://datacenter.iers.org/data/9/finals2000A.all", 3),
    "jup365.bsp": (NAIF + "satellites/jup365.bsp", 1000),
    "sat441.bsp": (NAIF + "satellites/sat441.bsp", 600),
    "nep097.bsp": (NAIF + "satellites/nep097.bsp", 90),
}

# The merged de441.bsp (~3.2 GB) is NOT published by NASA NAIF; they only
# provide de441_part-1.bsp and de441_part-2.bsp. If you host your merged
# file somewhere (Zenodo, Hugging Face, ...), put the direct link here:
DE441_URL = ""  # <- fill in
DE441_PARTS = {
    "de441_part-1.bsp": NAIF + "planets/de441_part-1.bsp",
    "de441_part-2.bsp": NAIF + "planets/de441_part-2.bsp",
}

CHUNK = 1024 * 1024
HEADERS = {"User-Agent": "EclipseSimulator-ephemeris-downloader/1.0"}


def default_dir():
    here = Path(__file__).resolve().parent
    if here.name == "Encoder":
        return here
    for p in (here, *here.parents):
        if (p / "Content" / "Encoder").is_dir():
            return p / "Content" / "Encoder"
    # fallback: scriptul e într-un subfolder al proiectului
    return here.parent / "Content" / "Encoder"


def download(url, dest, min_mb):
    name = dest.name
    min_bytes = min_mb * 1024 * 1024

    if dest.exists() and dest.stat().st_size >= min_bytes:
        print(f"[ok] {name} already present")
        return True

    part = dest.with_name(name + ".part")
    for attempt in range(2):
        done = part.stat().st_size if part.exists() else 0
        req = urllib.request.Request(url, headers=dict(HEADERS))
        if done:
            req.add_header("Range", f"bytes={done}-")
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                resumed = bool(done) and r.status == 206
                total = int(r.headers.get("Content-Length", 0)) + (done if resumed else 0)
                got = done if resumed else 0
                with open(part, "ab" if resumed else "wb") as f:
                    while True:
                        chunk = r.read(CHUNK)
                        if not chunk:
                            break
                        f.write(chunk)
                        got += len(chunk)
                        if total:
                            print(f"\r{name}: {got / 1e6:9.1f} / {total / 1e6:.1f} MB "
                                  f"({100 * got / total:5.1f}%)", end="", flush=True)
            print()
            break
        except urllib.error.HTTPError as e:
            if e.code == 416 and attempt == 0:  # stale .part file, start over
                part.unlink(missing_ok=True)
                continue
            print(f"[error] {name}: HTTP {e.code} {e.reason}")
            return False
        except (urllib.error.URLError, OSError) as e:
            print(f"\n[error] {name}: {e}\nRun the script again to resume.")
            return False

    if not part.exists() or part.stat().st_size < min_bytes:
        print(f"[error] {name} looks incomplete, run the script again to resume.")
        return False
    part.replace(dest)
    print(f"[ok] {name}")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, default=default_dir(),
                    help="destination folder (default: <project>/Content/Encoder)")
    ap.add_argument("--skip-de441", action="store_true",
                    help="do not download de441 (the ~3 GB planetary ephemeris)")
    args = ap.parse_args()

    dest_dir = args.dir.expanduser().resolve()
    dest_dir.mkdir(parents=True, exist_ok=True)
    print(f"Destination: {dest_dir}")

    free_gb = shutil.disk_usage(dest_dir).free / 1e9
    need_gb = 2 if args.skip_de441 else 6
    if free_gb < need_gb:
        print(f"[warning] only {free_gb:.1f} GB free, about {need_gb} GB recommended.")

    ok = True
    for name, (url, min_mb) in FILES.items():
        ok &= download(url, dest_dir / name, min_mb)

    if not args.skip_de441:
        if (dest_dir / "de441.bsp").exists():
            print("[ok] de441.bsp already present")
        elif DE441_URL:
            ok &= download(DE441_URL, dest_dir / "de441.bsp", 3000)
        else:
            print("\n[note] de441.bsp is missing and DE441_URL is not set.")
            print("Downloading the two official NAIF parts instead. Server.exe expects a")
            print("single de441.bsp, so merge them (spkmerge) or host your merged file")
            print("and set DE441_URL in this script.")
            for name, url in DE441_PARTS.items():
                ok &= download(url, dest_dir / name, 1400)

    print("\nDone." if ok else "\nSome files failed, run the script again.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()