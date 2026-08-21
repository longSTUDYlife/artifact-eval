#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure10c: CIR → seq-sync (firstPathAmp2) → align → clutter → angle-FFT RA → AoA CSV.

Pure Python port of batch_extract_aoa.m (202603122 env1).
Data source (first match):
  1) ../curve_raw_npy/Figure10c/8RX-ULA/raw.npz
  2) raw/antenna_data_port*_8ports_sensing_env1_{angle}_{times}.csv
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
    compute_ra_maps_stream,
    extract_aoa_from_angle_maps,
    parse_complex_list,
    static_clutter_removal,
    sync_by_id,
)

ANGLES = [-40, -30, -20, -10, 0, 10, 20, 30, 40]
TRIALS = [1, 2, 3]
MIN_RANGE_M = 1.8
CONFIGS = [
    ("8port", list(range(1, 9))),
    ("4port", [3, 4, 5, 6]),
    ("2port", [3, 4]),
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
    out = {
        "cir": cir.astype(np.complex64),
        "first_path": df["firstPath"].to_numpy(np.float64) if "firstPath" in df.columns else np.zeros(len(df)),
        "rx_pream_count": (
            df["rxPreamCount"].to_numpy(np.float64) if "rxPreamCount" in df.columns else np.ones(len(df))
        ),
        "first_path_amp2": (
            df["firstPathAmp2"].to_numpy(np.float64) if "firstPathAmp2" in df.columns else np.arange(len(df))
        ),
        "packet_type": (
            df["PacketType"].to_numpy(np.int32) if "PacketType" in df.columns else np.ones(len(df), dtype=np.int32)
        ),
        "n": len(df),
    }
    return out


def load_bundle(data_dir: Path) -> dict:
    npz = DONE / "curve_raw_npy" / "Figure10c" / "8RX-ULA" / "raw.npz"
    if npz.is_file():
        z = np.load(npz)
        calib = z["calib"]
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
            "calib": calib,
        }

    raw = data_dir / "raw"
    if not raw.is_dir():
        raise FileNotFoundError(f"Need {npz} or {raw}/ env1 CIR CSVs")
    calib_path = raw / "spatial_phase_avg_complex_v3_angle0.csv"
    calib = parse_complex_list(calib_path.read_text())
    ports = list(range(1, 9))
    max_f = 0
    blocks = {}
    for ang in ANGLES:
        for trial in TRIALS:
            for port in ports:
                p = raw / f"antenna_data_port{port}_8ports_sensing_env1_{ang}_{trial}.csv"
                blk = _load_csv_port(p)
                blocks[(port, ang, trial)] = blk
                max_f = max(max_f, blk["n"])
    n_taps = blocks[(1, 0, 1)]["cir"].shape[1]
    cir = np.zeros((8, len(ANGLES), len(TRIALS), max_f, n_taps), dtype=np.complex64)
    fp = np.zeros((8, len(ANGLES), len(TRIALS), max_f), dtype=np.float64)
    rx = np.zeros_like(fp)
    amp2 = np.zeros_like(fp)
    ptype = np.zeros((8, len(ANGLES), len(TRIALS), max_f), dtype=np.int32)
    n_frames = np.zeros((8, len(ANGLES), len(TRIALS)), dtype=np.int32)
    for ip, port in enumerate(ports):
        for ia, ang in enumerate(ANGLES):
            for it, trial in enumerate(TRIALS):
                blk = blocks[(port, ang, trial)]
                n = blk["n"]
                n_frames[ip, ia, it] = n
                cir[ip, ia, it, :n] = blk["cir"]
                fp[ip, ia, it, :n] = blk["first_path"]
                rx[ip, ia, it, :n] = blk["rx_pream_count"]
                amp2[ip, ia, it, :n] = blk["first_path_amp2"]
                ptype[ip, ia, it, :n] = blk["packet_type"]
    print(f"  CIR source: {raw}")
    return {
        "source": str(raw),
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports),
        "angles_deg": np.asarray(ANGLES, dtype=float),
        "trials": np.asarray(TRIALS, dtype=int),
        "first_path": fp,
        "rx_pream_count": rx,
        "first_path_amp2": amp2,
        "packet_type": ptype,
        "calib": calib,
    }


