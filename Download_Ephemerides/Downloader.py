#!/usr/bin/env python3
"""
Download the ephemeris / Earth-orientation files required by Server.exe
(3D Eclipse Simulator backend) and build a single de441.bsp.

Files go into <project>/Content/Encoder (found automatically by walking up
from this script; created next to the project root if missing).

Usage (Linux / WSL / Windows / macOS):
    python3 Downloader.py                 # download + merge -> Content/Encoder
    python3 Downloader.py --dir /path/to/Content/Encoder
    python3 Downloader.py --skip-de441    # only the small files
    python3 Downloader.py --keep-parts    # keep de441_part-*.bsp after merging

de441: NAIF only publishes de441_part-1.bsp and de441_part-2.bsp. This script
downloads both, downloads NAIF's `spkmerge` utility (to a temp folder), merges
the parts into de441.bsp and deletes the parts (unless --keep-parts).
If de441.bsp already exists nothing is downloaded or merged for it.

Features: standard library only (no pip install), skips files that are
already complete, resumes interrupted downloads (.part files).
"""
import argparse
import platform
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

MB = 1024 * 1024
NAIF = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/"
NAIF_UTILS = "https://naif.jpl.nasa.gov/pub/naif/utilities/"
LSK_URL = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls"

# file name -> (url, minimum expected size in MB, used to detect truncated files)
FILES = {
    "finals2000A.all": ("https://datacenter.iers.org/data/9/finals2000A.all", 3),
    "jup365.bsp": (NAIF + "satellites/jup365.bsp", 1000),
    "sat441.bsp": (NAIF + "satellites/sat441.bsp", 600),
    "nep097.bsp": (NAIF + "satellites/nep097.bsp", 90),
}

# Optional: direct link to an already merged de441.bsp (Zenodo, Hugging Face...).
# If set, it is used instead of downloading the parts + merging.
DE441_URL = ""
DE441_MIN_MB = 2500
DE441_PARTS = {
    "de441_part-1.bsp": NAIF + "planets/de441_part-1.bsp",
    "de441_part-2.bsp": NAIF + "planets/de441_part-2.bsp",
}
DE441_PART_MIN_MB = 1400

CHUNK = 1024 * 1024
HEADERS = {"User-Agent": "EclipseSimulator-ephemeris-downloader/1.0"}


def default_dir():
    here = Path(__file__).resolve().parent
    if here.name == "Encoder":
        return here
    for p in (here, *here.parents):
        if (p / "Content" / "Encoder").is_dir():
            return p / "Content" / "Encoder"
    # fallback: script is in a subfolder of the project
    return here.parent / "Content" / "Encoder"


def download(url, dest, min_mb):
    name = dest.name
    min_bytes = min_mb * MB

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


def spkmerge_source():
    """(url, local file name) of NAIF's spkmerge for this platform, or None."""
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Windows":
        return NAIF_UTILS + "PC_Windows_64bit/spkmerge.exe", "spkmerge.exe"
    if system == "Darwin":
        folder = ("MacM1_OSX_AppleC_64bit" if machine in ("arm64", "aarch64")
                  else "MacIntel_OSX_AppleC_64bit")
        return NAIF_UTILS + folder + "/spkmerge", "spkmerge"
    if system == "Linux" and machine in ("x86_64", "amd64"):
        return NAIF_UTILS + "PC_Linux_64bit/spkmerge", "spkmerge"
    return None


