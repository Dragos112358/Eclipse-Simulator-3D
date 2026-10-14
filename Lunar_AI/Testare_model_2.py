"""
Evaluare Lunar Altitude NN (contact bands): fp32 vs fp16.
FARA argumente in linia de comanda: editezi blocul CONFIG de mai jos si dai Run.

Note:
 - alt_mean / alt_std se iau din checkpoint (sampler-ul le recalculeaza random la fiecare init).
 - modelul fp16 e incarcat in fp32 (cast din embeddings fp16) => masoara eroarea de
   cuantizare a STOCARII, nu a unui inference in half.
 - "RMSE" din logul de training e sqrt(HuberLoss), nu RMSE real; aici e RMSE real in metri.
"""
import glob
import importlib
import json
import os
import sys
from types import SimpleNamespace

import numpy as np
import torch
import torch.nn as nn
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

THRESHOLDS_M = [1, 2, 5, 10, 25, 50]
TILE_NAMES = ["banda -90°", "banda +90°"]
COLORS = {"fp32": "tab:blue", "fp16": "tab:orange"}


# ---------------------------------------------------------------------------
# Utilitare
# ---------------------------------------------------------------------------
# ===================== CONFIG (editeaza aici) =====================
TRAIN_MODULE = None        # ex. "Fine_tuning_lunar_model"; None = incearca toate fisierele si il alege pe cel ce se potriveste checkpoint-ului
DEM_OVERRIDE = None        # None = foloseste DEM_PATH din fisierul de training
CKPT_FP32 = "lunar_altitude_contact_bands_finetuned.pt"
CKPT_FP16 = "lunar_altitude_contact_bands_fp16_finetuned.pt"   # accepta wildcard (numele era trunchiat)
N_TEST = 2_000_000
SEED = 1234
OUT_DIR = "eval_out"
GRID_LAT = 4096
GRID_LON = 512
PROFILE_LATS = None        # ex. [40.0, -10.0, 25.0]; None = 3 latitudini aleatoare
SLOPE_STEP_PX = 1.0
# ==================================================================

HERE = os.path.dirname(os.path.abspath(__file__))


def list_train_modules():
    """Toate .py-urile din folder care definesc LunarAltitudeContactBandsNet."""
    me = os.path.basename(__file__)
    out = []
    for f in sorted(os.listdir(HERE)):
        if f.endswith(".py") and f != me:
            try:
                with open(os.path.join(HERE, f), encoding="utf-8") as fh:
                    if "class LunarAltitudeContactBandsNet" in fh.read():
                        out.append(f[:-3])
            except (OSError, UnicodeDecodeError):
                pass
    return out


def resolve_ckpt(pattern):
    hits = sorted(glob.glob(pattern))
    if not hits:
        raise SystemExit(f"Nu gasesc checkpoint-ul '{pattern}' in {os.getcwd()}")
    return hits[0]


def get_config():
    return SimpleNamespace(
        module=TRAIN_MODULE, dem=DEM_OVERRIDE, fp32=CKPT_FP32, fp16=CKPT_FP16,
        n=N_TEST, seed=SEED, out=OUT_DIR, grid_lat=GRID_LAT, grid_lon=GRID_LON,
        profile_lats=PROFILE_LATS, slope_step_px=SLOPE_STEP_PX)


def ck_summary(sd):
    emb = [k for k in sd if k.startswith("encoding.embeddings.")]
    return (f"nivele={len(emb)}, in_dim_mlp={tuple(sd['mlp.0.weight'].shape)[1]}, "
            f"max_tabel={max(sd[k].shape[0] for k in emb)}")


