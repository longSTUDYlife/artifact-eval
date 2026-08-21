#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 10(d): Angular resolution vs. array size — standalone.

CIR (curve_raw_npy/Figure10d/.../raw.npz or raw/*.csv)
  → process_from_cir.py (row-align + angle-FFT RA slice)
  → angle_amplitude_{2,4,8}port.csv
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


def ensure_processed(data_dir, force=False):
    from_cir = os.environ.get('FIGURE10D_FROM_CIR', '1').lower() in ('1', 'true', 'yes')
    needed = [
        os.path.join(data_dir, f'angle_amplitude_{n}port.csv') for n in (2, 4, 8)
    ]
    if force or from_cir or not all(os.path.isfile(f) for f in needed):
        from process_from_cir import run_batch
        print('Running Python CIR→RA slice pipeline (process_from_cir) ...')
        run_batch(Path(data_dir), force=True)


def load_angle_amplitude_data(data_dir):
    out = {}
    for port, name in [(2, '2port'), (4, '4port'), (8, '8port')]:
        path = os.path.join(data_dir, f'angle_amplitude_{name}.csv')
        if not os.path.exists(path):
            print(f'  missing: {path}')
            continue
        df = pd.read_csv(path)
        angle_col = next((c for c in df.columns if 'angle' in c.lower()), None)
        amp_col = next((c for c in df.columns if 'amplitude' in c.lower()), None)
        if angle_col is None or amp_col is None:
            print(f'  bad columns in {name}: {list(df.columns)}')
            continue
        out[port] = (df[angle_col].to_numpy(), df[amp_col].to_numpy())
        print(
            f'  {name}: N={len(df)}, '
            f'frame={df["Frame"].iloc[0] if "Frame" in df.columns else "?"}, '
            f'range={df["Range_m"].iloc[0] if "Range_m" in df.columns else "?"}'
        )
    return out


def plot_figure10d(data_dir=None, output_basename='figure10d', output_dir=None, reprocess=False):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))
    if output_dir is None:
        output_dir = data_dir

    ensure_processed(data_dir, force=reprocess)

    print(f'Loading angle-amplitude CSVs from {data_dir} ...')
    data = load_angle_amplitude_data(data_dir)
    if not data:
        raise FileNotFoundError(f'No angle_amplitude CSVs in {data_dir}')

    configs = []
    if 2 in data:
        configs.append((data[2][0], data[2][1], '2RX-ULA', STYLE_2))
    if 4 in data:
        configs.append((data[4][0], data[4][1], '4RX-ULA', STYLE_4))
    if 8 in data:
        configs.append((data[8][0], data[8][1], '8RX-ULA', STYLE_8))

    fig, ax = plt.subplots(figsize=(5.0, 4.5))
    for x, y, label, style in configs:
        ax.plot(
            x, y,
            linewidth=style['linewidth'],
            color=style['color'],
            linestyle=style['linestyle'],
            label=label,
            alpha=0.9,
        )

    ax.set_xlabel('Angle (degrees)', fontsize=15)
    ax.set_ylabel('Normalized Amplitude', fontsize=15)
    ax.legend(fontsize=11, loc='upper right', framealpha=0.95, frameon=True, borderaxespad=0)
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax.set_xlim(-40, 40)
    y_max = max(np.max(c[1]) for c in configs)
    ax.set_ylim(0, y_max * 1.1)
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
    ap.add_argument('--basename', default='figure10d')
    ap.add_argument('--reprocess', action='store_true')
    args = ap.parse_args()
    plot_figure10d(
        data_dir=args.data_dir,
        output_dir=args.output_dir,
        output_basename=args.basename,
        reprocess=args.reprocess,
    )
