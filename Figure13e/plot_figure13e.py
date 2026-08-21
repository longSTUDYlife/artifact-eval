#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 13(e): Env1 sensing trajectory (Measured / True Path / Array).
Printed median / 90th (6.4 / 17.4 cm) are Fig. 13(f).

CIR (curve_raw_npy/Figure13e or Figure10c .../raw.npz)
  → process_from_cir.py (seq-sync + angle-FFT RA, min_range=2.05 m)
  → aoa_estimates/aoa_estimates_8port_{angle}.csv
"""

from __future__ import annotations

import argparse
import glob
import os
import re
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

ANGLES = [-40, -30, -20, -10, 0, 10, 20, 30, 40]
MIN_RANGE_M = 2.05


def _angle_to_xy(angle_deg, distance):
    angle_std = np.radians(90 - angle_deg)
    return distance * np.cos(angle_std), distance * np.sin(angle_std)


def _extract_angle_from_filename(filename):
    m = re.search(r'(\d+)port_(-?\d+)\.csv$', filename)
    return int(m.group(2)) if m else None


def _aoa_dir(data_dir):
    nested = os.path.join(data_dir, 'aoa_estimates')
    if os.path.isdir(nested) and glob.glob(os.path.join(nested, 'aoa_estimates_8port_*.csv')):
        return nested
    return data_dir


def track_error(est_aoa_deg, range_m, true_angle_deg):
    x, y = _angle_to_xy(est_aoa_deg, range_m)
    std = np.radians(90 - true_angle_deg)
    return np.abs(x * np.sin(std) - y * np.cos(std))


def ensure_processed(data_dir, force=False):
    fig_dir = data_dir
    est_dir = os.path.join(data_dir, 'aoa_estimates')
    if os.path.basename(os.path.abspath(data_dir)) == 'aoa_estimates':
        est_dir = data_dir
        fig_dir = os.path.dirname(data_dir)
    from_cir = os.environ.get(
        'FIGURE13E_FROM_CIR', os.environ.get('FIGURE12A_FROM_CIR', '1')
    ).lower() in ('1', 'true', 'yes')
    needed = [os.path.join(est_dir, f'aoa_estimates_8port_{ang}.csv') for ang in ANGLES]
    if force or from_cir or not all(os.path.isfile(f) for f in needed):
        from process_from_cir import run_batch
        print('Running Python CIR→RA track pipeline (process_from_cir) ...')
        run_batch(Path(fig_dir), force=True)
    return est_dir


def load_track_errors(data_dir, min_range_m=MIN_RANGE_M):
    est_dir = _aoa_dir(data_dir)
    csv_files = sorted(glob.glob(os.path.join(est_dir, 'aoa_estimates_8port_*.csv')))
    if not csv_files:
        raise FileNotFoundError(
            f'No aoa_estimates_8port_*.csv in {est_dir}. Run process_from_cir.py first.'
        )
    errs = []
    ranges = []
    for path in csv_files:
        true_angle = _extract_angle_from_filename(os.path.basename(path))
        if true_angle is None:
            continue
        df = pd.read_csv(path)
        aoa = pd.to_numeric(df['estimated_aoa'], errors='coerce').to_numpy()
        rng = pd.to_numeric(df['range'], errors='coerce').to_numpy()
        m = np.isfinite(aoa) & np.isfinite(rng) & (rng >= min_range_m)
        errs.append(track_error(aoa[m], rng[m], true_angle))
        ranges.append(rng[m])
    err = np.concatenate(errs) if errs else np.array([])
    rng_all = np.concatenate(ranges) if ranges else np.array([])
    return est_dir, err, rng_all


def draw_panel_a(ax, data_dir, margin_scale=1.0):
    est_dir = _aoa_dir(data_dir)
    csv_files = sorted(glob.glob(os.path.join(est_dir, 'aoa_estimates_8port_*.csv')))
    if not csv_files:
        ax.text(0.5, 0.5, 'No data', ha='center', va='center', transform=ax.transAxes)
        return

    angles_list = sorted(
        a for a in (_extract_angle_from_filename(os.path.basename(f)) for f in csv_files) if a is not None
    )
    colors = plt.cm.viridis(np.linspace(0, 1, max(len(angles_list), 1)))
    angle_color_map = dict(zip(angles_list, colors))

    min_angle, max_angle = -40, 40
    max_range = 5.0
    first_measured = True

    for filepath in csv_files:
        true_angle = _extract_angle_from_filename(os.path.basename(filepath))
        if true_angle is None:
            continue
        data = pd.read_csv(filepath)
        if 'estimated_aoa' not in data.columns or 'range' not in data.columns:
            continue
        data = data[['estimated_aoa', 'range']].copy()
        data['estimated_aoa'] = pd.to_numeric(data['estimated_aoa'], errors='coerce')
        data['range'] = pd.to_numeric(data['range'], errors='coerce')
        data = data.dropna()
        data = data[data['range'] >= MIN_RANGE_M]
        if len(data) == 0:
            continue
        ranges = data['range'].to_numpy()
        coords = [_angle_to_xy(ang, r) for ang, r in zip(data['estimated_aoa'].to_numpy(), ranges)]
        x = np.array([c[0] for c in coords])
        y = np.array([c[1] for c in coords])
        max_range = max(max_range, float(np.max(ranges)) * 1.1)
        color = angle_color_map.get(true_angle, 'gray')
        label = 'Measured' if first_measured else ''
        first_measured = False
        ax.plot(x, y, 'o-', linewidth=1.2, markersize=2.5, color=color, alpha=0.7,
                label=label, zorder=4)

    min_range = 0.5
    angle_range_deg = np.linspace(min_angle, max_angle, 100)
    sector_xy = np.array([_angle_to_xy(a, max_range) for a in angle_range_deg])
    ax.fill(
        np.r_[0, sector_xy[:, 0], 0], np.r_[0, sector_xy[:, 1], 0],
        color=[0.95, 0.95, 0.95], edgecolor=[0.7, 0.7, 0.7], linewidth=0.5,
        linestyle='--', alpha=0.3, zorder=1,
    )
    for ang in ANGLES:
        xe, ye = _angle_to_xy(ang, max_range)
        ax.plot([0, xe], [0, ye], color=[0.7, 0.7, 0.7], linewidth=0.5,
                linestyle='--', zorder=2)
    arc_xy = np.array([_angle_to_xy(a, max_range) for a in np.linspace(min_angle, max_angle, 200)])
    ax.plot(arc_xy[:, 0], arc_xy[:, 1], 'k-', linewidth=0.5, zorder=2)
    for dist in [1, 2, 3, 4, 5]:
        if dist <= max_range:
            da = np.linspace(min_angle, max_angle, 50)
            dx = np.array([_angle_to_xy(a, dist) for a in da])
            ax.plot(dx[:, 0], dx[:, 1], 'k:', linewidth=0.3, zorder=2)
    for ang in ANGLES:
        lx, ly = _angle_to_xy(ang, max_range * 1.04)
        ha = 'right' if ang < 0 else ('left' if ang > 0 else 'center')
        va = 'center' if ang != 0 else 'bottom'
        ax.text(lx, ly, f'{ang}°', ha=ha, va=va, fontsize=14, fontweight='bold', zorder=3)
    for dist in [1, 2, 3, 4, 5]:
        if dist <= max_range:
            lx, ly = _angle_to_xy(42, dist)
            ax.text(lx, ly, f'{dist}m', ha='left', va='center', fontsize=14,
                    fontweight='bold', zorder=3)

    for true_angle in np.arange(-40, 41, 10):
        r_range = np.linspace(min_range, max_range, 100)
        coords_true = [_angle_to_xy(true_angle, r) for r in r_range]
        x_true = np.array([c[0] for c in coords_true])
        y_true = np.array([c[1] for c in coords_true])
        ax.plot(x_true, y_true, '--', linewidth=1.5, color='red', alpha=0.6,
                label='True Path' if true_angle == -40 else '', zorder=3)

    ant_x = np.linspace(-0.35, 0.35, 8)
    ant_handle = ax.scatter(
        ant_x, np.zeros(8), s=25, color='#3333CC', marker='s', edgecolors='k',
        linewidths=0.5, label='ANT', zorder=7, alpha=0.9,
    )

    fan_x_min = max_range * np.cos(np.radians(130))
    fan_x_max = max_range * np.cos(np.radians(50))
    fan_y_max = max_range * np.sin(np.radians(90))
    x_margin = (fan_x_max - fan_x_min) * 0.1 * margin_scale
    y_margin = fan_y_max * 0.1 * margin_scale
    ax.set_xlim(min(fan_x_min - x_margin, -0.2), fan_x_max + x_margin)
    ax.set_ylim(-0.2, fan_y_max + y_margin)
    ax.set_xlabel('')
    ax.set_ylabel('')
    ax.set_xticks([])
    ax.set_yticks([])
    for side in ('top', 'right', 'bottom', 'left'):
        ax.spines[side].set_visible(False)
    handles, labels = ax.get_legend_handles_labels()
    leg_handles, leg_labels = [], []
    for h, lb in zip(handles, labels):
        if lb == 'Measured':
            leg_handles.append(h)
            leg_labels.append('Measured')
            break
    if 'True Path' in labels:
        idx = labels.index('True Path')
        leg_handles.append(handles[idx])
        leg_labels.append('True Path')
    leg_handles.append(ant_handle)
    leg_labels.append('Array')
    ax.legend(leg_handles, leg_labels, fontsize=11, loc='lower right',
              framealpha=0.95, frameon=True, borderaxespad=0)
    ax.set_aspect('equal')


def plot_figure13e(data_dir=None, output_basename='figure13e', reprocess=False):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))
    ensure_processed(data_dir, force=reprocess)

    est_dir, err, rng = load_track_errors(data_dir)
    n = int(err.size)
    med = float(np.median(err))
    p90 = float(np.percentile(err, 90))
    rmse = float(np.sqrt(np.mean(err ** 2)))
    print(
        f'N = {n}, median = {med:.3f} m, 90th = {p90:.3f} m, RMSE = {rmse:.3f} m '
        f'(Fig. 13(f) numbers)'
    )

    out_err = os.path.join(data_dir, 'track_errors_8port.csv')
    pd.DataFrame({'Track_Error': err, 'Range': rng}).to_csv(out_err, index=False)
    print(f'Saved errors: {out_err}')
    print(f'Trajectory CSVs: {est_dir}')

    fig, ax = plt.subplots(figsize=(5.0, 4.5))
    draw_panel_a(ax, data_dir)
    leg = ax.get_legend()
    if leg:
        leg.set_zorder(0)
    for spine in ax.spines.values():
        spine.set_zorder(1)

    plt.tight_layout()
    out_pdf = os.path.join(data_dir, f'{output_basename}.pdf')
    out_png = os.path.join(data_dir, f'{output_basename}.png')
    fig.savefig(out_pdf, bbox_inches='tight', format='pdf')
    fig.savefig(out_png, dpi=300, bbox_inches='tight')
    plt.close(fig)
    print(f'Saved: {out_pdf}')
    print(f'Saved: {out_png}')
    return out_pdf


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--reprocess', action='store_true')
    args = parser.parse_args()
    plot_figure13e(reprocess=args.reprocess)