def pick_train_module(ck, forced=None):
    """Alege fisierul de training a carui arhitectura se potriveste EXACT (toti tensorii, aceleasi forme)
    cu checkpoint-ul. Fara adaptari silentioase: daca nu se potriveste nimic, afiseaza diagnostic si iese."""
    names = [forced] if forced else list_train_modules()
    ck_sd = ck["model_state"]
    report = []
    for name in names:
        try:
            T = importlib.import_module(name)
            model = T.LunarAltitudeContactBandsNet(ck["recommended_lon_cells"])
        except Exception as e:  # noqa: BLE001
            report.append(f"  {name}: nu pot construi modelul ({e!r})")
            continue
        ms = {k: tuple(v.shape) for k, v in model.state_dict().items()}
        cs = {k: tuple(v.shape) for k, v in ck_sd.items()}
        bad = [k for k in set(ms) | set(cs) if ms.get(k) != cs.get(k)]
        if not bad:
            return T
        report.append(f"  {name}: {len(bad)} tensori nepotriviti | model: {ck_summary(model.state_dict())}")
        del model
    print(f"\nCheckpoint: {ck_summary(ck_sd)}")
    print("Niciun fisier de training nu se potriveste exact:")
    print("\n".join(report) or "  (nu am gasit niciun fisier cu LunarAltitudeContactBandsNet)")
    raise SystemExit("Fisierul cu care a fost antrenat checkpoint-ul nu e in folder sau a fost modificat "
                     "dupa antrenare. Restaureaza-l / pune-l aici, sau trimite-mi constantele lui.")


def load_model(T, ck, device):
    model = T.LunarAltitudeContactBandsNet(ck["recommended_lon_cells"])
    model.load_state_dict(ck["model_state"])  # strict; fp16 -> fp32 cast automat
    return model.to(device).eval(), ck


@torch.no_grad()
def predict(model, u, v, mean, std, device, bs=1 << 18):
    u = np.ascontiguousarray(u, dtype=np.float32)
    v = np.ascontiguousarray(v, dtype=np.float32)
    out = np.empty(len(u), dtype=np.float64)
    for i in range(0, len(u), bs):
        ut = torch.from_numpy(u[i:i + bs]).to(device)
        vt = torch.from_numpy(v[i:i + bs]).to(device)
        p = model(ut, vt, active_frac=1.0).float().cpu().numpy().astype(np.float64)
        out[i:i + bs] = p * std + mean
    return out


def make_set(sampler, n, kind, seed, centers):
    np.random.seed(seed)
    if kind == "uniform":
        lon, lat = sampler._uniform_area_lonlat_in_bands(n)
    else:
        lon, lat = sampler._biased_lonlat_in_bands(n)
    gt = sampler._sample_bilinear(lon, lat)
    ok = np.isfinite(gt)
    lon, lat, gt = lon[ok], lat[ok], gt[ok].astype(np.float64)
    u, v, _ = sampler.lonlat_to_uv(lon, lat)
    tile = (np.sin(np.radians(lon.astype(np.float64))) > 0).astype(int)  # 0 = -90, 1 = +90
    center = np.array(centers)[tile]
    lon_local = ((lon - center + 180.0) % 360.0) - 180.0
    return dict(lon=lon, lat=lat, u=u, v=v, gt=gt, tile=tile, lon_local=lon_local)


def compute_metrics(gt, pred):
    err = pred - gt
    a = np.abs(err)
    pc = np.percentile(a, [50, 90, 95, 99, 99.9])
    ss_tot = np.sum((gt - gt.mean()) ** 2)
    out = {
        "RMSE_m": float(np.sqrt(np.mean(err ** 2))),
        "MAE_m": float(a.mean()),
        "bias_m": float(err.mean()),
        "P50_m": float(pc[0]), "P90_m": float(pc[1]), "P95_m": float(pc[2]),
        "P99_m": float(pc[3]), "P99.9_m": float(pc[4]),
        "max_m": float(a.max()),
        "R2": float(1.0 - np.sum(err ** 2) / ss_tot),
    }
    for t in THRESHOLDS_M:
        out[f"<{t}m_%"] = float((a < t).mean() * 100.0)
    return out


def print_table(title, mets):
    names = list(mets)
    keys = list(mets[names[0]])
    print(f"\n=== {title} ===")
    print(f"{'metrica':<12}" + "".join(f"{m:>14}" for m in names))
    for k in keys:
        print(f"{k:<12}" + "".join(f"{mets[m][k]:>14.4f}" for m in names))


