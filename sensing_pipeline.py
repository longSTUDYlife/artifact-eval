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
        max_val = np.nanmax(processed)
        norm = processed / max_val if max_val > 0 else processed

        ge = ra >= min_range
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
        )
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
