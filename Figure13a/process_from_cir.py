#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 13(a): CIR → LDE → seq-align → MVDR → distance correction → errors + scatter.

Pure Python port of run_figure13a.m + test_single_angle_distance_multi_config_v3.m
(aoa_method='mvdr', unify off, RMSE filter 2 m).

Data (first match):
  1) ../curve_raw_npy/Figure13a/8RX-ULA/raw.npz
  2) raw/antenna_data_port*_8ports_concurrent_localization_accuracy_{angle}_{dist}.csv

LDE: lde_cache/*.csv (MATLAB export) if present, else extract_lde_complex_from_cir.
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pandas as pd

from lde_extract_core import approx_mag, extract_lde_complex_from_cir, lde_fullscan

HERE = Path(__file__).resolve().parent
DONE = HERE.parent
ANGLES = [-40, -30, -20, -10, 0, 10, 20, 30, 40]
DISTS = [1, 2, 3, 4]
PORTS = list(range(1, 9))
RMSE_THRESHOLD_M = 2.0
N_SCATTER_PER_CELL = 50

PHASE_GT1M = np.deg2rad(np.array([0.00, 471.1, 239.1, 308.3, 208.6, 124.3, 409.2, 660.7]))
PHASE_1M = np.deg2rad(np.array([0.00, -269.7, -175.6, -105.0, -209.4, 51.8, -10.1, -116.2]))

LDE_P = {
    "thFactor": 6,
    "noiseFrac": 0.15,
    "madK": 4.5,
    "quantize64": True,
    "gradWin": 3,
    "ampWin": 14,
    "mergeSeps": 12,
    "schmittHigh": 1.20,
    "schmittLookAhead": 10,
    "minStayBins": 3,
    "minSlope": 8,
    "ignoreRange": [1, 2],
    "thAdd": 300,
}


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
    """Match Figure11ab MATLAB compute_mvdr_aoa (no sign flip)."""
    spatial_signal = np.exp(1j * _wrap(spatial_phase))
    nf, n = spatial_signal.shape
    ports_to_use = list(ports_to_use)
    if n == 8 and ports_to_use == list(range(1, 9)):
        port_positions = np.arange(8, dtype=float)
    else:
        port_positions = np.arange(n, dtype=float)

    fft_size = 512
    freq_axis = np.arange(-fft_size / 2, fft_size / 2) / fft_size
    sin_theta_fft = -2 * freq_axis
    valid_idx = np.abs(sin_theta_fft) <= 1
    theta_deg = np.degrees(np.arcsin(sin_theta_fft[valid_idx]))
    sin_theta = sin_theta_fft[valid_idx]
    steering = np.exp(-1j * np.pi * port_positions[None, :] * sin_theta[:, None])

    valid_frames = [i for i in range(nf) if np.all(np.isfinite(spatial_signal[i]))]
    aoa = np.full(nf, np.nan)
    if not valid_frames:
        return aoa

    X = spatial_signal[valid_frames]
    R = (X.conj().T @ X) / len(valid_frames)
    R = R + (0.01 * np.trace(R).real / n) * np.eye(n)

    window_size = min(20, max(5, int(round(nf / 4))))
    half = int(round(window_size / 2))

    for frame in range(nf):
        if not np.all(np.isfinite(spatial_signal[frame])):
            continue
        w0 = max(0, frame - half)
        w1 = min(nf, frame + half + 1)
        win = [spatial_signal[wf] for wf in range(w0, w1) if np.all(np.isfinite(spatial_signal[wf]))]
        if len(win) < 2:
            R_frame = R
        else:
            W = np.asarray(win)
            R_frame = (W.conj().T @ W) / len(win)
            R_frame = R_frame + (0.01 * np.trace(R_frame).real / n) * np.eye(n)
        try:
            xsol = np.linalg.solve(R_frame, steering.T)
        except np.linalg.LinAlgError:
            continue
        denom = np.real(np.sum(np.conj(steering) * xsol.T, axis=1))
        spec = np.zeros_like(denom)
        good = denom > 0
        spec[good] = 1.0 / denom[good]
        # Match MATLAB signed AoA (Python FFT axis is opposite without this)
        aoa[frame] = -float(theta_deg[int(np.argmax(spec))])
    return aoa


def align_multiport(sequences, complex_small, complex_large, distances):
    n_ports = len(sequences)
    min_start = min(int(s[0]) for s in sequences if len(s))
    max_frames = max(len(s) for s in sequences)
    seq_range = np.mod(min_start + np.arange(max_frames), 256)

    padded_small = [np.full(max_frames, np.nan + 1j * np.nan, dtype=np.complex128) for _ in range(n_ports)]
    padded_large = [np.full(max_frames, np.nan + 1j * np.nan, dtype=np.complex128) for _ in range(n_ports)]
    padded_dist = [np.full(max_frames, np.nan) for _ in range(n_ports)]
    is_empty = [np.ones(max_frames, dtype=bool) for _ in range(n_ports)]

    for idx in range(n_ports):
        seqs = np.asarray(sequences[idx], dtype=int)
        orig_pos = 0
        for i, seq in enumerate(seq_range):
            if orig_pos < len(seqs) and seqs[orig_pos] == seq:
                padded_small[idx][i] = complex_small[idx][orig_pos]
                padded_large[idx][i] = complex_large[idx][orig_pos]
                padded_dist[idx][i] = distances[idx][orig_pos]
                is_empty[idx][i] = False
                orig_pos += 1
            else:
                while orig_pos < len(seqs):
                    seq_orig = int(seqs[orig_pos])
                    if seq_orig == seq:
                        break
                    forward_dist = seq_orig - seq if seq_orig >= seq else seq_orig - seq + 256
                    if forward_dist >= 128:
                        orig_pos += 1
                    else:
                        break

    keep = [i for i in range(max_frames) if all(not is_empty[p][i] for p in range(n_ports))]
    if not keep:
        return None
    keep = np.asarray(keep)
    cs = np.column_stack([padded_small[p][keep] for p in range(n_ports)])
    cl = np.column_stack([padded_large[p][keep] for p in range(n_ports)])
    dist = np.column_stack([padded_dist[p][keep] for p in range(n_ports)])
    return cs, cl, dist


def quality_mask(cir_rows, complex_large):
    """Two LDEs (complex_large finite) + CIR peak separation > 30 bins."""
    n = cir_rows.shape[0]
    ok = np.zeros(n, dtype=bool)
    for k in range(n):
        if not np.isfinite(complex_large[k]):
            continue
        mag = approx_mag(cir_rows[k])
        cand_idx, cand_amp, _thr = lde_fullscan(mag, LDE_P)
        if cand_idx is None or len(cand_idx) < 2:
            continue
        srt = np.argsort(cand_amp)[::-1][:2]
        ldes = np.sort(cand_idx[srt] + 0.5)
        if (ldes[1] - ldes[0]) > 30:
            ok[k] = True
    return ok


def correct_distance(measured, angle_deg, coeff):
    corrected = float(measured)
    for _ in range(10):
        err = (
            coeff["const"]
            + coeff["angle"] * angle_deg
            + coeff["dist"] * corrected
            + coeff["angle2"] * angle_deg ** 2
            + coeff["dist2"] * corrected ** 2
            + coeff["angle_dist"] * angle_deg * corrected
        )
        new = measured - err
        if abs(new - corrected) < 1e-6:
            return new
        corrected = new
    return corrected


def load_coeff(data_dir: Path) -> dict:
    t = pd.read_csv(data_dir / "distance_error_correction_coefficients.csv")
    return {k: float(t[k].iloc[0]) for k in ("const", "angle", "dist", "angle2", "dist2", "angle_dist")}


def load_bundle(data_dir: Path, angles=None, dists=None) -> dict:
    angles = list(angles if angles is not None else ANGLES)
    dists = list(dists if dists is not None else DISTS)
    npz = DONE / "curve_raw_npy" / "Figure13a" / "8RX-ULA" / "raw.npz"
    if npz.is_file():
        try:
            z = np.load(npz)
            _ = z["n_frames"]
        except Exception:
            z = None
        if z is not None:
            print(f"  CIR source: {npz}")
            ia = [int(np.where(np.isclose(z["angles_deg"].astype(float), float(a)))[0][0]) for a in angles]
            idd = [int(np.where(np.isclose(z["dists_m"].astype(float), float(d)))[0][0]) for d in dists]
            return {
                "source": str(npz),
                "cir": z["cir"][:, ia][:, :, idd],
                "n_frames": z["n_frames"][:, ia][:, :, idd],
                "sequence": z["sequence"][:, ia][:, :, idd],
                "packet_type": z["packet_type"][:, ia][:, :, idd],
                "first_path_amp1": z["first_path_amp1"][:, ia][:, :, idd],
                "ports": z["ports"].astype(int),
                "angles_deg": np.asarray(angles, dtype=float),
                "dists_m": np.asarray(dists, dtype=float),
            }

    raw = data_dir / "raw"
    if not raw.is_dir():
        raise FileNotFoundError(f"Need {npz} or {raw}/ antenna CSVs")
    print(f"  CIR source: {raw}")
    sample = pd.read_csv(
        raw / f"antenna_data_port1_8ports_concurrent_localization_accuracy_{angles[0]}_{dists[0]}.csv",
        nrows=1,
    )
    real_cols = sorted(
        [c for c in sample.columns if c.startswith("CIR_real_")],
        key=lambda c: int(c.split("_")[-1]),
    )
    n_taps = len(real_cols)
    max_f = 0
    for port in PORTS:
        for ang in angles:
            for dist in dists:
                p = raw / f"antenna_data_port{port}_8ports_concurrent_localization_accuracy_{ang}_{dist}.csv"
                max_f = max(max_f, sum(1 for _ in open(p)) - 1)
    cir = np.zeros((8, len(angles), len(dists), max_f, n_taps), dtype=np.complex64)
    seq = np.zeros((8, len(angles), len(dists), max_f), dtype=np.float64)
    ptype = np.zeros_like(seq, dtype=np.int32)
    amp1 = np.zeros_like(seq)
    n_frames = np.zeros((8, len(angles), len(dists)), dtype=np.int32)
    for ip, port in enumerate(PORTS):
        for ia, ang in enumerate(angles):
            for idd, dist in enumerate(dists):
                path = raw / f"antenna_data_port{port}_8ports_concurrent_localization_accuracy_{ang}_{dist}.csv"
                df = pd.read_csv(path)
                imag_cols = sorted(
                    [c for c in df.columns if c.startswith("CIR_imag_")],
                    key=lambda c: int(c.split("_")[-1]),
                )
                n = len(df)
                n_frames[ip, ia, idd] = n
                cir[ip, ia, idd, :n] = (
                    df[real_cols].to_numpy(np.float32) + 1j * df[imag_cols].to_numpy(np.float32)
                )
                seq[ip, ia, idd, :n] = df["Sequence"].to_numpy(float)
                ptype[ip, ia, idd, :n] = df["PacketType"].to_numpy(np.int32)
                amp1[ip, ia, idd, :n] = df["firstPathAmp1"].to_numpy(float)
    return {
        "source": str(raw),
        "cir": cir,
        "n_frames": n_frames,
        "sequence": seq,
        "packet_type": ptype,
        "first_path_amp1": amp1,
        "ports": np.asarray(PORTS),
        "angles_deg": np.asarray(angles, dtype=float),
        "dists_m": np.asarray(dists, dtype=float),
    }


def load_or_extract_lde(data_dir: Path, cir, port, ang, dist, lde_dir: Path | None = None):
    cache_dir = Path(lde_dir) if lde_dir is not None else data_dir / "lde_cache"
    cache = cache_dir / f"lde_complex_real_port{port}_angle{ang}_dist{dist}.csv"
    if cache.is_file():
        df = pd.read_csv(cache)
        cs = df["complex_small_real"].to_numpy(float) + 1j * df["complex_small_imag"].to_numpy(float)
        cl = df["complex_large_real"].to_numpy(float) + 1j * df["complex_large_imag"].to_numpy(float)
        return cs, cl
    print(f"      LDE extract port{port} {ang}°/{dist}m N={cir.shape[0]}")
    cs, cl, _ls, _ll = extract_lde_complex_from_cir(cir, expected_gap=float(dist))
    cache_dir.mkdir(parents=True, exist_ok=True)
    pd.DataFrame(
        {
            "complex_small_real": np.real(cs),
            "complex_small_imag": np.imag(cs),
            "complex_large_real": np.real(cl),
            "complex_large_imag": np.imag(cl),
        }
    ).to_csv(cache, index=False)
    return cs, cl


def _cell_port_views(bundle, ang, dist):
    """Per-port slices for one (angle, dist). Env-1 is [port, ang, dist, ...]; extras are [port, cell, ...]."""
    if bundle.get("layout") == "cells":
        hits = np.flatnonzero(
            np.isclose(bundle["angles_deg"].astype(float), float(ang))
            & np.isclose(bundle["dists_m"].astype(float), float(dist))
        )
        if hits.size != 1:
            raise KeyError(f"cell {ang}°/{dist}m not in bundle ({hits.size} matches)")
        ic = int(hits[0])
        views = []
        for ip in range(len(bundle["ports"])):
            n = int(bundle["n_frames"][ip, ic])
            views.append(
                {
                    "cir": bundle["cir"][ip, ic, :n],
                    "sequence": bundle["sequence"][ip, ic, :n],
                    "packet_type": bundle["packet_type"][ip, ic, :n],
                    "first_path_amp1": bundle["first_path_amp1"][ip, ic, :n],
                }
            )
        return views
    ia = int(np.where(np.isclose(bundle["angles_deg"], float(ang)))[0][0])
    idd = int(np.where(np.isclose(bundle["dists_m"], float(dist)))[0][0])
    views = []
    for ip in range(len(bundle["ports"])):
        n = int(bundle["n_frames"][ip, ia, idd])
        views.append(
            {
                "cir": bundle["cir"][ip, ia, idd, :n],
                "sequence": bundle["sequence"][ip, ia, idd, :n],
                "packet_type": bundle["packet_type"][ip, ia, idd, :n],
                "first_path_amp1": bundle["first_path_amp1"][ip, ia, idd, :n],
            }
        )
    return views


def process_one_cell(
    bundle,
    data_dir,
    ang,
    dist,
    coeff,
    phase_comp=None,
    apply_correction=True,
    lde_dir: Path | None = None,
    rmse_threshold: float = RMSE_THRESHOLD_M,
):
    if phase_comp is None:
        phase_comp = PHASE_1M if int(dist) == 1 else PHASE_GT1M

    sequences, c_small, c_large, distances, cirs = [], [], [], [], []
    views = _cell_port_views(bundle, ang, dist)
    for ip, port in enumerate(PORTS):
        view = views[ip]
        keep = view["packet_type"] != 1
        cir = view["cir"][keep]
        seq = view["sequence"][keep].astype(int)
        amp1 = view["first_path_amp1"][keep]
        cs, cl = load_or_extract_lde(data_dir, cir, port, ang, dist, lde_dir=lde_dir)
        if cs.shape[0] != cir.shape[0]:
            raise RuntimeError(
                f"LDE rows {cs.shape[0]} != filtered CIR {cir.shape[0]} "
                f"(port {port} {ang}° {dist}m)"
            )
        q = quality_mask(cir, cl)
        sequences.append(seq[q])
        c_small.append(cs[q])
        c_large.append(cl[q])
        distances.append(amp1[q])
        cirs.append(cir[q])

    aligned = align_multiport(sequences, c_small, c_large, distances)
    if aligned is None:
        return None
    cs, cl, dist_cm = aligned
    n_common = cs.shape[0]

    phase_small = np.angle(cs)
    phase_large = np.angle(cl)
    phase_diff = phase_small - phase_large  # MATLAB: no wrap here
    spatial_raw = phase_diff - phase_diff[:, [0]]
    spatial_cal = apply_calibration(spatial_raw, phase_comp)
    aoa_cal = compute_mvdr_aoa(spatial_cal, PORTS)

    dist_raw = np.full(n_common, np.nan)
    for f in range(n_common):
        for p in range(8):
            v = dist_cm[f, p]
            if np.isfinite(v) and v != 0:
                dist_raw[f] = v / 100.0
                break
    dist_m = dist_raw.copy()
    if apply_correction and coeff is not None:
        for f in range(n_common):
            if not np.isfinite(dist_m[f]) or not np.isfinite(aoa_cal[f]):
                continue
            dist_m[f] = correct_distance(dist_m[f], aoa_cal[f], coeff)

    true_d = float(dist)
    true_a = float(ang)
    true_x = true_d * np.cos(np.deg2rad(true_a))
    true_y = true_d * np.sin(np.deg2rad(true_a))
    est_x = np.full(n_common, np.nan)
    est_y = np.full(n_common, np.nan)
    for f in range(n_common):
        if not np.isfinite(aoa_cal[f]):
            continue
        d = dist_m[f] if np.isfinite(dist_m[f]) else true_d
        rad = np.deg2rad(aoa_cal[f])
        est_x[f] = d * np.cos(rad)
        est_y[f] = d * np.sin(rad)
    err = np.sqrt((est_x - true_x) ** 2 + (est_y - true_y) ** 2)
    keep = np.isfinite(est_x) & np.isfinite(est_y) & (err <= rmse_threshold)
    est_x[~keep] = np.nan
    est_y[~keep] = np.nan
    err[~keep] = np.nan
    return {
        "angle": true_a,
        "distance": true_d,
        "estimated_x_cal": est_x,
        "estimated_y_cal": est_y,
        "true_x": true_x,
        "true_y": true_y,
        "errors": err,
        "aoa_cal": aoa_cal,
        "measured_dist": dist_raw,
    }


def scatter_from_results(results):
    true_a, true_d, est_a, est_d = [], [], [], []
    for r in results:
        true_a.append(r["angle"])
        true_d.append(r["distance"])
        ex, ey = r["estimated_x_cal"], r["estimated_y_cal"]
        m = np.isfinite(ex) & np.isfinite(ey)
        if not np.any(m):
            continue
        err = np.sqrt((ex[m] - r["true_x"]) ** 2 + (ey[m] - r["true_y"]) ** 2)
        order = np.argsort(err)
        n_sel = min(N_SCATTER_PER_CELL, order.size)
        sx, sy = ex[m][order[:n_sel]], ey[m][order[:n_sel]]
        est_a.extend(np.degrees(np.arctan2(sy, sx)).tolist())
        est_d.extend(np.sqrt(sx ** 2 + sy ** 2).tolist())
    angles = np.asarray(true_a + est_a, dtype=float)
    dists = np.asarray(true_d + est_d, dtype=float)
    types = np.asarray([1] * len(true_a) + [0] * len(est_a), dtype=int)
    return pd.DataFrame({"angle": angles, "distance": dists, "type": types})


def run_batch(data_dir: Path | None = None, force: bool = False, smoke: bool = False) -> dict:
    data_dir = Path(data_dir or HERE)
    out_err = data_dir / "localization_errors_8port.csv"
    out_sc = data_dir / "localization_scatter_data.csv"
    if not force and out_err.is_file() and out_sc.is_file() and not smoke:
        print("Error/scatter CSVs present; skip CIR reprocess (force=True to rerun)")
        return {"skipped": True}

    print("=== Figure13a CIR → MVDR localization (Python) ===")
    angles = [0] if smoke else ANGLES
    dists = [2] if smoke else DISTS
    if smoke:
        print("SMOKE: 0° / 2 m only")
    bundle = load_bundle(data_dir, angles=angles, dists=dists)
    coeff = load_coeff(data_dir)

    results = []
    all_err = []
    for ang in angles:
        for dist in dists:
            print(f"  {ang}° / {dist} m")
            r = process_one_cell(bundle, data_dir, ang, dist, coeff)
            if r is None:
                print("    SKIP")
                continue
            e = r["errors"]
            e = e[np.isfinite(e)]
            all_err.append(e)
            results.append(r)
            print(
                f"    N={e.size} mean|err|={float(np.mean(e)):.3f} m  "
                f"AoA={float(np.nanmean(r['aoa_cal'])):.1f}°"
            )

    err = np.concatenate(all_err) if all_err else np.array([])
    n = int(err.size)
    med = float(np.median(err)) if n else float("nan")
    p90 = float(np.percentile(err, 90)) if n else float("nan")
    rmse = float(np.sqrt(np.mean(err ** 2))) if n else float("nan")
    print(f"\nN = {n}, median = {med:.3f} m, 90th = {p90:.3f} m, RMSE = {rmse:.3f} m")
    pd.DataFrame({"Localization_Error": err}).to_csv(out_err, index=False)
    scatter_from_results(results).to_csv(out_sc, index=False)
    print(f"Saved {out_err}")
    print(f"Saved {out_sc}")
    return {"n": n, "median": med, "p90": p90, "rmse": rmse}


if __name__ == "__main__":
    import os

    smoke = os.environ.get("SMOKE", "").lower() in ("1", "true", "yes")
    run_batch(HERE, force=True, smoke=smoke)