def binned_rmse(x, err, edges):
    nb = len(edges) - 1
    idx = np.clip(np.digitize(x, edges) - 1, 0, nb - 1)
    cnt = np.bincount(idx, minlength=nb)
    s2 = np.bincount(idx, weights=err ** 2, minlength=nb)
    s1 = np.bincount(idx, weights=err, minlength=nb)
    with np.errstate(invalid="ignore", divide="ignore"):
        return np.sqrt(s2 / cnt), s1 / cnt


def mae_map(lon_local, lat, err, w, lat_min, lat_max, nx, ny):
    rng = [[-w, w], [lat_min, lat_max]]
    H, _, _ = np.histogram2d(lon_local, lat, bins=[nx, ny], range=rng, weights=np.abs(err))
    C, _, _ = np.histogram2d(lon_local, lat, bins=[nx, ny], range=rng)
    with np.errstate(invalid="ignore", divide="ignore"):
        return (H / C).T


# ---------------------------------------------------------------------------
# Test de gradient (relevant pt profilul limbului / Baily's beads)
# ---------------------------------------------------------------------------
def slope_test(sampler, models, mean, std, device, n, seed, step_px):
    np.random.seed(seed)
    lon, lat = sampler._uniform_area_lonlat_in_bands(n)
    d = step_px * 360.0 / sampler.width
    res = {}
    for axis in ("lon", "lat"):
        lon2 = lon + d if axis == "lon" else lon
        lat2 = lat + d if axis == "lat" else lat
        g1 = sampler._sample_bilinear(lon, lat)
        g2 = sampler._sample_bilinear(lon2, lat2)
        u1, v1, _ = sampler.lonlat_to_uv(lon, lat)
        u2, v2, oob2 = sampler.lonlat_to_uv(lon2, lat2)
        ok = np.isfinite(g1) & np.isfinite(g2) & ~oob2 & (lat2 <= sampler.lat_max)
        g_slope = (g2 - g1)[ok].astype(np.float64)
        res[axis] = {"GT_slope_RMS_m": float(np.sqrt(np.mean(g_slope ** 2)))}
        for name, m in models.items():
            p1 = predict(m, u1[ok], v1[ok], mean, std, device)
            p2 = predict(m, u2[ok], v2[ok], mean, std, device)
            e = (p2 - p1) - g_slope
            res[axis][f"{name}_slope_RMSE_m"] = float(np.sqrt(np.mean(e ** 2)))
    return res


