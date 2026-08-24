#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Shared CIR → aligned window → clutter → angle-FFT RA (MATLAB Range_doppler port)."""

from __future__ import annotations

import numpy as np
from scipy.signal import resample_poly

C = 3e8
FC = 3494.4e6
LAMBDA = C / FC
FS = 64e9
UPSAMPLE = 64
NUM_CIR_POINTS = 100
N_ANGLE = 64
ORIGIN_START_IDX = 699
WIN_RADIUS = 83
RD_WINDOW = 83
WINDOW_LEFT = 10 * UPSAMPLE
WINDOW_RIGHT = 40 * UPSAMPLE
WINDOW_LEN = WINDOW_LEFT + WINDOW_RIGHT + 1
D = LAMBDA / 2


def parse_complex_list(text: str) -> np.ndarray:
    vals = []
    for line in text.splitlines():
        s = line.strip().replace(" ", "")
        if not s:
            continue
        s = s.replace("i", "j")
        if s.endswith("+0.0j") or s.endswith("+0j"):
            pass
        vals.append(complex(s))
    return np.asarray(vals, dtype=np.complex128)


def matlab_resample(cir_raw: np.ndarray, up: int = UPSAMPLE) -> np.ndarray:
    """Match MATLAB resample(x, p, 1) along tap axis 0. cir_raw: [taps, frames]."""
    x = np.asarray(cir_raw)
    if np.iscomplexobj(x):
        re = resample_poly(np.real(x).astype(np.float64), up, 1, axis=0)
        im = resample_poly(np.imag(x).astype(np.float64), up, 1, axis=0)
        return re + 1j * im
    return resample_poly(x.astype(np.float64), up, 1, axis=0)


def align_one_port(
    cir: np.ndarray,
    first_path: np.ndarray,
    rx_pream: np.ndarray,
    calib: complex,
    upsample_factor: int = UPSAMPLE,
    origin_start_idx: int = ORIGIN_START_IDX,
    window_left: int = WINDOW_LEFT,
    window_right: int = WINDOW_RIGHT,
) -> np.ndarray:
    """Return [window_len, frames] complex, MATLAB load_and_align_window one port."""
    cir = np.asarray(cir)
    n_frames = cir.shape[0]
    pream = np.maximum(np.asarray(rx_pream, dtype=np.float64), 1.0)
    cir_raw = (cir.astype(np.complex128) / pream[:, None]).T  # [taps, F]
    cir_up = matlab_resample(cir_raw, upsample_factor)
    lde = np.asarray(first_path, dtype=np.float64).copy()
    invalid = np.where((lde > 758) | (lde < 700))[0]
    for idx in invalid:
        src = 1 if idx == 0 else idx - 1
        if src < n_frames:
            cir_up[:, idx] = cir_up[:, src]
            lde[idx] = lde[src]

    # MATLAB 1-based row index → Python 0-based
    lde_idx = np.rint((lde - origin_start_idx) * upsample_factor).astype(np.int64) - 1
    window_len = window_left + window_right + 1
    L = cir_up.shape[0]
    offsets = np.arange(-window_left, window_right + 1)
    win_idx = lde_idx[None, :] + offsets[:, None]
    win_idx = np.clip(win_idx, 0, L - 1)
    cols = np.arange(n_frames)[None, :]
    extract = cir_up[win_idx, cols]
    phis = np.angle(extract[window_left, :])
    extract = extract * np.exp(-1j * phis) * complex(calib)
    return extract


def align_ports(
    cir_list: list[np.ndarray],
    first_path_list: list[np.ndarray],
    rx_pream_list: list[np.ndarray],
    calib: np.ndarray,
) -> np.ndarray:
    """Stack aligned ports → [M, window_len, frames]. Truncate to min frames."""
    n_frames = min(c.shape[0] for c in cir_list)
    M = len(cir_list)
    out = np.zeros((M, WINDOW_LEN, n_frames), dtype=np.complex128)
    for m in range(M):
        out[m] = align_one_port(
            cir_list[m][:n_frames],
            first_path_list[m][:n_frames],
            rx_pream_list[m][:n_frames],
            calib[m],
        )
    return out


