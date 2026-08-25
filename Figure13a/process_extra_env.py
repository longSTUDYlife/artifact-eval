#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fig. 13(a) Env-2/3/4: CIR → LDE → MVDR → refit distance → print metrics (no plot)."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
DONE = HERE.parent
sys.path.insert(0, str(DONE))
sys.path.insert(0, str(HERE))

from extra_env_config import LOC_PAPER  # noqa: E402
from process_from_cir import process_one_cell  # noqa: E402


def load_bundle(env: int) -> dict:
    npz = DONE / "curve_raw_npy" / "Figure13a" / f"env{env}" / "raw.npz"
    if not npz.is_file():
        raise FileNotFoundError(f"Packed Env-{env} CIR missing: {npz}")
    z = np.load(npz)
    print(f"  CIR source: {npz}")
    return {
        "source": str(npz),
        "layout": "cells",
        "cir": z["cir"],
        "n_frames": z["n_frames"],
        "sequence": z["sequence"],
        "packet_type": z["packet_type"],
        "first_path_amp1": z["first_path_amp1"],
        "ports": z["ports"].astype(int),
        "angles_deg": z["angles_deg"].astype(float),
        "dists_m": z["dists_m"].astype(float),
        "phase_deg": z["phase_deg"].astype(float),
    }


def fit_distance(measured, true_dist, angle):
    measured = np.asarray(measured, dtype=float)
    true_dist = np.asarray(true_dist, dtype=float)
    angle = np.asarray(angle, dtype=float)
    errors = measured - true_dist
    n = errors.size
    a = np.column_stack(
        [
            np.ones(n),
            angle,
            true_dist,
            angle ** 2,
            true_dist ** 2,
            angle * true_dist,
        ]
    )
    coeffs = np.linalg.pinv(a) @ errors
    pred = a @ coeffs
    ss_res = float(np.sum((errors - pred) ** 2))
    ss_tot = float(np.sum((errors - errors.mean()) ** 2))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
    rmse = float(np.sqrt(np.mean((errors - pred) ** 2)))
    names = ("const", "angle", "dist", "angle2", "dist2", "angle_dist")
    coeff = {k: float(v) for k, v in zip(names, coeffs)}
    print(f"  fitted distance correction  R²={r2:.4f}  RMSE={rmse:.4f} m")
    print(
        "  Error = {const:.6f} + {angle:.6f}*A + {dist:.6f}*D + "
        "{angle2:.6f}*A² + {dist2:.6f}*D² + {angle_dist:.6f}*A*D".format(**coeff)
    )
    return coeff


def run_env(env: int, data_dir: Path | None = None) -> dict:
    if env not in (2, 3, 4):
        raise ValueError(f"Fig. 13(a) extra env must be 2, 3, or 4 (got {env})")
    data_dir = Path(data_dir or HERE)
    bundle = load_bundle(env)
    phase = np.deg2rad(bundle["phase_deg"])
    cells = list(zip(bundle["angles_deg"].tolist(), bundle["dists_m"].tolist()))
    lde_dir = data_dir / f"lde_cache_env{env}"

    print(f"=== Fig. 13(a) Env-{env} ===")

    print("\n----- Stage 1: collect measured distance -----")
    meas, true_d, ang_fit = [], [], []
    for ang, dist in cells:
        print(f"  collect {ang:.0f}° / {dist:.0f} m")
        r = process_one_cell(
            bundle,
            data_dir,
            ang,
            dist,
            coeff=None,
            phase_comp=phase,
            apply_correction=False,
            lde_dir=lde_dir,
        )
        if r is None:
            print("    SKIP")
            continue
        m = np.isfinite(r["measured_dist"]) & np.isfinite(r["aoa_cal"])
        meas.append(r["measured_dist"][m])
        true_d.append(np.full(int(m.sum()), r["distance"]))
        ang_fit.append(r["aoa_cal"][m])
        print(f"    n={int(m.sum())}")
    if not meas:
        raise RuntimeError(f"Env-{env} produced no fit points")
    coeff = fit_distance(
        np.concatenate(meas),
        np.concatenate(true_d),
        np.concatenate(ang_fit),
    )

    print("\n----- Stage 3: apply correction -----")
    all_err = []
    for ang, dist in cells:
        print(f"  {ang:.0f}° / {dist:.0f} m")
        r = process_one_cell(
            bundle,
            data_dir,
            ang,
            dist,
            coeff=coeff,
            phase_comp=phase,
            apply_correction=True,
            lde_dir=lde_dir,
        )
        if r is None:
            print("    SKIP")
            continue
        e = r["errors"]
        e = e[np.isfinite(e)]
        all_err.append(e)
        print(
            f"    N={e.size}  med={float(np.median(e)):.3f} m  "
            f"p90={float(np.percentile(e, 90)):.3f} m  "
            f"RMSE={float(np.sqrt(np.mean(e ** 2))):.3f} m  "
            f"AoA={float(np.nanmean(r['aoa_cal'])):.1f}°"
        )

    err = np.concatenate(all_err) if all_err else np.array([])
    n = int(err.size)
    med = float(np.median(err)) if n else float("nan")
    p90 = float(np.percentile(err, 90)) if n else float("nan")
    rmse = float(np.sqrt(np.mean(err ** 2))) if n else float("nan")
    print(
        f"\nFig. 13(a) Env-{env}  "
        f"median = {med*100:.1f} cm, 90th = {p90*100:.1f} cm"
    )
    paper = LOC_PAPER[env]
    print(
        f"expected           "
        f"median = {paper['median_m']*100:.1f} cm, "
        f"90th = {paper['p90_m']*100:.1f} cm"
    )
    out = data_dir / f"localization_errors_8port_env{env}.csv"
    pd.DataFrame({"Localization_Error": err}).to_csv(out, index=False)
    print(f"Saved {out}")
    return {"n": n, "median": med, "p90": p90, "rmse": rmse}


if __name__ == "__main__":
    import argparse

    p = argparse.ArgumentParser(description="Fig. 13(a) extra-env metrics from packed CIR")
    p.add_argument("--env", type=int, required=True, choices=(2, 3, 4))
    args = p.parse_args()
    run_env(args.env)
