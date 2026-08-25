#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fig. 13(e) Env-2/3/4: packed CIR → RA → print tracking metrics (no plot)."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
DONE = HERE.parent
sys.path.insert(0, str(DONE))

from extra_env_config import SENSE_FROM_CIR  # noqa: E402
from sensing_pipeline import (  # noqa: E402
    align_ports,
    compute_ra_maps_stream,
    extract_aoa_from_angle_maps,
    extract_aoa_track_continuous,
    static_clutter_removal,
    sync_by_id,
)


def load_bundle(env: int) -> dict:
    npz = DONE / "curve_raw_npy" / "Figure13e" / f"env{env}" / "raw.npz"
    if not npz.is_file():
        raise FileNotFoundError(f"Packed Env-{env} CIR missing: {npz}")
    z = np.load(npz)
    print(f"  CIR source: {npz}")
    return {
        "source": str(npz),
        "cir": z["cir"],
        "n_frames": z["n_frames"],
        "ports": z["ports"].astype(int),
        "angles_deg": z["angles_deg"].astype(float),
        "trials": z["trials"].astype(int),
        "first_path": z["first_path"],
        "rx_pream_count": z["rx_pream_count"],
        "first_path_amp2": z["first_path_amp2"],
        "packet_type": z["packet_type"],
        "calib": z["calib"],
        "sync": str(z["sync"]),
        "extract": str(z["extract"]),
        "min_range_m": float(z["min_range_m"]),
        "skip_windows": int(z["skip_windows"]),
        "keep_windows": int(z["keep_windows"]),
    }


def perp(aoa, rng, true):
    return np.abs(
        np.asarray(rng, float) * np.sin(np.deg2rad(np.asarray(aoa, float) - true))
    )


def process_one(bundle: dict, iq: int) -> pd.DataFrame | None:
    ang = float(bundle["angles_deg"][iq])
    trial = int(bundle["trials"][iq])
    cir_l, fp_l, rx_l, amp2_l, ptype_l = [], [], [], [], []
    for ip in range(len(bundle["ports"])):
        n = int(bundle["n_frames"][ip, iq])
        if n <= 0:
            return None
        cir_l.append(bundle["cir"][ip, iq, :n])
        fp_l.append(bundle["first_path"][ip, iq, :n])
        rx_l.append(bundle["rx_pream_count"][ip, iq, :n])
        amp2_l.append(bundle["first_path_amp2"][ip, iq, :n])
        ptype_l.append(bundle["packet_type"][ip, iq, :n])

    if bundle["sync"] == "index":
        kept_cir, kept_fp, kept_rx = [], [], []
        for cir, fp, rx, pt in zip(cir_l, fp_l, rx_l, ptype_l):
            m = pt == 1
            kept_cir.append(cir[m])
            kept_fp.append(fp[m])
            kept_rx.append(rx[m])
        n = min(c.shape[0] for c in kept_cir)
        print(f"    index-align {n} frames (PacketType==1, min over 8 ports)")
        if n < 83:
            print("    skip: too few frames")
            return None
        cir_s = [c[:n] for c in kept_cir]
        fp_s = [c[:n] for c in kept_fp]
        rx_s = [c[:n] for c in kept_rx]
    else:
        rows, common = sync_by_id(amp2_l, ptype_l)
        if common.size == 0:
            print("    no common sync ids")
            return None
        print(f"    synced {common.size} frames (seq [{int(common.min())}, {int(common.max())}])")
        cir_s = [c[r] for c, r in zip(cir_l, rows)]
        fp_s = [c[r] for c, r in zip(fp_l, rows)]
        rx_s = [c[r] for c, r in zip(rx_l, rows)]

    rx_aln = align_ports(cir_s, fp_s, rx_s, bundle["calib"])
    if rx_aln.shape[2] < 83:
        print(f"    skip frames={rx_aln.shape[2]}")
        return None
    filtered = static_clutter_removal(rx_aln)
    del rx_aln
    maps, ra, theta = compute_ra_maps_stream(filtered)
    del filtered

    if bundle["extract"] == "track":
        aoa, rng, eng, info = extract_aoa_track_continuous(
            maps,
            ra,
            theta,
            init_min_range=2.0,
            init_windows=25,
            prefer_aoa=ang,
            prefer_aoa_gate=25.0,
            gate_range_m=0.9,
            gate_angle_deg=28.0,
            min_range=1.0,
        )
        print(
            f"    windows={len(aoa)} init_k={info['init_k']} "
            f"r={info['init_range']:.2f} a={info['init_aoa']:.1f} "
            f"assoc={100 * info['assoc_rate']:.0f}%"
        )
        skip = bundle["skip_windows"]
        keep = bundle["keep_windows"]
        if keep > 0:
            aoa, rng, eng = aoa[skip : skip + keep], rng[skip : skip + keep], eng[skip : skip + keep]
            print(f"    paper window skip={skip} keep={len(aoa)}")
    else:
        aoa, rng, eng = extract_aoa_from_angle_maps(
            maps, ra, theta, bundle["min_range_m"]
        )
        mae = float(np.nanmean(np.abs(aoa - ang)))
        print(f"    windows={len(aoa)}  MAE={mae:.2f} deg")
    del maps
    return pd.DataFrame(
        {
            "frame": np.arange(1, len(aoa) + 1),
            "aoa": ang,
            "times": trial,
            "estimated_aoa": aoa,
            "range": rng,
            "energy": eng,
        }
    )


def run_env(env: int, data_dir: Path | None = None) -> dict:
    if env not in (2, 3, 4):
        raise ValueError(f"Fig. 13(e) extra env must be 2, 3, or 4 (got {env})")
    data_dir = Path(data_dir or HERE)
    bundle = load_bundle(env)
    n_pairs = int(bundle["angles_deg"].size)
    print(f"=== Fig. 13(e) Env-{env} ===")

    err_parts, rng_parts = [], []
    for iq in range(n_pairs):
        ang = int(bundle["angles_deg"][iq])
        trial = int(bundle["trials"][iq])
        print(f"\n  {ang:+d}° trial {trial}")
        df = process_one(bundle, iq)
        if df is None:
            continue
        m = np.isfinite(df["estimated_aoa"]) & np.isfinite(df["range"])
        err_parts.append(perp(df.loc[m, "estimated_aoa"], df.loc[m, "range"], ang))
        rng_parts.append(df.loc[m, "range"].to_numpy())

    if not err_parts:
        raise RuntimeError(f"Env-{env} produced no tracks")
    err = np.concatenate(err_parts)
    rng = np.concatenate(rng_parts)
    n = int(err.size)
    med = float(np.median(err))
    p90 = float(np.percentile(err, 90))
    rmse = float(np.sqrt(np.mean(err ** 2)))
    print(
        f"\nFig. 13(e) Env-{env}  "
        f"median = {med*100:.1f} cm, 90th = {p90*100:.1f} cm"
    )
    exp = SENSE_FROM_CIR[env]
    print(
        f"expected           "
        f"median = {exp['median_m']*100:.1f} cm, "
        f"90th = {exp['p90_m']*100:.1f} cm"
    )
    out = data_dir / f"track_errors_8port_env{env}.csv"
    pd.DataFrame({"Track_Error": err, "Range": rng}).to_csv(out, index=False)
    print(f"Saved {out}")
    return {"n": n, "median": med, "p90": p90, "rmse": rmse}


if __name__ == "__main__":
    import argparse

    p = argparse.ArgumentParser(description="Fig. 13(e) extra-env metrics from packed CIR")
    p.add_argument("--env", type=int, required=True, choices=(2, 3, 4))
    args = p.parse_args()
    run_env(args.env)