def static_clutter_removal(rx: np.ndarray, win_radius: int = WIN_RADIUS) -> np.ndarray:
    """Sliding mean of past win_radius frames (MATLAB static_clutter_removal)."""
    _m, _l, n_frames = rx.shape
    out = np.empty_like(rx)
    cs = np.cumsum(rx, axis=2)
    for f in range(n_frames):
        a = max(0, f - win_radius)
        n = f - a
        if n <= 0:
            out[:, :, f] = rx[:, :, f]
            continue
        s = cs[:, :, f - 1]
        if a > 0:
            s = s - cs[:, :, a - 1]
        out[:, :, f] = rx[:, :, f] - s / n
    return out


def theta_and_range_axes(window_left: int = WINDOW_LEFT, window_len: int = WINDOW_LEN):
    k = np.arange(-(N_ANGLE / 2 - 1), N_ANGLE / 2)
    sin_theta = k / N_ANGLE * LAMBDA / D
    theta = np.degrees(np.arcsin(np.clip(sin_theta, -1.0, 1.0)))
    theta = theta.astype(np.float64)
    theta[np.abs(sin_theta) > 1] = np.nan
    ra = (np.arange(window_len) - window_left) / FS * C / 2
    return theta, ra


def start_list(num_frames: int, rd_window: int = RD_WINDOW) -> np.ndarray:
    last = num_frames - rd_window + 1
    if last < 100:
        return np.array([], dtype=int)
    return np.arange(100, last + 1, 10)


def ra_one_window(filtered: np.ndarray, win_idx_1based: int) -> np.ndarray:
    """Angle-FFT RA map [Nangle-1, range] for MATLAB window index (1-based)."""
    starts = start_list(filtered.shape[2])
    i = int(win_idx_1based) - 1
    if i < 0 or i >= len(starts):
        raise IndexError(f"window {win_idx_1based} not in 1..{len(starts)}")
    return _ra_from_start(filtered, int(starts[i]))


def compute_ra_maps_stream(filtered: np.ndarray) -> tuple[list[np.ndarray], np.ndarray, np.ndarray]:
    theta, ra = theta_and_range_axes(window_left=WINDOW_LEFT, window_len=filtered.shape[1])
    maps = []
    starts = start_list(filtered.shape[2])
    nwin = len(starts)
    for s, st in enumerate(starts):
        maps.append(_ra_from_start(filtered, int(st)))
        if (s + 1) % 40 == 0 or s + 1 == nwin:
            print(f"    RA win {s + 1}/{nwin}", flush=True)
    return maps, ra, theta


def _ra_from_start(filtered: np.ndarray, start_idx_1based: int) -> np.ndarray:
    # MATLAB start_idx is 1-based frame index
    s0 = start_idx_1based - 1
    sl = filtered[:, :, s0 : s0 + RD_WINDOW]  # [M, range, doppler]
    rd = np.fft.fftshift(np.fft.fft(sl, axis=2), axes=2)
    n_ang = N_ANGLE
    angle_full = np.fft.fftshift(np.fft.fft(rd, n=n_ang, axis=0), axes=0)
    angle_cube = np.abs(angle_full[1:, :, :])
    angle_cube[:, :, 40:43] = 0.0  # MATLAB 41:43
    return np.max(angle_cube, axis=2)