# ---------------------------------------------------------------------------
# Grafice
# ---------------------------------------------------------------------------
def fig_error_dist(gt, preds, path):
    fig, ax = plt.subplots(1, 2, figsize=(13, 4.5))
    lim = np.percentile(np.abs(preds["fp32"] - gt), 99.9)
    bins = np.linspace(-lim, lim, 201)
    for m, p in preds.items():
        e = p - gt
        ax[0].hist(e, bins=bins, histtype="step", lw=1.5, color=COLORS[m], label=m)
        a = np.sort(np.abs(e))
        idx = np.linspace(0, len(a) - 1, 3000).astype(int)
        ax[1].plot(a[idx], (idx + 1) / len(a) * 100, color=COLORS[m], label=m)
    ax[0].set_yscale("log"); ax[0].set_xlabel("eroare (pred - GT) [m]"); ax[0].set_ylabel("count")
    ax[0].set_title("Distributia erorii"); ax[0].legend()
    ax[1].set_xscale("log"); ax[1].set_xlabel("|eroare| [m]"); ax[1].set_ylabel("% puncte cu eroare <= x")
    ax[1].set_title("CDF eroare absoluta"); ax[1].grid(alpha=0.3, which="both"); ax[1].legend()
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_pred_vs_gt(gt, preds, path):
    sub = np.random.RandomState(0).choice(len(gt), min(200_000, len(gt)), replace=False)
    fig, ax = plt.subplots(1, 2, figsize=(13, 5))
    hb = ax[0].hexbin(gt[sub], preds["fp32"][sub], gridsize=120, bins="log", mincnt=1, cmap="viridis")
    lo, hi = gt.min(), gt.max()
    ax[0].plot([lo, hi], [lo, hi], "r--", lw=0.8)
    ax[0].set_xlabel("GT [m]"); ax[0].set_ylabel("pred fp32 [m]"); ax[0].set_title("Pred vs GT (fp32)")
    fig.colorbar(hb, ax=ax[0], label="log10(count)")
    d = preds["fp16"] - preds["fp32"]
    ax[1].hist(d, bins=200, color="tab:purple")
    ax[1].set_yscale("log"); ax[1].set_xlabel("pred fp16 - pred fp32 [m]")
    ax[1].set_title(f"Efect cuantizare fp16 (RMS={np.sqrt(np.mean(d**2)):.4f} m, max={np.abs(d).max():.3f} m)")
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_binned(s, preds, w, lat_min, lat_max, path):
    gt = s["gt"]
    fig, ax = plt.subplots(1, 3, figsize=(17, 4.5))
    tile_col = ["tab:green", "tab:red"]
    e_lat = np.linspace(lat_min, lat_max, 37)
    e_lon = np.linspace(-w, w, 21)
    c_lat, c_lon = 0.5 * (e_lat[1:] + e_lat[:-1]), 0.5 * (e_lon[1:] + e_lon[:-1])
    for t in (0, 1):
        mk = s["tile"] == t
        for m, p in preds.items():
            e = (p - gt)[mk]
            ls = "-" if m == "fp32" else "--"
            r, _ = binned_rmse(s["lat"][mk], e, e_lat)
            ax[0].plot(c_lat, r, ls, color=tile_col[t], label=f"{TILE_NAMES[t]} {m}")
            r, _ = binned_rmse(s["lon_local"][mk], e, e_lon)
            ax[1].plot(c_lon, r, ls, color=tile_col[t], label=f"{TILE_NAMES[t]} {m}")
    ax[0].set_xlabel("latitudine [°]"); ax[0].set_ylabel("RMSE [m]"); ax[0].set_title("RMSE vs latitudine")
    ax[1].set_xlabel("lon - centru banda [°]"); ax[1].set_title("RMSE vs pozitie in banda (efecte de margine)")
    edges = np.unique(np.percentile(gt, np.linspace(0, 100, 21)))
    ctr = 0.5 * (edges[1:] + edges[:-1])
    for m, p in preds.items():
        r, b = binned_rmse(gt, p - gt, edges)
        ax[2].plot(ctr, r, "-" if m == "fp32" else "--", color=COLORS[m], label=f"RMSE {m}")
        if m == "fp32":
            ax[2].plot(ctr, b, ":", color="k", label="bias fp32")
    ax[2].axhline(0, color="gray", lw=0.5)
    ax[2].set_xlabel("altitudine GT [m]"); ax[2].set_title("Eroare vs altitudine (regresie spre medie?)")
    for a in ax:
        a.grid(alpha=0.3); a.legend(fontsize=7)
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


def fig_heatmaps(s, preds, w, lat_min, lat_max, path):
    nx, ny = max(2, int(round(2 * w))), 90
    maps = {}
    for t in (0, 1):
        mk = s["tile"] == t
        for m, p in preds.items():
            maps[(t, m)] = mae_map(s["lon_local"][mk], s["lat"][mk], (p - s["gt"])[mk],
                                   w, lat_min, lat_max, nx, ny)
    allv = np.concatenate([x[np.isfinite(x)] for x in maps.values()])
    vmax = np.percentile(allv, 99)
    fig, ax = plt.subplots(2, 2, figsize=(11, 14))
    im = None
    for i, t in enumerate((0, 1)):
        for j, m in enumerate(preds):
            im = ax[i, j].imshow(maps[(t, m)], origin="lower", aspect="auto", cmap="magma",
                                 extent=[-w, w, lat_min, lat_max], vmin=0, vmax=vmax)
            ax[i, j].set_title(f"MAE {m} - {TILE_NAMES[t]}")
            ax[i, j].set_xlabel("lon - centru [°]"); ax[i, j].set_ylabel("lat [°]")
    fig.colorbar(im, ax=ax, label="MAE [m]", shrink=0.6)
    fig.savefig(path, dpi=130); plt.close(fig)


