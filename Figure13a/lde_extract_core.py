#!/usr/bin/env python3
"""
Two-stage LDE phase extraction with LDE logic aligned to
Concurrent/export_lde_complex_8antenna.m.

The plotting/statistics flow is reused from extract_lde_phases_two_stage.py;
only the LDE extraction algorithm and parameters are replaced here.
"""

import argparse
import re
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.interpolate import PchipInterpolator


def approx_mag(xc):
    I = np.abs(np.real(xc))
    Q = np.abs(np.imag(xc))
    return np.maximum(I, Q) + 0.25 * np.minimum(I, Q)


def interpolate_complex(x, x_bins, xi, method="pchip"):
    x = np.asarray(x)
    re = np.real(x)
    im = np.imag(x)
    if method == "pchip":
        re_i = PchipInterpolator(x_bins, re)(xi)
        im_i = PchipInterpolator(x_bins, im)(xi)
    else:
        re_i = np.interp(xi, x_bins, re)
        im_i = np.interp(xi, x_bins, im)
    return re_i + 1j * im_i




def tail_number_from_filename(fname):
    """Match MATLAB regexp: '(\\d+)(?=[^\\d]*$)'."""
    match = re.search(r"(\d+)(?=[^\d]*$)", str(fname))
    if match is None:
        return np.nan, False

    num = float(match.group(1))
    return num, np.isfinite(num)


def lde_fullscan(x, P):
    """
    Python equivalent of export_lde_complex_8antenna.m::lde_fullscan.

    candIdx uses MATLAB-style 1-based fractional bin coordinates so the caller
    can apply the same `candIdx + 0.5` correction as the MATLAB script.
    """
    x = np.asarray(x, dtype=float)
    N = len(x)

    noise_region_bins = 10
    merge_gap_below = 2
    min_stay_above = 3
    early_peak_frac = 0.80
    grad_span = max(1, int(P.get("gradWin", 3)))

    valid = np.ones(N, dtype=bool)
    ignore_range = P.get("ignoreRange")
    if ignore_range is not None and len(ignore_range) >= 2:
        start = max(1, int(ignore_range[0]))
        end = min(N, int(ignore_range[1]))
        if start <= end:
            valid[start - 1:end] = False

    valid_idx = np.flatnonzero(valid)
    start_idx = int(valid_idx[0]) if len(valid_idx) else 0
    noise_end = min(N, start_idx + noise_region_bins)
    thr = P["thFactor"] * np.mean(x[start_idx:noise_end])

    above = x >= thr
    if merge_gap_below > 0:
        below = ~above
        padded = np.concatenate(([False], below, [False])).astype(int)
        dz = np.diff(padded)
        gap_starts = np.flatnonzero(dz == 1)
        gap_ends = np.flatnonzero(dz == -1) - 1

        for left, right in zip(gap_starts, gap_ends):
            gap_len = right - left + 1
            left_ok = left - 1 >= 0 and above[left - 1]
            right_ok = right + 1 < N and above[right + 1]
            if gap_len <= merge_gap_below and left_ok and right_ok:
                above[left:right + 1] = True

    padded = np.concatenate(([False], above, [False])).astype(int)
    d = np.diff(padded)
    cluster_starts = np.flatnonzero(d == 1) + 1
    cluster_ends = np.flatnonzero(d == -1)

    if len(cluster_starts) == 0:
        return np.array([]), np.array([]), thr

    g = np.diff(x)
    cand_idx = []
    cand_amp = []

    for s_bin, e_bin in zip(cluster_starts, cluster_ends):
        stay_hi = min(e_bin, s_bin + max(0, min_stay_above - 1))
        if np.sum(x[s_bin - 1:stay_hi] >= thr) < min_stay_above:
            continue

        cluster = x[s_bin - 1:e_bin]
        peak_amp_max = np.max(cluster)
        peak_idx_max = s_bin + int(np.argmax(cluster))

        locs = []
        for i in range(max(s_bin + 1, 2), min(e_bin - 1, N - 1) + 1):
            if x[i - 1] >= x[i - 2] and x[i - 1] > x[i]:
                locs.append(i)
        locs = [i for i in locs if x[i - 1] >= thr]

        if locs:
            strong_locs = [i for i in locs if x[i - 1] >= early_peak_frac * peak_amp_max]
            seed = strong_locs[0] if strong_locs else locs[0]
        else:
            seed = peak_idx_max

        lo = max(2, s_bin - grad_span)
        hi = min(N - 1, s_bin + grad_span)
        hi = min(hi, seed - 1)
        if lo > hi:
            lo = max(2, seed - 1)
            hi = seed - 1
            if lo > hi:
                lo = max(2, s_bin)
                hi = lo

        segment = g[lo - 1:hi]
        if len(segment) == 0:
            continue

        m = lo + int(np.argmax(segment))

        gm1 = g[max(m - 1, 1) - 1]
        g0 = g[m - 1]
        gp1 = g[min(m + 1, N - 1) - 1]
        denom = g0 - min(gm1, gp1)
        if denom <= 0:
            frac = 0.0
        else:
            frac = 0.5 * (gp1 - gm1) / denom
            frac = max(-0.5, min(0.5, frac))

        lde = m + frac
        lde = min(lde, seed - 0.5)

        if P.get("quantize64", True):
            lde = np.round(lde * 64) / 64

        cand_idx.append(lde)
        cand_amp.append(peak_amp_max)

    return np.asarray(cand_idx), np.asarray(cand_amp), thr