def merge_de441(dest_dir, keep_parts):
    final = dest_dir / "de441.bsp"
    merged = dest_dir / "de441.bsp"
    setup = dest_dir / "de441_merge_setup.txt"
    parts = [dest_dir / n for n in DE441_PARTS]

    for p in parts:
        if not p.exists() or p.stat().st_size < DE441_PART_MIN_MB * MB:
            print(f"[error] {p.name} missing or incomplete, cannot merge.")
            return False

    src = spkmerge_source()
    if src is None:
        print(f"[error] no spkmerge build for {platform.system()} {platform.machine()}.")
        print("Get it from https://naif.jpl.nasa.gov/naif/utilities.html and merge manually.")
        return False
    url, exe_name = src

    # spkmerge goes in a temp dir (on WSL /tmp supports the exec bit, /mnt/c may not).
    with tempfile.TemporaryDirectory(prefix="spkmerge_") as td:
        exe = Path(td) / exe_name
        print("Downloading spkmerge...")
        if not download(url, exe, 0.1):
            return False
        exe.chmod(exe.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

        # spkmerge always requires a LEAPSECONDS_KERNEL command. Kept next to the
        # output (relative name, avoids path-length limits) and removed afterwards.
        lsk = dest_dir / "naif0012.tls"
        lsk_existed = lsk.exists()
        print("Downloading leapseconds kernel...")
        if not download(LSK_URL, lsk, 0.001):
            return False

        merged.unlink(missing_ok=True)  # spkmerge refuses to overwrite
        # No BODIES / BEGIN_TIME / END_TIME => all bodies, full coverage.
        setup.write_text(
            f"LEAPSECONDS_KERNEL = {lsk.name}\n"
            f"SPK_KERNEL = {merged.name}\n"
            + "".join(f"SOURCE_SPK_KERNEL = {p.name}\n" for p in parts)
        )
        print("Merging de441 parts with spkmerge (can take several minutes)...")
        try:
            res = subprocess.run([str(exe), setup.name], cwd=dest_dir,
                                 capture_output=True, text=True)
        finally:
            setup.unlink(missing_ok=True)
            if not lsk_existed:
                lsk.unlink(missing_ok=True)

    expected = sum(p.stat().st_size for p in parts)
    if res.returncode != 0 or not merged.exists() or merged.stat().st_size < 0.95 * expected:
        print("[error] merge failed.")
        print((res.stdout + res.stderr).strip()[-1500:])
        merged.unlink(missing_ok=True)
        return False

    merged.replace(final)
    print(f"[ok] de441.bsp ({final.stat().st_size / 1e9:.2f} GB)")
    if not keep_parts:
        for p in parts:
            p.unlink(missing_ok=True)
        print("Removed de441_part-*.bsp (use --keep-parts to keep them).")
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", type=Path, default=default_dir(),
                    help="destination folder (default: <project>/Content/Encoder)")
    ap.add_argument("--skip-de441", action="store_true",
                    help="do not download/merge de441 (the ~3 GB planetary ephemeris)")
    ap.add_argument("--keep-parts", action="store_true",
                    help="keep de441_part-*.bsp after a successful merge")
    args = ap.parse_args()

    dest_dir = args.dir.expanduser().resolve()
    dest_dir.mkdir(parents=True, exist_ok=True)
    print(f"Destination: {dest_dir}")

    de441 = dest_dir / "de441.bsp"
    de441_present = de441.exists() and de441.stat().st_size >= DE441_MIN_MB * MB

    free_gb = shutil.disk_usage(dest_dir).free / 1e9
    need_gb = 2 if (args.skip_de441 or de441_present) else 9  # parts + merged + small files
    if free_gb < need_gb:
        print(f"[warning] only {free_gb:.1f} GB free, about {need_gb} GB recommended.")

    ok = True
    for name, (url, min_mb) in FILES.items():
        ok &= download(url, dest_dir / name, min_mb)

    if not args.skip_de441:
        if de441_present:
            print("[ok] de441.bsp already present")
        elif DE441_URL:
            ok &= download(DE441_URL, de441, DE441_MIN_MB)
        else:
            parts_ok = True
            for name, url in DE441_PARTS.items():
                parts_ok &= download(url, dest_dir / name, DE441_PART_MIN_MB)
            ok &= parts_ok
            if parts_ok:
                ok &= merge_de441(dest_dir, args.keep_parts)

    print("\nDone." if ok else "\nSome steps failed, run the script again.")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()