def fig_reconstruction(sampler, models, mean, std, device, centers, w, tile, gl, gn, path):
    lons = np.linspace(centers[tile] - w, centers[tile] + w, gn)
    lats = np.linspace(sampler.lat_max, sampler.lat_min, gl)  # nord sus
    LON, LAT = np.meshgrid(lons, lats)
    lon, lat = LON.ravel(), LAT.ravel()
    gt = sampler._sample_bilinear(lon, lat).reshape(gl, gn)
    u, v, _ = sampler.lonlat_to_uv(lon, lat)
    pr = {m: predict(mod, u, v, mean, std, device).reshape(gl, gn) for m, mod in models.items()}
    ext = [-w, w, sampler.lat_min, sampler.lat_max]
    lo, hi = np.nanpercentile(gt, [1, 99])
    err32 = np.abs(pr["fp32"] - gt)
    dq = np.abs(pr["fp16"] - pr["fp32"])
    fig, ax = plt.subplots(1, 4, figsize=(17, 12))
    panels = [(gt, "GT", "gray", lo, hi), (pr["fp32"], "pred fp32", "gray", lo, hi),
              (err32, "|eroare| fp32 [m]", "magma", 0, np.percentile(err32, 99)),
              (dq, "|fp16 - fp32| [m]", "viridis", 0, max(np.percentile(dq, 99), 1e-6))]
    for a, (img, title, cm, vmin, vmax) in zip(ax, panels):
        im = a.imshow(img, extent=ext, aspect="auto", cmap=cm, vmin=vmin, vmax=vmax)
        a.set_title(title); a.set_xlabel("lon - centru [°]")
        fig.colorbar(im, ax=a, shrink=0.5)
    ax[0].set_ylabel("lat [°]")
    fig.suptitle(f"Reconstructie {TILE_NAMES[tile]} (grid {gl}x{gn})")
    fig.tight_layout(); fig.savefig(path, dpi=110); plt.close(fig)