def process_one(bundle, ports, ia, it) -> pd.DataFrame | None:
    port_idx = [int(np.where(bundle["ports"] == p)[0][0]) for p in ports]
    cir_l, fp_l, rx_l, amp2_l, ptype_l = [], [], [], [], []
    for ip in port_idx:
        n = int(bundle["n_frames"][ip, ia, it])
        if n <= 0:
            return None
        cir_l.append(bundle["cir"][ip, ia, it, :n])
        fp_l.append(bundle["first_path"][ip, ia, it, :n])
        rx_l.append(bundle["rx_pream_count"][ip, ia, it, :n])
        amp2_l.append(bundle["first_path_amp2"][ip, ia, it, :n])
        ptype_l.append(bundle["packet_type"][ip, ia, it, :n])

    rows, common = sync_by_id(amp2_l, ptype_l)
    if common.size == 0:
        print("      no common sync ids")
        return None
    print(f"      synced {common.size} frames (seq [{int(common.min())}, {int(common.max())}])")
    cir_s = [c[r] for c, r in zip(cir_l, rows)]
    fp_s = [c[r] for c, r in zip(fp_l, rows)]
    rx_s = [c[r] for c, r in zip(rx_l, rows)]
    calib = bundle["calib"][np.asarray(ports) - 1]
    rx_aln = align_ports(cir_s, fp_s, rx_s, calib)
    if rx_aln.shape[2] < 83:
        print(f"      skip frames={rx_aln.shape[2]}")
        return None
    filtered = static_clutter_removal(rx_aln)
    del rx_aln
    maps, ra, theta = compute_ra_maps_stream(filtered)
    del filtered
    aoa, rng, eng = extract_aoa_from_angle_maps(maps, ra, theta, MIN_RANGE_M)
    ang = float(bundle["angles_deg"][ia])
    trial = int(bundle["trials"][it])
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


def run_batch(data_dir: Path | None = None, force: bool = False, angles=None) -> dict:
    data_dir = Path(data_dir or HERE)
    out_dir = data_dir / "aoa_estimates"
    out_dir.mkdir(exist_ok=True)
    angles = list(ANGLES if angles is None else angles)
    needed = [out_dir / f"aoa_estimates_{tag}_{ang}.csv" for tag, _ in CONFIGS for ang in angles]
    if not force and all(p.is_file() for p in needed):
        print("aoa_estimates already present; skip (set force=True to rerun)")
        return {"skipped": True}

    print("=== Figure10c CIR → RA AoA (Python) ===")
    bundle = load_bundle(data_dir)
    results = {}
    for tag, ports in CONFIGS:
        print(f"\n======== {tag} ports {ports} ========")
        for ang in angles:
            ia = int(np.where(np.isclose(bundle["angles_deg"], float(ang)))[0][0])
            parts = []
            for it, trial in enumerate(bundle["trials"]):
                print(f"  aoa={ang:+d} times={int(trial)}")
                df = process_one(bundle, ports, ia, it)
                if df is not None:
                    mae = float(np.nanmean(np.abs(df["estimated_aoa"] - df["aoa"])))
                    print(f"    {len(df)} win  MAE={mae:.2f}")
                    parts.append(df)
            if not parts:
                continue
            out = pd.concat(parts, ignore_index=True)
            path = out_dir / f"aoa_estimates_{tag}_{ang}.csv"
            out.to_csv(path, index=False)
            mae = float(np.nanmean(np.abs(out["estimated_aoa"] - out["aoa"])))
            print(f"  -> {path.name}  N={len(out)}  MAE={mae:.2f}")
            results[f"{tag}_{ang}"] = {"n": len(out), "mae": mae}
    return results


if __name__ == "__main__":
    force = os.environ.get("FORCE_CIR_REPROCESS", "1") not in ("0", "false", "False")
    run_batch(force=force)
