#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Academic-style dynamic range vs range plot (SCR vs no-SCR).

Reads matched-range RD DR samples from dynamic_range_all_groups.csv,
aggregates with a sliding window (default 0.5 m width, 0.25 m step),
and saves publication-quality PDF/PNG into this folder.
"""

import os
import shutil
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import rcParams

# Same font/layout as Figure 10/11/12 combined paper figures
rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Times New Roman', 'Times', 'DejaVu Serif']
rcParams['font.weight'] = 'bold'
rcParams['axes.labelweight'] = 'bold'
rcParams['axes.titleweight'] = 'bold'
rcParams['axes.labelsize'] = 15
rcParams['xtick.labelsize'] = 14
rcParams['ytick.labelsize'] = 14
rcParams['legend.fontsize'] = 11
rcParams['axes.unicode_minus'] = False
rcParams['pdf.fonttype'] = 42
rcParams['ps.fonttype'] = 42

# Optional extra CSV if packs are absent (artifact uses curve_raw_npy).
DEFAULT_SOURCE_CSV = None


def sliding_window_stats(ranges, values, win_width=0.5, win_step=0.25):
    """
    Sliding-window mean/std over range.

    Windows: [start, start+win_width), stepped by win_step.
    Plot x uses window left edge (covers near-range end).
    """
    ranges = np.asarray(ranges, dtype=float)
    values = np.asarray(values, dtype=float)
    valid = np.isfinite(ranges) & np.isfinite(values)
    ranges = ranges[valid]
    values = values[valid]

    r_start = np.floor(np.min(ranges) / win_step) * win_step
    r_end = np.ceil(np.max(ranges) / win_step) * win_step
    win_starts = np.arange(r_start, r_end - win_width + 1e-12, win_step)

    xs, means, stds, counts = [], [], [], []
    for s in win_starts:
        mask = (ranges >= s) & (ranges < s + win_width)
        n = int(np.sum(mask))
        if n == 0:
            continue
        xs.append(s)  # left edge
        means.append(np.mean(values[mask]))
        stds.append(np.std(values[mask], ddof=0))
        counts.append(n)

    return (
        np.asarray(xs),
        np.asarray(means),
        np.asarray(stds),
        np.asarray(counts),
    )


def ensure_local_data(data_dir, source_csv=DEFAULT_SOURCE_CSV):
    """Prefer curve_raw_npy SCR/noSCR packs; else local/source CSV."""
    local_csv = os.path.join(data_dir, 'dynamic_range_all_groups.csv')
    npz_scr = os.path.join(
        os.path.dirname(data_dir), 'curve_raw_npy', 'Figure12', 'SCR', 'raw.npz'
    )
    npz_raw = os.path.join(
        os.path.dirname(data_dir), 'curve_raw_npy', 'Figure12', 'noSCR', 'raw.npz'
    )
    if os.path.isfile(npz_scr) and os.path.isfile(npz_raw):
        # Rebuild CSV from packed per-window measurement samples (not sliding-window stats)
        z_s = np.load(npz_scr)
        z_r = np.load(npz_raw)
        df = pd.DataFrame({
            'range_m': z_s['range_m'],
            'DR_scr_dB': z_s['DR_dB'],
            'DR_raw_dB': z_r['DR_dB'],
            'group': z_s['group'],
        })
        df.to_csv(local_csv, index=False)
        print(f"Built {local_csv} from curve_raw_npy SCR/noSCR raw.npz ({len(df)} samples)")
        return local_csv
    if os.path.exists(local_csv):
        return local_csv
    if source_csv and os.path.exists(source_csv):
        shutil.copy2(source_csv, local_csv)
        print(f"Copied source data -> {local_csv}")
        return local_csv
    raise FileNotFoundError(
        f"Need curve_raw_npy/Figure12 packs or {local_csv}"
    )


# Shared canvas with Figure11 bar plot for side-by-side LaTeX alignment
FIGSIZE = (3.5, 2.8)
SUBPLOT_RECT = dict(left=0.20, right=0.97, bottom=0.20, top=0.95)


def plot_dynamic_range_academic(
    data_dir=None,
    output_filename='figure12.pdf',
    win_width=0.5,
    win_step=0.25,
    source_csv=DEFAULT_SOURCE_CSV,
):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))

    csv_path = ensure_local_data(data_dir, source_csv=source_csv)
    df = pd.read_csv(csv_path)
    print(f"Loaded {len(df)} samples from {csv_path}")
    print(f"  groups: {sorted(df['group'].unique().tolist())}")

    ranges = df['range_m'].to_numpy()
    dr_raw = df['DR_raw_dB'].to_numpy()
    dr_scr = df['DR_scr_dB'].to_numpy()

    x_raw, mu_raw, sd_raw, n_raw = sliding_window_stats(
        ranges, dr_raw, win_width=win_width, win_step=win_step
    )
    x_scr, mu_scr, sd_scr, n_scr = sliding_window_stats(
        ranges, dr_scr, win_width=win_width, win_step=win_step
    )

    # Save aggregated curve data used by the figure
    stats_path = os.path.join(data_dir, 'dynamic_range_sliding_window_stats.csv')
    stats_df = pd.DataFrame({
        'range_m': x_raw,
        'DR_raw_mean_dB': mu_raw,
        'DR_raw_std_dB': sd_raw,
        'DR_raw_n': n_raw,
        'DR_scr_mean_dB': mu_scr,
        'DR_scr_std_dB': sd_scr,
        'DR_scr_n': n_scr,
        'win_width_m': win_width,
        'win_step_m': win_step,
    })
    stats_df.to_csv(stats_path, index=False)
    print(f"Saved sliding-window stats -> {stats_path}")

    # Colors consistent with other academic figures in this repo
    color_raw = '#5B4E77'   # deep purple-blue
    color_scr = '#CD853F'   # peru / warm brown-orange

    # Restrict plotted range to [1.0, 5.5] m
    x_max = 5.5
    mask_raw = (x_raw >= 1.0) & (x_raw <= x_max)
    mask_scr = (x_scr >= 1.0) & (x_scr <= x_max)
    x_raw, mu_raw, sd_raw = x_raw[mask_raw], mu_raw[mask_raw], sd_raw[mask_raw]
    x_scr, mu_scr, sd_scr = x_scr[mask_scr], mu_scr[mask_scr], sd_scr[mask_scr]

    # Create figure with size suitable for two-column format
    fig, ax = plt.subplots(figsize=FIGSIZE)

    # +/- 1 std shade
    ax.fill_between(
        x_raw, mu_raw - sd_raw, mu_raw + sd_raw,
        color=color_raw, alpha=0.18, linewidth=0, zorder=2
    )
    ax.fill_between(
        x_scr, mu_scr - sd_scr, mu_scr + sd_scr,
        color=color_scr, alpha=0.18, linewidth=0, zorder=2
    )

    # Sliding-window mean curves
    ax.plot(
        x_raw, mu_raw, '-o',
        color=color_raw, linewidth=1.5, markersize=3.5,
        markerfacecolor=color_raw, label='No SCR', zorder=3
    )
    ax.plot(
        x_scr, mu_scr, '-s',
        color=color_scr, linewidth=1.5, markersize=3.5,
        markerfacecolor=color_scr, label='With SCR', zorder=3
    )

    ax.axhline(y=0, color='k', linestyle='--', linewidth=1.0, alpha=0.7, zorder=2)

    # Labels / legend / ticks: match Figure 10/11/12 combined style
    ax.set_xlabel('Range (m)', fontsize=15, fontweight='bold')
    ax.set_ylabel('SNR (dB)', fontsize=15, fontweight='bold')

    # No title for academic paper style
    ax.legend(
        fontsize=11, loc='upper right', framealpha=0.95, frameon=True,
        borderaxespad=0, prop={'size': 11, 'weight': 'bold'},
    )

    # Grid settings
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)

    ax.set_xlim([1.0, x_max])
    ax.set_xticks([1, 2, 3, 4, 5])
    ax.set_ylim([-10, 40])
    ax.tick_params(labelsize=14)
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontweight('bold')
        label.set_fontfamily('serif')

    # Fixed axes box (no tight crop) so LaTeX side-by-side x-axes align
    fig.subplots_adjust(**SUBPLOT_RECT)

    output_path = os.path.join(data_dir, output_filename)
    plt.savefig(output_path, format='pdf')
    print(f"Academic-style plot saved to: {output_path}")

    # Also save PNG preview
    png_path = os.path.splitext(output_path)[0] + '.png'
    plt.savefig(png_path, format='png', dpi=300)
    print(f"PNG preview saved to: {png_path}")
    plt.close(fig)

    print("\nSummary:")
    print(f"  No SCR : mean={np.mean(dr_raw):.2f} dB, median={np.median(dr_raw):.2f} dB")
    print(f"  With SCR: mean={np.mean(dr_scr):.2f} dB, median={np.median(dr_scr):.2f} dB")
    print(f"  Mean SCR-raw gap: {np.mean(dr_scr - dr_raw):.2f} dB")

    return output_path


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print("=" * 80)
    print("Academic-Style Dynamic Range Plot Generator")
    print("=" * 80)
    print()

    output_file = plot_dynamic_range_academic(data_dir=script_dir)

    print("\n" + "=" * 80)
    if output_file:
        print("Dynamic range plot generation completed!")
    else:
        print("Error: Failed to generate dynamic range plot")
    print("=" * 80)