def fig_profiles(sampler, models, mean, std, device, centers, lats0, path, win_deg=2.0):
    step = 0.5 * 360.0 / sampler.width
    n_pts = int(win_deg / step)
    fig, axes = plt.subplots(2, len(lats0), figsize=(5.5 * len(lats0), 7.5), squeeze=False)
    for t in (0, 1):
        for j, lat0 in enumerate(lats0):
            lat = lat0 + np.arange(n_pts) * step
            lon = np.full(n_pts, centers[t], dtype=np.float64)
            gt = sampler._sample_bilinear(lon, lat)
            u, v, _ = sampler.lonlat_to_uv(lon, lat)
            a = axes[t, j]
            a.plot(lat, gt, "k", lw=1.6, label="GT")
            for m, mod in models.items():
                p = predict(mod, u, v, mean, std, device)
                rmse = np.sqrt(np.mean((p - gt) ** 2))
                a.plot(lat, p, "-" if m == "fp32" else "--", color=COLORS[m], lw=1.1,
                       label=f"{m} (RMSE {rmse:.2f} m)")
            a.set_title(f"{TILE_NAMES[t]}, lon={centers[t]:+.0f}°")
            a.set_xlabel("lat [°]"); a.set_ylabel("altitudine [m]"); a.grid(alpha=0.3); a.legend(fontsize=7)
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    os.chdir(HERE)  # caile relative (DEM, checkpoint-uri) se rezolva fata de folderul scriptului
    sys.path.insert(0, HERE)
    args = get_config()
    device = "cuda" if torch.cuda.is_available() else "cpu"
    os.makedirs(args.out, exist_ok=True)
    print(f"Device: {device}")
    if not (args.fp32 and args.fp16):
        raise SystemExit("Seteaza CKPT_FP32 si CKPT_FP16 in blocul CONFIG.")
    paths = {"fp32": resolve_ckpt(args.fp32), "fp16": resolve_ckpt(args.fp16)}
    raw = {n: torch.load(p, map_location="cpu", weights_only=False) for n, p in paths.items()}
    T = pick_train_module(raw["fp32"], args.module)
    print(f"Fisier de training potrivit checkpoint-ului: {T.__name__}")
    dem = args.dem or T.DEM_PATH

    models, cks = {}, {}
    for name, p in paths.items():
        models[name], cks[name] = load_model(T, raw[name], device)
        print(f"[{name}] {p} | {os.path.getsize(p)/1024**2:.1f} MB pe disc | "
              f"metrica salvata la training={cks[name]['best_val_rmse_m']:.2f} m (sqrt-Huber, nu RMSE real)")

    ck = cks["fp32"]
    mean, std = ck["alt_mean"], ck["alt_std"]
    w, centers = ck["band_half_width_deg"], ck["limb_centers_deg"]
    sampler = T.LunarDEMSamplerContactBands(dem, band_half_width_deg=w)
    T.print_memory_report(models["fp32"])

    sets = {
        "uniform": make_set(sampler, args.n, "uniform", args.seed, centers),
        "focus": make_set(sampler, args.n // 2, "focus", args.seed + 1, centers),
    }
    report, uni_preds = {}, None

    for sname, s in sets.items():
        preds = {m: predict(mod, s["u"], s["v"], mean, std, device) for m, mod in models.items()}
        mets = {m: compute_metrics(s["gt"], preds[m]) for m in models}
        print_table(f"set '{sname}' (n={len(s['gt'])})", mets)
        d = preds["fp16"] - preds["fp32"]
        qd = {"rms_m": float(np.sqrt(np.mean(d ** 2))), "max_m": float(np.abs(d).max()),
              "P99.9_m": float(np.percentile(np.abs(d), 99.9))}
        print(f"fp16 vs fp32 (diferenta directa): RMS={qd['rms_m']:.5f} m | "
              f"P99.9={qd['P99.9_m']:.4f} m | max={qd['max_m']:.4f} m")
        report[sname] = {"metrics": mets, "fp16_vs_fp32": qd}

        if sname == "uniform":
            uni_preds = preds
            for t in (0, 1):
                mk = s["tile"] == t
                line = " | ".join(
                    f"{m}: RMSE={compute_metrics(s['gt'][mk], preds[m][mk])['RMSE_m']:.3f} "
                    f"MAE={compute_metrics(s['gt'][mk], preds[m][mk])['MAE_m']:.3f}"
                    for m in models)
                print(f"  {TILE_NAMES[t]} (n={int(mk.sum())}): {line}")

    sl = slope_test(sampler, models, mean, std, device, min(args.n, 1_000_000),
                    args.seed + 2, args.slope_step_px)
    print(f"\n=== Test gradient (pas = {args.slope_step_px} px DEM) ===")
    for axis, r in sl.items():
        print(f"  {axis}: " + " | ".join(f"{k}={v:.3f}" for k, v in r.items()))
    report["slope"] = sl

    s = sets["uniform"]
    fig_error_dist(s["gt"], uni_preds, f"{args.out}/01_error_distribution.png")
    fig_pred_vs_gt(s["gt"], uni_preds, f"{args.out}/02_pred_vs_gt.png")
    fig_binned(s, uni_preds, w, sampler.lat_min, sampler.lat_max, f"{args.out}/03_binned_errors.png")
    fig_heatmaps(s, uni_preds, w, sampler.lat_min, sampler.lat_max, f"{args.out}/04_error_heatmaps.png")
    for t in (0, 1):
        fig_reconstruction(sampler, models, mean, std, device, centers, w, t,
                           args.grid_lat, args.grid_lon, f"{args.out}/05_reconstruction_tile{t}.png")
    lats0 = args.profile_lats or list(np.random.RandomState(args.seed).uniform(
        sampler.lat_min + 1, sampler.lat_max - 3, 3))
    fig_profiles(sampler, models, mean, std, device, centers, lats0, f"{args.out}/06_limb_profiles.png")

    with open(f"{args.out}/metrics.json", "w") as f:
        json.dump(report, f, indent=2)
    print(f"\nGata. Grafice + metrics.json in ./{args.out}/")


if __name__ == "__main__":
    main()