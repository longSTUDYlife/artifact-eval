#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure10ab: CIR → LDE → seq-align → calibration → MVDR → error CSVs.

Pure Python port of test_all_angles_multi_config.m + test_single_angle_multi_config_v2.m
(aoa_method='mvdr', IQR/unify off).

Data source (first match):
  1) ../curve_raw_npy/Figure10ab/8RX-ULA/raw.npz
  2) raw/antenna_data_port*_....csv
"""

from __future__ import annotations

import os
from pathlib import Path

import numpy as np
import pandas as pd

from lde_extract_core import extract_lde_complex_from_cir

HERE = Path(__file__).resolve().parent
DONE = HERE.parent
ANGLES = [-40, -30, -20, -10, 0, 10, 20, 30, 40]

# Same compensations as test_all_angles_multi_config.m
PHASE_COMP_8 = np.deg2rad(np.array([0.00, 120.9, -109.4, 350.2, 441.6, 352.2, 196.1, 44.8]))
PHASE_COMP_4 = np.deg2rad(np.array([0.0, 350.2 + 109.4, 441.6 + 109.4, 352.2 + 109.4]))
PHASE_COMP_2 = np.deg2rad(np.array([0.0, 350.2 + 109.4]))

CONFIGS = [
    ("8port", list(range(1, 9)), PHASE_COMP_8),
    ("4port", [3, 4, 5, 6], PHASE_COMP_4),
    ("2port", [3, 4], PHASE_COMP_2),
]


def _wrap(p):
    return np.angle(np.exp(1j * np.asarray(p, dtype=float)))


def apply_calibration(spatial_phase_raw, phase_compensation):
    out = np.full_like(spatial_phase_raw, np.nan, dtype=float)
    for port in range(spatial_phase_raw.shape[1]):
        col = spatial_phase_raw[:, port]
        m = np.isfinite(col)
        out[m, port] = _wrap(col[m] - phase_compensation[port])
    return out


def compute_mvdr_aoa(spatial_phase, ports_to_use):
    """Match MATLAB compute_mvdr_aoa (sliding-window Capon)."""
    spatial_signal = np.exp(1j * _wrap(spatial_phase))
    nf, n = spatial_signal.shape
    ports_to_use = list(ports_to_use)

    if n == 4 and ports_to_use == [3, 4, 5, 6]:
        port_positions = np.array([0, 1, 2, 3], dtype=float)
    elif n == 2 and ports_to_use == [4, 5]:
        port_positions = np.array([0, 1], dtype=float)
    elif n == 8 and ports_to_use == list(range(1, 9)):
        port_positions = np.arange(8, dtype=float)
    else:
        port_positions = np.arange(n, dtype=float)

    fft_size = 512
    freq_axis = np.arange(-fft_size / 2, fft_size / 2) / fft_size
    sin_theta_fft = -2 * freq_axis
    valid_idx = np.abs(sin_theta_fft) <= 1
    theta_deg = np.degrees(np.arcsin(sin_theta_fft[valid_idx]))
    sin_theta = sin_theta_fft[valid_idx]

    steering = np.exp(-1j * np.pi * port_positions[None, :] * sin_theta[:, None])  # [A, N]

    valid_frames = [i for i in range(nf) if np.all(np.isfinite(spatial_signal[i]))]
    aoa = np.full(nf, np.nan)
    if not valid_frames:
        return aoa

    X = spatial_signal[valid_frames]  # [M, N]
    R = (X.conj().T @ X) / len(valid_frames)
    R = R + (0.01 * np.trace(R).real / n) * np.eye(n)

    window_size = min(20, max(5, int(round(nf / 4))))
    half = int(round(window_size / 2))

    for frame in range(nf):
        if not np.all(np.isfinite(spatial_signal[frame])):
            continue
        w0 = max(0, frame - half)
        w1 = min(nf, frame + half + 1)
        win = []
        for wf in range(w0, w1):
            if np.all(np.isfinite(spatial_signal[wf])):
                win.append(spatial_signal[wf])
        if len(win) < 2:
            R_frame = R
        else:
            W = np.asarray(win)
            R_frame = (W.conj().T @ W) / len(win)
            R_frame = R_frame + (0.01 * np.trace(R_frame).real / n) * np.eye(n)

        # Capon: 1 / (a^H R^{-1} a)
        try:
            Xsol = np.linalg.solve(R_frame, steering.T)  # [N, A]
        except np.linalg.LinAlgError:
            continue
        denom = np.real(np.sum(np.conj(steering) * Xsol.T, axis=1))
        spec = np.zeros_like(denom)
        good = denom > 0
        spec[good] = 1.0 / denom[good]
        # Negate to match array / true-angle convention (estimates otherwise ≈ −θ)
        aoa[frame] = -float(theta_deg[int(np.argmax(spec))])
    return aoa


def align_multiport(sequences, complex_small, complex_large, lde_small, lde_large):
    """
    Two-step pad + keep frames where all ports non-empty.
    sequences: list of 1d int arrays
    complex_*: list of 1d complex arrays
    Returns aligned arrays [Nf, Nports]
    """
    n_ports = len(sequences)
    min_start = min(int(s[0]) for s in sequences if len(s))
    max_frames = max(len(s) for s in sequences)
    seq_range = np.mod(min_start + np.arange(max_frames), 256)

    padded_small = [np.full(max_frames, np.nan + 1j * np.nan, dtype=np.complex128) for _ in range(n_ports)]
    padded_large = [np.full(max_frames, np.nan + 1j * np.nan, dtype=np.complex128) for _ in range(n_ports)]
    padded_ls = [np.full(max_frames, np.nan) for _ in range(n_ports)]
    padded_ll = [np.full(max_frames, np.nan) for _ in range(n_ports)]
    is_empty = [np.ones(max_frames, dtype=bool) for _ in range(n_ports)]

    for idx in range(n_ports):
        seqs = np.asarray(sequences[idx], dtype=int)
        orig_pos = 0
        for i, seq in enumerate(seq_range):
            if orig_pos < len(seqs) and seqs[orig_pos] == seq:
                padded_small[idx][i] = complex_small[idx][orig_pos]
                padded_large[idx][i] = complex_large[idx][orig_pos]
                padded_ls[idx][i] = lde_small[idx][orig_pos]
                padded_ll[idx][i] = lde_large[idx][orig_pos]
                is_empty[idx][i] = False
                orig_pos += 1
            else:
                while orig_pos < len(seqs):
                    seq_orig = int(seqs[orig_pos])
                    if seq_orig == seq:
                        break
                    if seq_orig >= seq:
                        forward_dist = seq_orig - seq
                    else:
                        forward_dist = seq_orig - seq + 256
                    if forward_dist >= 128:
                        orig_pos += 1
                    else:
                        break

    keep = []
    for i in range(max_frames):
        if all(not is_empty[p][i] for p in range(n_ports)):
            keep.append(i)
    if not keep:
        return None

    keep = np.asarray(keep)
    cs = np.column_stack([padded_small[p][keep] for p in range(n_ports)])
    cl = np.column_stack([padded_large[p][keep] for p in range(n_ports)])
    ls = np.column_stack([padded_ls[p][keep] for p in range(n_ports)])
    ll = np.column_stack([padded_ll[p][keep] for p in range(n_ports)])
    return cs, cl, ls, ll


def load_cir_bundle(data_dir: Path):
    """Load multiport CIR from raw.npz or per-angle CSVs."""
    npz = DONE / "curve_raw_npy" / "Figure10ab" / "8RX-ULA" / "raw.npz"
    if npz.is_file():
        z = np.load(npz)
        return {
            "source": str(npz),
            "cir": z["cir"],  # [port, angle, frame, tap]
            "n_frames": z["n_frames"],
            "ports": z["ports"].astype(int),
            "angles_deg": z["angles_deg"].astype(float),
            "sequence": z["sequence"],
        }

    raw = data_dir / "raw"
    if not raw.is_dir():
        raise FileNotFoundError(
            f"Need {npz} or {raw}/ antenna CSVs for CIR→AoA"
        )
    # Build same tensor from CSVs
    ports = list(range(1, 9))
    angles = ANGLES
    # probe shape
    sample = pd.read_csv(
        raw / f"antenna_data_port1_8ports_concurrent_localization_aoa_accuracy_0.csv",
        nrows=1,
    )
    real_cols = sorted(
        [c for c in sample.columns if c.startswith("CIR_real_")],
        key=lambda c: int(c.split("_")[-1]),
    )
    n_taps = len(real_cols)
    # count max frames
    max_f = 0
    for ang in angles:
        for port in ports:
            p = raw / f"antenna_data_port{port}_8ports_concurrent_localization_aoa_accuracy_{ang}.csv"
            max_f = max(max_f, sum(1 for _ in open(p)) - 1)
    cir = np.zeros((8, len(angles), max_f, n_taps), dtype=np.complex64)
    seq = np.full((8, len(angles), max_f), np.nan)
    n_frames = np.zeros((8, len(angles)), dtype=np.int32)
    for ia, ang in enumerate(angles):
        for ip, port in enumerate(ports):
            p = raw / f"antenna_data_port{port}_8ports_concurrent_localization_aoa_accuracy_{ang}.csv"
            df = pd.read_csv(p)
            imag_cols = sorted(
                [c for c in df.columns if c.startswith("CIR_imag_")],
                key=lambda c: int(c.split("_")[-1]),
            )
            n = len(df)
            n_frames[ip, ia] = n
            cir[ip, ia, :n] = (
                df[real_cols].to_numpy(np.float32)
                + 1j * df[imag_cols].to_numpy(np.float32)
            )
            if "Sequence" in df.columns:
                seq[ip, ia, :n] = df["Sequence"].to_numpy(float)
            else:
                seq[ip, ia, :n] = np.arange(n)
    return {
        "source": str(raw),
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports),
        "angles_deg": np.asarray(angles, dtype=float),
        "sequence": seq,
    }


def process_one_angle(bundle, angle_deg, ports_to_use, phase_comp, cache_dir: Path):
    ports = list(ports_to_use)
    angles = bundle["angles_deg"]
    ia = int(np.where(np.isclose(angles, float(angle_deg)))[0][0])
    port_idx = [int(np.where(bundle["ports"] == p)[0][0]) for p in ports]

    sequences, c_small, c_large, l_small, l_large = [], [], [], [], []
    for ip, port in zip(port_idx, ports):
        n = int(bundle["n_frames"][ip, ia])
        cir = bundle["cir"][ip, ia, :n]
        seq = bundle["sequence"][ip, ia, :n].astype(int)
        cache = cache_dir / f"lde_port{port}_ang{int(angle_deg)}.npz"
        if cache.is_file():
            z = np.load(cache)
            cs, cl = z["complex_small"], z["complex_large"]
            ls, ll = z["lde_small"], z["lde_large"]
        else:
            print(f"      LDE port{port} angle {angle_deg}° (N={n}) ...")
            # Match MATLAB export_lde: expectedGap = last number in filename
            # e.g. ..._accuracy_-40.csv → 40, ..._0.csv → 0
            gap = float(abs(int(angle_deg)))
            cs, cl, ls, ll = extract_lde_complex_from_cir(cir, expected_gap=gap)
            np.savez_compressed(
                cache,
                complex_small=cs,
                complex_large=cl,
                lde_small=ls,
                lde_large=ll,
            )
        sequences.append(seq)
        c_small.append(cs)
        c_large.append(cl)
        l_small.append(ls)
        l_large.append(ll)

    aligned = align_multiport(sequences, c_small, c_large, l_small, l_large)
    if aligned is None:
        return None
    cs, cl, ls, ll = aligned

    # Quality: require two LDEs and separation > 30
    two = np.isfinite(cs).all(axis=1) & np.isfinite(cl).all(axis=1)
    sep = np.abs(ll - ls)
    good = two & np.all(sep > 30, axis=1)
    if not np.any(good):
        return None
    cs, cl = cs[good], cl[good]

    phase_small = np.angle(cs)
    phase_large = np.angle(cl)
    phase_diff = _wrap(phase_small - phase_large)
    spatial = _wrap(phase_diff - phase_diff[:, [0]])
    spatial_cal = apply_calibration(spatial, phase_comp)
    aoa = compute_mvdr_aoa(spatial_cal, ports)
    return aoa


def run_batch(data_dir: Path | None = None, force: bool = False) -> dict:
    data_dir = Path(data_dir or HERE)
    out_needed = [
        data_dir / f"frame_errors_{tag}_filtered.csv" for tag, _, _ in CONFIGS
    ] + [data_dir / f"angle_errors_{tag}_filtered.csv" for tag, _, _ in CONFIGS]
    if not force and all(p.is_file() for p in out_needed):
        print("ULA error CSVs already present; skip CIR reprocess (set force=True to rerun)")
        return {"skipped": True}

    print("=== Figure10ab CIR → MVDR (Python) ===")
    bundle = load_cir_bundle(data_dir)
    print(f"  CIR source: {bundle['source']}")
    cache_dir = data_dir / "lde_cache_py"
    cache_dir.mkdir(exist_ok=True)

    results = {}
    for tag, ports, comp in CONFIGS:
        print(f"  Config {tag} ports={ports}")
        frame_errs = []
        angle_rows = []
        for ang in ANGLES:
            print(f"    angle {ang}°")
            aoa = process_one_angle(bundle, ang, ports, comp, cache_dir)
            if aoa is None or not np.any(np.isfinite(aoa)):
                print(f"      WARNING: no valid frames")
                continue
            err = np.abs(aoa[np.isfinite(aoa)] - ang)
            frame_errs.append(err)
            angle_rows.append(
                {
                    "True_Angle": ang,
                    "Mean_Error": float(np.mean(err)),
                    "Std_Error": float(np.std(err, ddof=1)) if err.size > 1 else 0.0,
                    "Estimated_Angle_Mean": float(np.nanmean(aoa)),
                    "Estimated_Angle_Std": float(np.nanstd(aoa, ddof=1))
                    if np.sum(np.isfinite(aoa)) > 1
                    else 0.0,
                    "N_Frames": int(err.size),
                }
            )
            print(f"      N={err.size} median_abs_err={np.median(err):.2f}°")
        all_err = np.concatenate(frame_errs) if frame_errs else np.array([])
        pd.DataFrame({"Absolute_Error": all_err}).to_csv(
            data_dir / f"frame_errors_{tag}_filtered.csv", index=False
        )
        pd.DataFrame(angle_rows).to_csv(
            data_dir / f"angle_errors_{tag}_filtered.csv", index=False
        )
        results[tag] = {"n": int(all_err.size), "median": float(np.median(all_err)) if all_err.size else None}
        print(f"  Wrote frame/angle_errors_{tag}_filtered.csv  N={all_err.size}")
    return results


if __name__ == "__main__":
    force = os.environ.get("FORCE_CIR_REPROCESS", "1") not in ("0", "false", "False")
    run_batch(force=force)