def extract_aoa_from_angle_maps(
    angle_maps: list[np.ndarray],
    range_axis: np.ndarray,
    theta_axis: np.ndarray,
    min_range: float = 1.8,
    range_max: float | None = None,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    valid = ~np.isnan(theta_axis)
    theta_valid = theta_axis[valid]
    ra = np.asarray(range_axis).reshape(-1)
    cluster_threshold = 0.9
    cluster_angle_range = 40.0
    cluster_range_range = 0.5
    n = len(angle_maps)
    aoa = np.full(n, np.nan)
    rng = np.full(n, np.nan)
    energy = np.full(n, np.nan)
    angle_grid, range_grid = np.meshgrid(theta_valid, ra, indexing="xy")
    angle_grid = angle_grid.T
    range_grid = range_grid.T

    for k, amap in enumerate(angle_maps):
        if amap.ndim == 3:
            amap = np.max(amap, axis=2)
        arv = amap[valid, :]
        processed = arv.copy()
        processed[:, ra < min_range] = 0
        if range_max is not None:
            processed[:, ra > range_max] = 0
        max_val = np.nanmax(processed)
        norm = processed / max_val if max_val > 0 else processed

        ge = ra >= min_range
        if range_max is not None:
            ge = ge & (ra <= range_max)
        if np.any(ge):
            sub = arv[:, ge]
            maxidx = int(np.nanargmax(sub))
            a_idx_v, r_f = np.unravel_index(maxidx, sub.shape)
            r_idx = np.flatnonzero(ge)[r_f]
            maxval = sub[a_idx_v, r_f]
        else:
            maxidx = int(np.nanargmax(arv))
            a_idx_v, r_idx = np.unravel_index(maxidx, arv.shape)
            maxval = arv[a_idx_v, r_idx]

        max_angle = theta_valid[a_idx_v]
        max_range = ra[r_idx]
        high = norm >= cluster_threshold
        nearby = (
            (np.abs(angle_grid - max_angle) <= cluster_angle_range)
            & (np.abs(range_grid - max_range) <= cluster_range_range)
            & high
            & (range_grid >= min_range)
        )
        if range_max is not None:
            nearby = nearby & (range_grid <= range_max)
        if np.any(nearby):
            w = norm[nearby]
            tw = np.sum(w)
            if tw > 0:
                cluster_angle = np.sum(angle_grid[nearby] * w) / tw
                cluster_range = np.sum(range_grid[nearby] * w) / tw
            else:
                cluster_angle, cluster_range = max_angle, max_range
        else:
            cluster_angle, cluster_range = max_angle, max_range
        aoa[k] = cluster_angle
        rng[k] = cluster_range
        energy[k] = maxval
    return aoa, rng, energy


def sync_by_id(
    sync_ids: list[np.ndarray],
    packet_type: list[np.ndarray] | None = None,
) -> tuple[list[np.ndarray], np.ndarray]:
    """MATLAB sync_frames_by_sequence: PacketType==1 then intersect sorted unique ids."""
    seqs = []
    keepers = []
    for i, sid in enumerate(sync_ids):
        sid = np.asarray(sid)
        if packet_type is not None:
            mask = np.asarray(packet_type[i]) == 1
            idx = np.flatnonzero(mask)
            sid = sid[mask]
        else:
            idx = np.arange(sid.size)
        seqs.append(sid)
        keepers.append(idx)
    common = seqs[0]
    for s in seqs[1:]:
        common = np.intersect1d(common, s)
    row_idx = []
    for sid, idx in zip(seqs, keepers):
        lookup = {}
        for j, v in enumerate(sid):
            lookup.setdefault(float(v), []).append(j)
        pos = []
        for v in common:
            js = lookup.get(float(v), [])
            pos.append(int(idx[js[0]]) if js else -1)
        row_idx.append(np.asarray(pos, dtype=int))
    return row_idx, common


def _matlab_argmax(arr: np.ndarray) -> tuple[int, ...]:
    """Linear index of first max, MATLAB column-major order."""
    idx = int(np.argmax(np.asarray(arr).ravel(order="F")))
    return np.unravel_index(idx, arr.shape, order="F")


def extract_aoa_track_continuous(
    angle_maps: list[np.ndarray],
    range_axis: np.ndarray,
    theta_axis: np.ndarray,
    *,
    init_min_range: float = 2.0,
    init_max_range: float | None = None,
    init_windows: int = 20,
    peak_rel_thr: float = 0.35,
    multi_hyp: bool = True,
    n_hypotheses: int = 5,
    hyp_cluster_range_m: float = 0.55,
    hyp_cluster_angle_deg: float = 18.0,
    prefer_aoa: float | None = None,
    prefer_aoa_gate: float = 20.0,
    gate_range_m: float = 0.8,
    gate_angle_deg: float = 25.0,
    sigma_range_m: float = 0.35,
    sigma_angle_deg: float = 12.0,
    vel_alpha: float = 0.35,
    min_range: float = 1.0,
    max_range: float | None = None,
    cluster_threshold: float = 0.75,
    cluster_range_m: float = 0.4,
    cluster_angle_deg: float = 15.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, dict]:
    """Port of Range_doppler/extract_aoa_track_continuous.m."""
    opt = {
        "init_min_range": init_min_range,
        "init_max_range": init_max_range,
        "init_windows": init_windows,
        "peak_rel_thr": peak_rel_thr,
        "n_hypotheses": n_hypotheses,
        "hyp_cluster_range_m": hyp_cluster_range_m,
        "hyp_cluster_angle_deg": hyp_cluster_angle_deg,
        "prefer_aoa": prefer_aoa,
        "prefer_aoa_gate": prefer_aoa_gate,
        "gate_range_m": gate_range_m,
        "gate_angle_deg": gate_angle_deg,
        "sigma_range_m": sigma_range_m,
        "sigma_angle_deg": sigma_angle_deg,
        "vel_alpha": vel_alpha,
        "min_range": min_range,
        "max_range": max_range,
        "cluster_threshold": cluster_threshold,
        "cluster_range_m": cluster_range_m,
        "cluster_angle_deg": cluster_angle_deg,
    }
    valid = ~np.isnan(theta_axis)
    theta_valid = theta_axis[valid]
    ra0 = np.asarray(range_axis).reshape(-1)
    angle_grid0, range_grid0 = np.meshgrid(theta_valid, ra0, indexing="xy")
    angle_grid0 = angle_grid0.T
    range_grid0 = range_grid0.T

    maps2d = []
    for amap in angle_maps:
        if amap.ndim == 3:
            amap = np.max(amap, axis=2)
        maps2d.append(np.asarray(amap)[valid, :])

    n_init = min(init_windows, len(maps2d))
    hyps = _collect_init_hypotheses(maps2d, ra0, theta_valid, n_init, opt)
    if not hyps:
        amap = maps2d[0].copy()
        amap[:, ~_range_mask(ra0, min_range, max_range)] = 0
        ai, ri = _matlab_argmax(amap)
        hyps = [{
            "a": float(theta_valid[ai]),
            "r": float(ra0[ri]),
            "e": float(amap[ai, ri]),
            "k0": 1,
            "label": "fallback",
        }]

    if (not multi_hyp) or len(hyps) == 1:
        aoa, rng, eng, assoc_ok, gate_expand = _run_track(
            maps2d, ra0, theta_valid, angle_grid0, range_grid0, hyps[0], opt
        )
        info = _pack_track_info(hyps[0], assoc_ok, gate_expand, 1, hyps)
        return aoa, rng, eng, info

    scores = np.full(len(hyps), -np.inf)
    tracks = []
    for h, hyp in enumerate(hyps):
        aoa_h, rng_h, eng_h, assoc_h, gate_h = _run_track(
            maps2d, ra0, theta_valid, angle_grid0, range_grid0, hyp, opt
        )
        scores[h] = _hypothesis_score(aoa_h, rng_h, eng_h, assoc_h, hyp)
        tracks.append((aoa_h, rng_h, eng_h, assoc_h, gate_h))
    best_i = int(np.argmax(scores))
    aoa, rng, eng, assoc_ok, gate_expand = tracks[best_i]
    info = _pack_track_info(hyps[best_i], assoc_ok, gate_expand, best_i + 1, hyps)
    info["hyp_scores"] = scores
    return aoa, rng, eng, info


def _range_mask(range_axis: np.ndarray, rmin: float, rmax: float | None) -> np.ndarray:
    m = np.asarray(range_axis) >= rmin
    if rmax is not None and np.isfinite(rmax):
        m = m & (np.asarray(range_axis) <= rmax)
    return m


def _hypothesis_score(aoa, range_m, energy, assoc_ok, hyp) -> float:
    n = aoa.size
    if n < 2:
        return -np.inf
    assoc_rate = float(np.mean(assoc_ok))
    e_ok = energy[np.isfinite(energy) & (energy > 0)]
    e_mean = float(np.mean(e_ok)) if e_ok.size else float(hyp["e"])
    if not np.isfinite(e_mean) or e_mean <= 0:
        e_mean = float(hyp["e"])
    d_a = np.abs(np.diff(aoa))
    d_r = np.abs(np.diff(range_m))
    jump_a = float(np.mean(d_a[np.isfinite(d_a)])) if d_a.size else 0.0
    jump_r = float(np.mean(d_r[np.isfinite(d_r)])) if d_r.size else 0.0
    smooth = 1.0 / (1.0 + jump_a / 8.0 + jump_r / 0.35)
    score = assoc_rate * np.log1p(e_mean) * smooth
    return float(score + 1e-3 * np.log1p(max(float(hyp["e"]), 0.0)))


def _local_peaks_ra(amap, theta_valid, range_axis, thr):
    peaks_a, peaks_r, peaks_e = [], [], []
    na, nr = amap.shape
    for ai in range(1, na - 1):
        for ri in range(1, nr - 1):
            v = amap[ai, ri]
            if v < thr:
                continue
            if v >= np.max(amap[ai - 1 : ai + 2, ri - 1 : ri + 2]):
                peaks_a.append(theta_valid[ai])
                peaks_r.append(range_axis[ri])
                peaks_e.append(v)
    if not peaks_r:
        ai, ri = _matlab_argmax(amap)
        v = amap[ai, ri]
        if v >= thr:
            return (
                np.array([theta_valid[ai]]),
                np.array([range_axis[ri]]),
                np.array([v]),
            )
        return np.array([]), np.array([]), np.array([])
    return np.asarray(peaks_a), np.asarray(peaks_r), np.asarray(peaks_e)


def _collect_init_hypotheses(maps2d, ra0, theta_valid, n_init, opt):
    cand_a, cand_r, cand_e, cand_k = [], [], [], []
    for k in range(n_init):
        amap = maps2d[k].copy()
        mask = _range_mask(ra0, opt["init_min_range"], opt["init_max_range"])
        if not np.any(mask):
            continue
        amap[:, ~mask] = 0
        mx = float(np.max(amap))
        if not (mx > 0):
            continue
        thr = opt["peak_rel_thr"] * mx
        pa, pr, pe = _local_peaks_ra(amap, theta_valid, ra0, thr)
        if pr.size == 0:
            continue
        if opt["prefer_aoa"] is not None:
            in_gate = np.abs(pa - opt["prefer_aoa"]) <= opt["prefer_aoa_gate"]
            if np.any(in_gate):
                pa, pr, pe = pa[in_gate], pr[in_gate], pe[in_gate]
        cand_a.append(pa)
        cand_r.append(pr)
        cand_e.append(pe)
        cand_k.append(np.full(pr.size, k + 1))
    if not cand_r:
        return []
    cand_a = np.concatenate(cand_a)
    cand_r = np.concatenate(cand_r)
    cand_e = np.concatenate(cand_e)
    cand_k = np.concatenate(cand_k)

    used = np.zeros(cand_r.size, dtype=bool)
    clusters = []
    for i in range(cand_r.size):
        if used[i]:
            continue
        memb = (
            (np.abs(cand_r - cand_r[i]) <= opt["hyp_cluster_range_m"])
            & (np.abs(cand_a - cand_a[i]) <= opt["hyp_cluster_angle_deg"])
            & (~used)
        )
        used[memb] = True
        clusters.append(np.flatnonzero(memb))

    n_c = len(clusters)
    c_a = np.zeros(n_c)
    c_r = np.zeros(n_c)
    c_e = np.zeros(n_c)
    c_k = np.zeros(n_c)
    c_n = np.zeros(n_c)
    c_score = np.zeros(n_c)
    for i, idx in enumerate(clusters):
        w = cand_e[idx]
        sw = float(np.sum(w)) + np.finfo(float).eps
        c_a[i] = float(np.sum(cand_a[idx] * w) / sw)
        c_r[i] = float(np.sum(cand_r[idx] * w) / sw)
        c_e[i] = float(np.max(w))
        c_k[i] = float(np.min(cand_k[idx]))
        c_n[i] = float(idx.size)
        c_score[i] = c_e[i] * np.sqrt(c_n[i])

    n_keep_score = min(n_c, max(2, opt["n_hypotheses"] - 2))
    ord_idx = np.argsort(-c_score, kind="mergesort")
    pick = list(ord_idx[:n_keep_score])
    i_e = int(np.argmax(c_e))
    i_r = int(np.argmax(c_r))
    for extra in (i_e, i_r):
        if extra not in pick:
            pick.append(extra)
    pick = pick[: opt["n_hypotheses"]]

    hyps = []
    for i in pick:
        if i == i_r and i == i_e:
            label = "strong+far"
        elif i == i_r:
            label = "far"
        elif i == i_e:
            label = "strong"
        else:
            label = "persist"
        hyps.append({
            "a": float(c_a[i]),
            "r": float(c_r[i]),
            "e": float(c_e[i]),
            "k0": int(max(1, c_k[i])),
            "label": label,
        })
    return hyps


def _associate(amap, angle_grid, range_grid, min_mask, a_pred, r_pred, gate_r, gate_a, opt):
    dr = range_grid - r_pred
    da = angle_grid - a_pred
    gate = min_mask & (np.abs(dr) <= gate_r) & (np.abs(da) <= gate_a)
    if not np.any(gate):
        return False, np.nan, np.nan, np.nan
    amp = amap.copy()
    amp[~gate] = 0
    mx = float(np.max(amp))
    if not (mx > 0):
        return False, np.nan, np.nan, np.nan
    score = amp * np.exp(
        -0.5
        * ((dr / opt["sigma_range_m"]) ** 2 + (da / opt["sigma_angle_deg"]) ** 2)
    )
    score[~gate] = 0
    ai, ri = _matlab_argmax(score)
    a0 = float(angle_grid[ai, ri])
    r0 = float(range_grid[ai, ri])
    e0 = float(amap[ai, ri])
    norm_map = amp / mx
    near = (
        gate
        & (np.abs(angle_grid - a0) <= opt["cluster_angle_deg"])
        & (np.abs(range_grid - r0) <= opt["cluster_range_m"])
        & (norm_map >= opt["cluster_threshold"])
    )
    if np.any(near):
        w = norm_map[near]
        sw = float(np.sum(w))
        a_new = float(np.sum(angle_grid[near] * w) / sw)
        r_new = float(np.sum(range_grid[near] * w) / sw)
    else:
        a_new, r_new = a0, r0
    return True, a_new, r_new, e0


def _run_track(maps2d, ra0, theta_valid, angle_grid0, range_grid0, hyp, opt):
    num_win = len(maps2d)
    aoa = np.full(num_win, np.nan)
    rng = np.full(num_win, np.nan)
    energy = np.full(num_win, np.nan)
    assoc_ok = np.zeros(num_win, dtype=bool)
    gate_expand = np.zeros(num_win, dtype=bool)
    k0 = int(hyp["k0"]) - 1
    r = float(hyp["r"])
    a = float(hyp["a"])
    e0 = float(hyp["e"])
    vr = 0.0
    va = 0.0
    aoa[k0] = a
    rng[k0] = r
    energy[k0] = e0
    assoc_ok[k0] = True
    aoa[:k0] = a
    rng[:k0] = r
    energy[:k0] = e0

    min_mask0 = _range_mask(range_grid0, opt["min_range"], opt["max_range"])
    for k in range(k0 + 1, num_win):
        r_pred = r + vr
        a_pred = a + va
        amap = maps2d[k]
        if amap.size == 0:
            aoa[k] = a_pred
            rng[k] = r_pred
            energy[k] = energy[k - 1]
            continue
        ok, a_new, r_new, e_new = _associate(
            amap, angle_grid0, range_grid0, min_mask0,
            a_pred, r_pred, opt["gate_range_m"], opt["gate_angle_deg"], opt,
        )
        if not ok:
            gate_expand[k] = True
            ok, a_new, r_new, e_new = _associate(
                amap, angle_grid0, range_grid0, min_mask0,
                a_pred, r_pred,
                opt["gate_range_m"] * 1.8, opt["gate_angle_deg"] * 1.8, opt,
            )
        if ok:
            assoc_ok[k] = True
            vr = (1 - opt["vel_alpha"]) * vr + opt["vel_alpha"] * (r_new - r)
            va = (1 - opt["vel_alpha"]) * va + opt["vel_alpha"] * (a_new - a)
            r, a = r_new, a_new
            aoa[k] = a
            rng[k] = r
            energy[k] = e_new
        else:
            r, a = r_pred, a_pred
            aoa[k] = a
            rng[k] = r
            energy[k] = energy[k - 1]
            vr *= 0.7
            va *= 0.7
    return aoa, rng, energy, assoc_ok, gate_expand


def _pack_track_info(best, assoc_ok, gate_expand, best_i, hyps) -> dict:
    return {
        "init_k": int(best["k0"]),
        "init_range": float(best["r"]),
        "init_aoa": float(best["a"]),
        "init_label": best["label"],
        "assoc_ok": assoc_ok,
        "gate_expand": gate_expand,
        "assoc_rate": float(np.mean(assoc_ok)),
        "best_hyp": int(best_i),
        "n_hyp": len(hyps),
    }
