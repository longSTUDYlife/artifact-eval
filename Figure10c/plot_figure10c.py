#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 10(c): Angular accuracy (Sensing) — standalone CDF.

CIR (curve_raw_npy/Figure10c/.../raw.npz or raw/*.csv)
  → process_from_cir.py (seq-sync + angle-FFT RA)
  → aoa_estimates/aoa_estimates_{2,4,8}port_{angle}.csv
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib import rcParams

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

STYLE_8 = {'color': '#F18F01', 'linestyle': '-.', 'linewidth': 1.5}
STYLE_4 = {'color': '#C73E1D', 'linestyle': '--', 'linewidth': 1.5}
STYLE_2 = {'color': '#2E86AB', 'linestyle': '-', 'linewidth': 1.5}

ANGLES = [-40, -30, -20, -10, 0, 10, 20, 30, 40]


def ensure_processed(data_dir, force=False):
    """CIR → AoA CSVs via process_from_cir (Docker / no MATLAB)."""
    fig_dir = data_dir
    est_dir = os.path.join(data_dir, 'aoa_estimates')
    if os.path.basename(os.path.abspath(data_dir)) == 'aoa_estimates':
        est_dir = data_dir
        fig_dir = os.path.dirname(data_dir)
    from_cir = os.environ.get('FIGURE10C_FROM_CIR', '1').lower() in ('1', 'true', 'yes')
    needed = [
        os.path.join(est_dir, f'aoa_estimates_{n}port_{ang}.csv')
        for n in (8, 4, 2) for ang in ANGLES
    ]
    if force or from_cir or not all(os.path.isfile(f) for f in needed):
        from process_from_cir import run_batch
        print('Running Python CIR→RA AoA pipeline (process_from_cir) ...')
        run_batch(Path(fig_dir), force=True)
    return est_dir


def detect_outliers_zscore(data, threshold=3):
    mean = np.mean(data)
    std = np.std(data)
    if std == 0:
        return np.ones_like(data, dtype=bool)
    return np.abs((data - mean) / std) < threshold


def load_sensing_errors(data_dir, apply_filter=False, file_suffix=''):
    """Load AoA errors. apply_filter=False: use all rows independently per port.

    file_suffix: e.g. '' (primary), '_baseline', '_track'
    """
    all_errors_8, all_errors_4, all_errors_2 = [], [], []

    for angle in ANGLES:
        paths = {
            8: os.path.join(data_dir, f'aoa_estimates_8port_{angle}{file_suffix}.csv'),
            4: os.path.join(data_dir, f'aoa_estimates_4port_{angle}{file_suffix}.csv'),
            2: os.path.join(data_dir, f'aoa_estimates_2port_{angle}{file_suffix}.csv'),
        }
        dfs = {}
        for nport, path in paths.items():
            if os.path.exists(path):
                df = pd.read_csv(path)
                df['error'] = df['estimated_aoa'] - df['aoa']
                df['key'] = df['frame'].astype(str) + '_' + df['times'].astype(str)
                dfs[nport] = df
            else:
                print(f'  angle {angle:+d}°: missing {nport}port, skip that port')

        if not dfs:
            continue

        if apply_filter and len(dfs) == 3:
            def valid_mask(df):
                va = detect_outliers_zscore(df['estimated_aoa'].values, 3)
                ve = detect_outliers_zscore(df['error'].values, 3)
                vab = np.abs(df['error'].values) < 30
                return va & ve & vab

            valid_keys = (
                set(dfs[8].loc[valid_mask(dfs[8]), 'key'])
                & set(dfs[4].loc[valid_mask(dfs[4]), 'key'])
                & set(dfs[2].loc[valid_mask(dfs[2]), 'key'])
            )
            for nport, bucket in ((8, all_errors_8), (4, all_errors_4), (2, all_errors_2)):
                bucket.extend(dfs[nport].loc[dfs[nport]['key'].isin(valid_keys), 'error'].values)
            print(f'  angle {angle:+d}°: N={len(valid_keys)} common frames (filtered)')
        else:
            for nport, bucket in ((8, all_errors_8), (4, all_errors_4), (2, all_errors_2)):
                if nport in dfs:
                    bucket.extend(dfs[nport]['error'].values)
            ns = {n: len(dfs[n]) if n in dfs else 0 for n in (8, 4, 2)}
            print(f'  angle {angle:+d}°: N8={ns[8]} N4={ns[4]} N2={ns[2]} (no filter)')

    return (
        np.asarray(all_errors_8, dtype=float),
        np.asarray(all_errors_4, dtype=float),
        np.asarray(all_errors_2, dtype=float),
    )


def plot_figure10c(data_dir=None, output_basename='figure10c', apply_filter=False,
                   output_dir=None, file_suffix='', reprocess=False):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))
    if output_dir is None:
        output_dir = data_dir if os.path.basename(os.path.abspath(data_dir)) != 'aoa_estimates' \
            else os.path.dirname(data_dir)

    est_dir = ensure_processed(data_dir, force=reprocess)

    print(f'Loading sensing aoa_estimates (apply_filter={apply_filter}, suffix={file_suffix!r}) ...')
    err_8, err_4, err_2 = load_sensing_errors(
        est_dir, apply_filter=apply_filter, file_suffix=file_suffix)
    if err_8.size == 0 and err_4.size == 0 and err_2.size == 0:
        raise FileNotFoundError(f'No sensing AoA estimates in {data_dir}')

    configs = [
        (np.abs(err_8), '8RX-ULA', STYLE_8),
        (np.abs(err_4), '4RX-ULA', STYLE_4),
        (np.abs(err_2), '2RX-ULA', STYLE_2),
    ]

    fig, ax = plt.subplots(figsize=(5.0, 4.5))
    for errors, label, style in configs:
        if errors.size == 0:
            print(f'  {label}: empty, skip')
            continue
        sorted_err = np.sort(errors)
        cdf = np.arange(1, len(sorted_err) + 1) / len(sorted_err)
        # Default line (not steps-post): matches Figure10 paper look; dense CDFs
        # already look continuous, while sparse/clustered track outputs look jagged as stairs.
        ax.plot(
            sorted_err, cdf * 100,
            linewidth=style['linewidth'],
            color=style['color'],
            linestyle=style['linestyle'],
            label=label,
            alpha=0.9,
        )
        print(
            f'  {label}: N={len(errors)}, '
            f'median={np.percentile(errors, 50):.2f}°, '
            f'90th={np.percentile(errors, 90):.2f}°'
        )

    ax.axhline(y=50, color='gray', linestyle=':', linewidth=1.0, alpha=0.4)
    ax.axhline(y=90, color='gray', linestyle=':', linewidth=1.0, alpha=0.4)
    ax.set_xlabel('Absolute Error (degrees)', fontsize=15)
    ax.set_ylabel('Cumulative Probability (%)', fontsize=15)
    ax.legend(fontsize=11, loc='lower right', framealpha=0.95, frameon=True, borderaxespad=0)
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax.set_ylim(0, 100)
    ax.set_xlim(0, 25)
    ax.tick_params(labelsize=14)

    leg = ax.get_legend()
    if leg:
        leg.set_zorder(0)
    for spine in ax.spines.values():
        spine.set_zorder(1)

    plt.tight_layout()

    os.makedirs(output_dir, exist_ok=True)
    out_pdf = os.path.join(output_dir, f'{output_basename}.pdf')
    out_png = os.path.join(output_dir, f'{output_basename}.png')
    fig.savefig(out_pdf, bbox_inches='tight', format='pdf')
    fig.savefig(out_png, dpi=300, bbox_inches='tight')
    plt.close(fig)

    print(f'Saved: {out_pdf}')
    print(f'Saved: {out_png}')
    return out_pdf


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('--data-dir', default=here)
    ap.add_argument('--output-dir', default=here)
    ap.add_argument('--basename', default='figure10c')
    ap.add_argument('--suffix', default='', help="CSV name suffix, e.g. '_baseline' or '_track'")
    ap.add_argument('--filter', action='store_true', help='enable old z-score/intersection filter')
    ap.add_argument('--reprocess', action='store_true', help='force CIR→AoA recompute')
    args = ap.parse_args()
    plot_figure10c(
        data_dir=args.data_dir,
        output_dir=args.output_dir,
        output_basename=args.basename,
        apply_filter=args.filter,
        file_suffix=args.suffix,
        reprocess=args.reprocess,
    )