def _interp_grid(nbins, upsample_factor):
    """Match MATLAB `1 : 1/L : Nbins` exactly."""
    return 1 + np.arange((nbins - 1) * upsample_factor + 1) / upsample_factor


def extract_lde_two_phases(csv_file):
    """
    Extract two LDE phases using the same LDE parameters and assignment logic as
    Concurrent/export_lde_complex_8antenna.m.
    """
    df = pd.read_csv(csv_file)

    cir_real_cols = [c for c in df.columns if c.startswith("CIR_real_")]
    cir_imag_cols = [c for c in df.columns if c.startswith("CIR_imag_")]

    def get_index(col_name):
        return int(col_name.split("_")[-1])

    cir_real_cols = sorted(cir_real_cols, key=get_index)
    cir_imag_cols = sorted(cir_imag_cols, key=get_index)

    assert len(cir_real_cols) == len(cir_imag_cols), "CIR_real和CIR_imag列数不匹配"

    nbins = len(cir_real_cols)
    nf = len(df)

    r = df[cir_real_cols].values.astype(float)
    i = df[cir_imag_cols].values.astype(float)
    cir = r + 1j * i

    P = {
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

    upsample_factor = 64
    interp_method = "pchip"

    expected_gap, has_gap_num = tail_number_from_filename(csv_file)
    gap_tol = 10

    phase_small = np.full(nf, np.nan)
    phase_large = np.full(nf, np.nan)
    thresholds = np.full(nf, np.nan)
    lde_positions = []
    magnitudes = []

    x_bins = np.arange(1, nbins + 1)
    xi = _interp_grid(nbins, upsample_factor)

    def idx_up(lf, n_up):
        return int(np.clip(np.round((lf - 1) * upsample_factor), 0, n_up - 1))

    for k in range(nf):
        x = cir[k, :]
        mag = approx_mag(x)
        magnitudes.append(mag.copy())

        cand_idx, cand_amp, thr = lde_fullscan(mag, P)
        thresholds[k] = thr

        lde_pos = {"small": np.nan, "large": np.nan, "all_candidates": []}
        for idx, amp in zip(cand_idx, cand_amp):
            lde_pos["all_candidates"].append({"pos": idx + 0.5, "amp": amp})

        if len(cand_idx) == 0:
            lde_positions.append(lde_pos)
            continue

        sort_idx = np.argsort(cand_amp)[::-1][:min(2, len(cand_amp))]
        ldes = cand_idx[sort_idx] + 0.5

        x_up = interpolate_complex(x, x_bins, xi, interp_method)

        if len(ldes) < 2:
            single_lde = ldes[0]
            phase_small[k] = np.angle(x_up[idx_up(single_lde, len(x_up))])
            lde_pos["small"] = single_lde
            lde_positions.append(lde_pos)
            continue

        ldes_sorted = np.sort(ldes)
        small_idx = ldes_sorted[0]
        large_idx = ldes_sorted[1]
        delta = large_idx - small_idx

        do_swap = False
        if has_gap_num:
            if delta > (expected_gap + gap_tol):
                do_swap = True
            elif abs(delta - expected_gap) <= gap_tol:
                do_swap = False
            elif delta < (expected_gap - gap_tol):
                do_swap = True
            else:
                do_swap = False

        v_small = x_up[idx_up(small_idx, len(x_up))]
        v_large = x_up[idx_up(large_idx, len(x_up))]

        if not do_swap:
            phase_small[k] = np.angle(v_small)
            phase_large[k] = np.angle(v_large)
            lde_pos["small"] = small_idx
            lde_pos["large"] = large_idx
        else:
            phase_small[k] = np.angle(v_large)
            phase_large[k] = np.angle(v_small)
            lde_pos["small"] = large_idx
            lde_pos["large"] = small_idx

        lde_positions.append(lde_pos)

    frame_indices = np.arange(1, nf + 1)
    return frame_indices, phase_small, phase_large, thresholds, lde_positions, magnitudes


def extract_lde_complex_from_cir(cir, expected_gap=None):
    """
    CIR [Nf, Nbins] complex → complex_small/large (same as export_lde_complex_8antenna.m).
    expected_gap: optional filename-style gap hint; None disables swap logic.
    """
    cir = np.asarray(cir)
    nf, nbins = cir.shape
    P = {
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
    L = 64
    gap_tol = 10
    has_gap = expected_gap is not None and np.isfinite(expected_gap)

    complex_small = np.full(nf, np.nan + 1j * np.nan, dtype=np.complex128)
    complex_large = np.full(nf, np.nan + 1j * np.nan, dtype=np.complex128)
    lde_small_pos = np.full(nf, np.nan)
    lde_large_pos = np.full(nf, np.nan)

    x_bins = np.arange(1, nbins + 1)
    xi = _interp_grid(nbins, L)

    def idx_up(lf, n_up):
        return int(np.clip(np.round((lf - 1) * L), 0, n_up - 1))

    for k in range(nf):
        x = cir[k]
        mag = approx_mag(x)
        cand_idx, cand_amp, _thr = lde_fullscan(mag, P)
        if len(cand_idx) == 0:
            continue
        sort_idx = np.argsort(cand_amp)[::-1][: min(2, len(cand_amp))]
        ldes = cand_idx[sort_idx] + 0.5
        x_up = interpolate_complex(x, x_bins, xi, "pchip")
        if len(ldes) < 2:
            complex_small[k] = x_up[idx_up(ldes[0], len(x_up))]
            lde_small_pos[k] = ldes[0]
            continue
        ldes_sorted = np.sort(ldes)
        small_idx, large_idx = ldes_sorted[0], ldes_sorted[1]
        delta = large_idx - small_idx
        do_swap = False
        if has_gap:
            if delta > (expected_gap + gap_tol):
                do_swap = True
            elif abs(delta - expected_gap) <= gap_tol:
                do_swap = False
            elif delta < (expected_gap - gap_tol):
                do_swap = True
        v_small = x_up[idx_up(small_idx, len(x_up))]
        v_large = x_up[idx_up(large_idx, len(x_up))]
        if not do_swap:
            complex_small[k], complex_large[k] = v_small, v_large
            lde_small_pos[k], lde_large_pos[k] = small_idx, large_idx
        else:
            complex_small[k], complex_large[k] = v_large, v_small
            lde_small_pos[k], lde_large_pos[k] = large_idx, small_idx

    return complex_small, complex_large, lde_small_pos, lde_large_pos


def read_sequences(csv_file, fallback_len):
    df = pd.read_csv(csv_file, usecols=lambda c: c == "Sequence")
    if "Sequence" not in df.columns:
        return np.arange(fallback_len)
    return df["Sequence"].to_numpy()


def align_two_by_sequence(seq1, seq2):
    """Match the two-step sequence padding/alignment used by the MATLAB test."""
    if len(seq1) == 0 or len(seq2) == 0:
        return np.array([], dtype=int), np.array([], dtype=int)

    min_start_seq = min(seq1[0], seq2[0])
    n_padded = max(len(seq1), len(seq2))
    seq_range = np.mod(min_start_seq + np.arange(n_padded), 256)

    seqs = [np.asarray(seq1), np.asarray(seq2)]
    ptrs = [0, 0]
    aligned = [[], []]

    for seq in seq_range:
        row_indices = []
        for port_idx, port_seq in enumerate(seqs):
            ptr = ptrs[port_idx]
            matched_idx = None

            if ptr < len(port_seq) and port_seq[ptr] == seq:
                matched_idx = ptr
                ptr += 1
            else:
                while ptr < len(port_seq):
                    seq_orig = port_seq[ptr]
                    forward_dist = seq_orig - seq if seq_orig >= seq else seq_orig - seq + 256
                    if forward_dist >= 128:
                        ptr += 1
                    else:
                        break

            ptrs[port_idx] = ptr
            row_indices.append(matched_idx)

        if row_indices[0] is not None and row_indices[1] is not None:
            aligned[0].append(row_indices[0])
            aligned[1].append(row_indices[1])

    return np.asarray(aligned[0], dtype=int), np.asarray(aligned[1], dtype=int)


def lde_distance_valid(lde_pos, min_distance=30):
    candidates = lde_pos.get("all_candidates", [])
    if len(candidates) < 2:
        return False

    top2 = sorted(candidates, key=lambda c: c["amp"], reverse=True)[:2]
    positions = sorted(c["pos"] for c in top2)
    return (positions[1] - positions[0]) > min_distance


def wrap_phase(phase):
    return np.angle(np.exp(1j * phase))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Standalone two-CSV LDE phase demo")
    parser.add_argument("csv1", help="First antenna CIR CSV")
    parser.add_argument("csv2", help="Second antenna CIR CSV")
    args = parser.parse_args(argv)

    output_dir = Path(__file__).resolve().parent
    csv_file1 = args.csv1
    csv_file2 = args.csv2
    
    csv_path1 = Path(csv_file1)
    csv_path2 = Path(csv_file2)

    if not csv_path1.exists():
        print(f"错误: 找不到文件1: {csv_file1}")
        return
    if not csv_path2.exists():
        print(f"错误: 找不到文件2: {csv_file2}")
        return

    print(f"正在处理文件1: {csv_file1}")
    print("使用与 export_lde_complex_8antenna.m 一致的LDE逻辑提取两个LDE相位中...")
    frame_indices1, phase_small1, phase_large1, thresholds1, lde_positions1, magnitudes1 = extract_lde_two_phases(csv_file1)

    print(f"\n正在处理文件2: {csv_file2}")
    print("使用与 export_lde_complex_8antenna.m 一致的LDE逻辑提取两个LDE相位中...")
    frame_indices2, phase_small2, phase_large2, thresholds2, lde_positions2, magnitudes2 = extract_lde_two_phases(csv_file2)

    seq1 = read_sequences(csv_file1, len(frame_indices1))
    seq2 = read_sequences(csv_file2, len(frame_indices2))
    aligned_idx1, aligned_idx2 = align_two_by_sequence(seq1, seq2)
    max_frames = len(aligned_idx1)

    aligned_idx1 = aligned_idx1[:max_frames]
    aligned_idx2 = aligned_idx2[:max_frames]

    frame_indices = np.arange(1, max_frames + 1)
    phase_small1 = phase_small1[aligned_idx1]
    phase_large1 = phase_large1[aligned_idx1]
    phase_small2 = phase_small2[aligned_idx2]
    phase_large2 = phase_large2[aligned_idx2]
    thresholds1_aligned = thresholds1[aligned_idx1]
    thresholds2_aligned = thresholds2[aligned_idx2]
    lde_positions1_aligned = [lde_positions1[i] for i in aligned_idx1]
    lde_positions2_aligned = [lde_positions2[i] for i in aligned_idx2]
    magnitudes1_aligned = [magnitudes1[i] for i in aligned_idx1]
    magnitudes2_aligned = [magnitudes2[i] for i in aligned_idx2]

    valid1 = ~np.isnan(phase_small1) & ~np.isnan(phase_large1)
    valid2 = ~np.isnan(phase_small2) & ~np.isnan(phase_large2)
    distance_valid1 = np.array([lde_distance_valid(pos) for pos in lde_positions1_aligned])
    distance_valid2 = np.array([lde_distance_valid(pos) for pos in lde_positions2_aligned])
    valid1 &= distance_valid1
    valid2 &= distance_valid2

    phase_diff1_raw = np.full(max_frames, np.nan)
    phase_diff2_raw = np.full(max_frames, np.nan)

    if np.any(valid1):
        phase_diff1_raw[valid1] = phase_small1[valid1] - phase_large1[valid1]
    if np.any(valid2):
        phase_diff2_raw[valid2] = phase_small2[valid2] - phase_large2[valid2]

    # MATLAB keeps the intermediate difference raw, then maps it back onto the
    # phase circle before AoA. These plotted phase quantities need the same wrap.
    phase_diff1 = wrap_phase(phase_diff1_raw)
    phase_diff2 = wrap_phase(phase_diff2_raw)

    valid_both = valid1 & valid2
    double_diff = np.full(max_frames, np.nan)
    if np.any(valid_both):
        double_diff[valid_both] = wrap_phase(phase_diff2_raw[valid_both] - phase_diff1_raw[valid_both])

    print(f"\nSequence对齐后总帧数（全部）: {max_frames}")
    print(f"文件1有效两LDE数: {np.sum(valid1)}")
    print(f"文件2有效两LDE数: {np.sum(valid2)}")
    print(f"同时有效的帧数: {np.sum(valid_both)}")

    valid_thr1 = ~np.isnan(thresholds1_aligned)
    valid_thr2 = ~np.isnan(thresholds2_aligned)
    if np.any(valid_thr1):
        thr1_vals = thresholds1_aligned[valid_thr1]
        print("\n=== 文件1阈值统计信息 ===")
        print(f"阈值范围: [{np.min(thr1_vals):.2f}, {np.max(thr1_vals):.2f}]")
        print(f"阈值平均值: {np.mean(thr1_vals):.2f}")
    if np.any(valid_thr2):
        thr2_vals = thresholds2_aligned[valid_thr2]
        print("\n=== 文件2阈值统计信息 ===")
        print(f"阈值范围: [{np.min(thr2_vals):.2f}, {np.max(thr2_vals):.2f}]")
        print(f"阈值平均值: {np.mean(thr2_vals):.2f}")

    print(f"\n文件1相位差范围: [{np.nanmin(phase_diff1):.3f}, {np.nanmax(phase_diff1):.3f}] rad")
    print(f"文件2相位差范围: [{np.nanmin(phase_diff2):.3f}, {np.nanmax(phase_diff2):.3f}] rad")
    if np.any(valid_both):
        print(f"Double Difference范围: [{np.nanmin(double_diff):.3f}, {np.nanmax(double_diff):.3f}] rad")
        print(f"Double Difference均值: {np.nanmean(double_diff):.3f} rad")
        print(f"Double Difference标准差: {np.nanstd(double_diff):.3f} rad")

    print("\n正在绘制文件1前10帧CIR可视化...")
    cir_output_file1 = output_dir / "cir_lde_visualization_file1_new.pdf"
    base.visualize_cir_with_lde(
        magnitudes1_aligned,
        lde_positions1_aligned,
        thresholds1_aligned,
        num_frames=10,
        output_file=str(cir_output_file1),
        title_prefix="File 1: ",
    )

    print("\n正在绘制文件2前10帧CIR可视化...")
    cir_output_file2 = output_dir / "cir_lde_visualization_file2_new.pdf"
    base.visualize_cir_with_lde(
        magnitudes2_aligned,
        lde_positions2_aligned,
        thresholds2_aligned,
        num_frames=10,
        output_file=str(cir_output_file2),
        title_prefix="File 2: ",
    )

    output_file_raw = output_dir / "lde_phases_two_stage_raw_new.pdf"
    print("\n正在绘制原始相位图形...")
    base.plot_two_stage_phases(frame_indices, phase_diff1, phase_diff2, double_diff, str(output_file_raw))

    print("\n正在应用异常值过滤（基于Double Difference）...")
    double_diff_consistent = double_diff.copy()
    double_diff_consistent[~valid_both] = np.nan

    valid_mask = base.filter_outliers_by_phase_diff(double_diff_consistent, method="iqr", factor=1.5)

    phase_diff1_filtered = phase_diff1.copy()
    phase_diff2_filtered = phase_diff2.copy()
    double_diff_filtered = double_diff_consistent.copy()

    final_valid = valid_both & valid_mask & (~np.isnan(double_diff_consistent))

    phase_diff1_filtered[~final_valid] = np.nan
    phase_diff2_filtered[~final_valid] = np.nan
    double_diff_filtered[~final_valid] = np.nan

    num_outliers = np.sum(~final_valid)
    num_valid = np.sum(final_valid)
    print(f"过滤前有效数据点: {np.sum(valid_both)}")
    print(f"过滤后保留数据点: {num_valid}")
    print(f"过滤掉的异常值: {num_outliers}")

    if np.any(final_valid):
        print(f"\n过滤后Double Difference范围: [{np.nanmin(double_diff_filtered):.3f}, {np.nanmax(double_diff_filtered):.3f}] rad")
        print(f"过滤后Double Difference均值: {np.nanmean(double_diff_filtered):.3f} rad")
        print(f"过滤后Double Difference标准差: {np.nanstd(double_diff_filtered):.3f} rad")

    output_file_filtered = output_dir / "lde_phases_two_stage_filtered_new.pdf"
    print("\n正在绘制过滤后的相位图形...")
    base.plot_two_stage_phases(frame_indices, phase_diff1_filtered, phase_diff2_filtered, double_diff_filtered, str(output_file_filtered))

    errorbar_output = output_dir / "phase_distribution_errorbar_new.pdf"
    print("\n正在绘制相位分布errorbar图...")
    base.plot_phase_distribution_errorbar(
        phase_small1[:max_frames],
        phase_small2[:max_frames],
        phase_diff1_filtered,
        phase_diff2_filtered,
        double_diff_filtered,
        str(errorbar_output),
    )

    print("\n完成!")


if __name__ == "__main__":
    main()
