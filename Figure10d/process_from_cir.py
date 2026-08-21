#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure10d: CIR → row-align → clutter → angle-FFT RA slice (window 14, ~3.18 m).

Pure Python port of batch_extract_slice.m (20260218 tworef aoa=0 times=3).
Data source (first match):
  1) ../curve_raw_npy/Figure10d/8RX-ULA/raw.npz
  2) raw/antenna_data_port*_8ports_sensing_car_square_tworef_0_3.csv
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
DONE = HERE.parent
sys.path.insert(0, str(DONE))

from sensing_pipeline import (  # noqa: E402
    align_ports,
    parse_complex_list,
    ra_one_window,
    static_clutter_removal,
    theta_and_range_axes,
)

TARGET_WIN = 14
TARGET_RANGE = 3.1828125
MIN_RANGE_ZERO = 0.6
CONFIGS = [
    ("8port", list(range(1, 9))),
    ("4port", list(range(1, 5))),
    ("2port", list(range(1, 3))),
]


def _load_csv_port(path: Path) -> dict:
    df = pd.read_csv(path)
    real_cols = sorted(
        [c for c in df.columns if c.startswith("CIR_real_")],
        key=lambda c: int(c.split("_")[-1]),
    )
    imag_cols = sorted(
        [c for c in df.columns if c.startswith("CIR_imag_")],
        key=lambda c: int(c.split("_")[-1]),
    )
    cir = df[real_cols].to_numpy(np.float32) + 1j * df[imag_cols].to_numpy(np.float32)
    return {
        "cir": cir.astype(np.complex64),
        "first_path": df["firstPath"].to_numpy(np.float64) if "firstPath" in df.columns else np.zeros(len(df)),
        "rx_pream_count": (
            df["rxPreamCount"].to_numpy(np.float64) if "rxPreamCount" in df.columns else np.ones(len(df))
        ),
        "n": len(df),
    }


def load_bundle(data_dir: Path) -> dict:
    npz = DONE / "curve_raw_npy" / "Figure10d" / "8RX-ULA" / "raw.npz"
    if npz.is_file():
        z = np.load(npz)
        print(f"  CIR source: {npz}")
        return {
            "source": str(npz),
            "cir": z["cir"],
            "n_frames": z["n_frames"],
            "ports": z["ports"].astype(int),
            "first_path": z["first_path"],
            "rx_pream_count": z["rx_pream_count"],
            "calib": z["calib"],
        }

    raw = data_dir / "raw"
    if not raw.is_dir():
        raise FileNotFoundError(f"Need {npz} or {raw}/ tworef CIR CSVs")
    calib = parse_complex_list((raw / "calibration.csv").read_text())
    ports = list(range(1, 9))
    blocks = []
    max_f = 0
    for port in ports:
        p = raw / f"antenna_data_port{port}_8ports_sensing_car_square_tworef_0_3.csv"
        blk = _load_csv_port(p)
        blocks.append(blk)
        max_f = max(max_f, blk["n"])
    n_taps = blocks[0]["cir"].shape[1]
    cir = np.zeros((8, max_f, n_taps), dtype=np.complex64)
    fp = np.zeros((8, max_f), dtype=np.float64)
    rx = np.zeros_like(fp)
    n_frames = np.zeros(8, dtype=np.int32)
    for ip, blk in enumerate(blocks):
        n = blk["n"]
        n_frames[ip] = n
        cir[ip, :n] = blk["cir"]
        fp[ip, :n] = blk["first_path"]
        rx[ip, :n] = blk["rx_pream_count"]
    print(f"  CIR source: {raw}")
    return {
        "source": str(raw),
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports),
        "first_path": fp,
        "rx_pream_count": rx,
        "calib": calib,
    }


def run_batch(data_dir: Path | None = None, force: bool = False) -> dict:
    data_dir = Path(data_dir or HERE)
    needed = [data_dir / f"angle_amplitude_{tag}.csv" for tag, _ in CONFIGS]
    if not force and all(p.is_file() for p in needed):
        print("angle_amplitude CSVs already present; skip (set force=True to rerun)")
        return {"skipped": True}

    print("=== Figure10d CIR → RA slice (Python) ===")
    bundle = load_bundle(data_dir)
    n_min = int(np.min(bundle["n_frames"]))
    cir_l = [bundle["cir"][i, :n_min] for i in range(8)]
    fp_l = [bundle["first_path"][i, :n_min] for i in range(8)]
    rx_l = [bundle["rx_pream_count"][i, :n_min] for i in range(8)]
    print(f"  Load 8-port aligned cube, frames={n_min}")
    rx_aln = align_ports(cir_l, fp_l, rx_l, bundle["calib"])
    filtered = static_clutter_removal(rx_aln)
    del rx_aln
    theta, ra = theta_and_range_axes()
    valid = ~np.isnan(theta)
    theta_v = theta[valid]
    r_idx = int(np.argmin(np.abs(ra - TARGET_RANGE)))

    results = {}
    for tag, ports in CONFIGS:
        idx = [p - 1 for p in ports]
        amap = ra_one_window(filtered[idx, :, :], TARGET_WIN)
        amap = amap[valid, :]
        amap[:, ra < MIN_RANGE_ZERO] = 0
        gmax = np.nanmax(amap)
        sl = amap[:, r_idx] / gmax if gmax > 0 else amap[:, r_idx]
        T = pd.DataFrame(
            {
                "Frame": np.full(theta_v.size, TARGET_WIN),
                "Range_m": np.full(theta_v.size, ra[r_idx]),
                "Angle_deg": theta_v,
                "Amplitude_normalized": sl,
            }
        )
        out = data_dir / f"angle_amplitude_{tag}.csv"
        T.to_csv(out, index=False)
        im = int(np.argmax(sl))
        print(f"  {tag} r={ra[r_idx]:.3f} ymax={sl[im]:.3f} @ {theta_v[im]:.1f} deg -> {out.name}")
        results[tag] = {"range": float(ra[r_idx]), "ymax": float(sl[im]), "peak_deg": float(theta_v[im])}
    return results


if __name__ == "__main__":
    force = os.environ.get("FORCE_CIR_REPROCESS", "1") not in ("0", "false", "False")
    run_batch(force=force)
