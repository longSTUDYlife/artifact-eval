#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 13(a): Env1 localization scatter (EST / GRD / Array).

CIR (curve_raw_npy/Figure13a/.../raw.npz or raw/*.csv + lde_cache)
  → process_from_cir.py (seq-align + MVDR + distance correction)
  → localization_errors_8port.csv + localization_scatter_data.csv
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


def _angle_to_xy(angle_deg, distance):
    angle_std = np.radians(90 - angle_deg)
    return distance * np.cos(angle_std), distance * np.sin(angle_std)


def load_env1_errors(data_dir):
    path = os.path.join(data_dir, 'localization_errors_8port.csv')
    if not os.path.isfile(path):
        raise FileNotFoundError(
            f'{path} missing. Run process_from_cir.py or MATLAB run_figure13a.m first.'
        )
    df = pd.read_csv(path)
    col = 'Localization_Error' if 'Localization_Error' in df.columns else df.columns[0]
    err = df[col].to_numpy(dtype=float)
    err = err[np.isfinite(err)]
    return path, err


def draw_panel_a(ax, data_dir, margin_scale=1.0):
    path = os.path.join(data_dir, 'localization_scatter_data.csv')
    if not os.path.exists(path):
        raise FileNotFoundError(
            f'{path} missing. Run process_from_cir.py or MATLAB run_figure13a.m first.'
        )
    df = pd.read_csv(path)
    gnd_data = df[df['type'] == 1]
    est_data = df[df['type'] == 0]

    def to_xy(angles, distances):
        return np.array([_angle_to_xy(a, d) for a, d in zip(angles, distances)])

    gnd_x, gnd_y = np.array([]), np.array([])
    est_x, est_y = np.array([]), np.array([])
    gnd_xy = est_xy = None
    if len(gnd_data) > 0:
        gnd_xy = to_xy(gnd_data['angle'].values, gnd_data['distance'].values)
        gnd_x, gnd_y = gnd_xy[:, 0], gnd_xy[:, 1]
    if len(est_data) > 0:
        est_xy = to_xy(est_data['angle'].values, est_data['distance'].values)
        est_x, est_y = est_xy[:, 0], est_xy[:, 1]

    min_angle, max_angle = -40, 40
    max_dist = 5.0
    if gnd_xy is not None or est_xy is not None:
        all_xy = (
            np.vstack([gnd_xy, est_xy])
            if (gnd_xy is not None and est_xy is not None)
            else (gnd_xy if gnd_xy is not None else est_xy)
        )
        max_dist = max(max_dist, np.max(np.linalg.norm(all_xy, axis=1)) * 1.1)

    angle_range = np.linspace(min_angle, max_angle, 100)
    sector_xy = np.array([_angle_to_xy(a, max_dist) for a in angle_range])
    ax.fill(
        np.r_[0, sector_xy[:, 0], 0], np.r_[0, sector_xy[:, 1], 0],
        color=[0.95, 0.95, 0.95], edgecolor=[0.7, 0.7, 0.7], linewidth=0.5,
        linestyle='--', alpha=0.3, zorder=1,
    )
    for ang in [-40, -30, -20, -10, 0, 10, 20, 30, 40]:
        xe, ye = _angle_to_xy(ang, max_dist)
        ax.plot([0, xe], [0, ye], color=[0.7, 0.7, 0.7], linewidth=0.5,
                linestyle='--', zorder=2)
    arc_xy = np.array([_angle_to_xy(a, max_dist) for a in np.linspace(min_angle, max_angle, 200)])
    ax.plot(arc_xy[:, 0], arc_xy[:, 1], 'k-', linewidth=0.5, zorder=2)
    for dist in [1, 2, 3, 4, 5]:
        if dist <= max_dist:
            da = np.linspace(min_angle, max_angle, 50)
            dx = np.array([_angle_to_xy(a, dist) for a in da])
            ax.plot(dx[:, 0], dx[:, 1], 'k:', linewidth=0.3, zorder=2)

    for ang in [-40, -30, -20, -10, 0, 10, 20, 30, 40]:
        lx, ly = _angle_to_xy(ang, max_dist * 1.04)
        ha = 'right' if ang < 0 else ('left' if ang > 0 else 'center')
        va = 'center' if ang != 0 else 'bottom'
        ax.text(lx, ly, f'{ang}°', ha=ha, va=va, fontsize=14, fontweight='bold', zorder=3)
    for dist in [1, 2, 3, 4, 5]:
        if dist <= max_dist:
            lx, ly = _angle_to_xy(42, dist)
            ax.text(lx, ly, f'{dist}m', ha='left', va='center', fontsize=14,
                    fontweight='bold', zorder=3)

    est_handle = None
    if len(est_x) > 0:
        est_handle = ax.scatter(
            est_x, est_y, s=8, c='#E91E1E', marker='^', edgecolors='none',
            label='EST', alpha=0.8, zorder=6,
        )
    gnd_handle = None
    if len(gnd_x) > 0:
        gnd_handle = ax.scatter(
            gnd_x, gnd_y, s=5, c='#2E86AB', marker='s', edgecolors='#2E86AB',
            linewidths=0.3, label='GRD', zorder=4, alpha=0.9,
        )
    ant_x = np.linspace(-0.35, 0.35, 8)
    ant_handle = ax.scatter(
        ant_x, np.zeros(8), s=25, color='#3333CC', marker='s', edgecolors='k',
        linewidths=0.5, label='UWB', zorder=7, alpha=0.9,
    )

    if len(est_x) > 0 or len(gnd_x) > 0:
        all_x = np.concatenate([est_x, gnd_x]) if len(est_x) and len(gnd_x) else (
            est_x if len(est_x) else gnd_x)
        all_y = np.concatenate([est_y, gnd_y]) if len(est_x) and len(gnd_y) else (
            est_y if len(est_y) else gnd_y)
        data_span = max(np.ptp(all_x), np.ptp(all_y), 1.0)
        margin = 0.28 * data_span * margin_scale
        ax.set_xlim(np.min(all_x) - margin, np.max(all_x) + margin)
        ax.set_ylim(np.min(all_y) - margin, np.max(all_y) + margin)
    else:
        ax.set_xlim(-0.5, max_dist + 0.5)
        ax.set_ylim(-max_dist * 0.6, max_dist * 0.6)

    ax.set_xlabel('')
    ax.set_ylabel('')
    ax.set_xticks([])
    ax.set_yticks([])
    for side in ('top', 'right', 'bottom', 'left'):
        ax.spines[side].set_visible(False)
    hands, labs = [], []
    if est_handle:
        hands.append(est_handle); labs.append('EST')
    if gnd_handle:
        hands.append(gnd_handle); labs.append('GRD')
    if ant_handle:
        hands.append(ant_handle); labs.append('Array')
    if hands:
        ax.legend(hands, labs, fontsize=11, loc='lower right',
                  framealpha=0.95, frameon=True, borderaxespad=0)
    ax.set_aspect('equal')


def ensure_processed(data_dir, force=False):
    fig_dir = data_dir
    from_cir = os.environ.get(
        'FIGURE13A_FROM_CIR', os.environ.get('FIGURE11A_FROM_CIR', '1')
    ).lower() in ('1', 'true', 'yes')
    needed = [
        os.path.join(fig_dir, 'localization_errors_8port.csv'),
        os.path.join(fig_dir, 'localization_scatter_data.csv'),
    ]
    if force or from_cir or not all(os.path.isfile(f) for f in needed):
        from process_from_cir import run_batch
        print('Running Python CIR→MVDR localization (process_from_cir) ...')
        run_batch(Path(fig_dir), force=True)
    return fig_dir


def plot_figure13a(data_dir=None, output_basename='figure13a', reprocess=False):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))
    ensure_processed(data_dir, force=reprocess)

    src, err = load_env1_errors(data_dir)
    n = int(err.size)
    med = float(np.median(err))
    p90 = float(np.percentile(err, 90))
    rmse = float(np.sqrt(np.mean(err ** 2)))
    print(
        f'N = {n}, median = {med:.3f} m, 90th = {p90:.3f} m, RMSE = {rmse:.3f} m'
    )
    print(f'Errors: {src}')
    print(f'Scatter: {os.path.join(data_dir, "localization_scatter_data.csv")}')

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
    plot_figure13a(reprocess=args.reprocess)
