#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 10(a)(b): Localization angular accuracy — CDF + per-angle bars.

Same raw data / MATLAB pipeline:
  raw/antenna_data_port*_8ports_concurrent_localization_aoa_accuracy_*.csv
  + pdoa_data_{0,10,20,30,40}d.csv (DW3000)

  MATLAB (MVDR; no IQR; no unify) -> frame_errors_* + angle_errors_*
  build_pdoa_error_csv.py -> pdoa_error_data.csv
"""

from __future__ import annotations

import os
import subprocess
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib import rcParams

from build_pdoa_error_csv import build_pdoa_error_csv

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
STYLE_DW3000 = {'color': '#6A994E', 'linestyle': ':', 'linewidth': 1.5}

MATLAB_BIN = os.environ.get(
    'MATLAB_BIN', '/Applications/MATLAB_R2025a.app/bin/matlab'
)


def ensure_ula_processed(data_dir, force=False):
    needed = [
        'frame_errors_8port_filtered.csv',
        'frame_errors_4port_filtered.csv',
        'frame_errors_2port_filtered.csv',
        'angle_errors_8port_filtered.csv',
        'angle_errors_4port_filtered.csv',
        'angle_errors_2port_filtered.csv',
    ]
    from_cir = os.environ.get('FIGURE10AB_FROM_CIR', '1').lower() in ('1', 'true', 'yes')
    if force or from_cir or not all(os.path.isfile(os.path.join(data_dir, f)) for f in needed):
        # Prefer pure-Python CIR → MVDR pipeline (Docker / no MATLAB)
        try:
            from process_from_cir import run_batch
            print('Running Python CIR→MVDR pipeline (process_from_cir) ...')
            run_batch(data_dir, force=True)
        except Exception as py_exc:
            print(f'Python CIR pipeline failed ({py_exc}); trying MATLAB fallback...')
            if not os.path.isfile(MATLAB_BIN):
                raise FileNotFoundError(
                    f'CIR→AoA failed in Python and MATLAB not found at {MATLAB_BIN}. '
                    f'Original error: {py_exc}'
                ) from py_exc
            data_dir_m = data_dir.replace("'", "''")
            proc = subprocess.run(
                [MATLAB_BIN, '-batch', f"cd('{data_dir_m}'); test_all_angles_multi_config;"],
                cwd=data_dir,
                capture_output=True,
                text=True,
                check=False,
            )
            out = (proc.stdout or '') + (proc.stderr or '')
            print(out[-4000:] if len(out) > 4000 else out)
            if proc.returncode != 0:
                raise RuntimeError(f'MATLAB failed (exit {proc.returncode})') from py_exc
    for f in needed:
        path = os.path.join(data_dir, f)
        if not os.path.isfile(path):
            raise FileNotFoundError(f'Missing after MATLAB run: {path}')


def load_cdf_errors(data_dir):
    files = [
        ('frame_errors_8port_filtered.csv', '8RX-ULA', STYLE_8),
        ('frame_errors_4port_filtered.csv', '4RX-ULA', STYLE_4),
        ('frame_errors_2port_filtered.csv', '2RX-ULA', STYLE_2),
        ('pdoa_error_data.csv', '2-antenna DW3000', STYLE_DW3000),
    ]
    configs = []
    for fname, label, style in files:
        path = os.path.join(data_dir, fname)
        if not os.path.exists(path):
            print(f'Missing: {path}')
            continue
        df = pd.read_csv(path)
        col = 'Absolute_Error' if 'Absolute_Error' in df.columns else df.columns[0]
        errors = df[col].to_numpy(dtype=float)
        errors = errors[np.isfinite(errors)]
        configs.append((errors, label, style))
        print(f'Loaded CDF {label}: N={len(errors)}')
    return configs


def merge_symmetric_angles(angles, mean_errors, std_errors):
    abs_angles = np.abs(angles)
    unique_angles = np.unique(abs_angles)
    merged_means, merged_stds = [], []
    for angle in unique_angles:
        mask = abs_angles == angle
        angle_mean_errors = mean_errors[mask]
        angle_std_errors = std_errors[mask]
        merged_means.append(np.mean(angle_mean_errors))
        if len(angle_mean_errors) > 1:
            mean_of_stds_sq = np.mean(angle_std_errors ** 2)
            var_of_means = np.var(angle_mean_errors)
            merged_stds.append(np.sqrt(mean_of_stds_sq + var_of_means))
        else:
            merged_stds.append(angle_std_errors[0])
    return unique_angles, np.array(merged_means), np.array(merged_stds)


def load_angle_errors_filtered(data_dir):
    configs = []
    for port, label, color in [
        (8, '8RX-ULA', STYLE_8['color']),
        (4, '4RX-ULA', STYLE_4['color']),
        (2, '2RX-ULA', STYLE_2['color']),
    ]:
        path = os.path.join(data_dir, f'angle_errors_{port}port_filtered.csv')
        if not os.path.exists(path):
            continue
        df = pd.read_csv(path)
        angles = df['True_Angle'].values
        mean_errors = np.abs(df['Mean_Error'].values)
        std_errors = df['Std_Error'].values
        merged = merge_symmetric_angles(angles, mean_errors, std_errors)
        configs.append((*merged, label, color))
        print(f'Loaded bars {label}: {len(merged[0])} abs-angles')
    return configs


def load_dw3000_angle_errors(data_dir, display_angles=(0, 10, 20, 30, 40)):
    means, stds = [], []
    for ang in display_angles:
        path = os.path.join(data_dir, f'pdoa_data_{ang}d.csv')
        if not os.path.exists(path):
            return None
        df = pd.read_csv(path)
        if 'angle_deg' not in df.columns:
            return None
        abs_err = np.abs(df['angle_deg'].to_numpy(dtype=float) - float(ang))
        abs_err = abs_err[np.isfinite(abs_err)]
        if abs_err.size == 0:
            return None
        means.append(np.mean(abs_err))
        stds.append(np.std(abs_err, ddof=1) if abs_err.size > 1 else 0.0)
    return (
        np.asarray(display_angles, dtype=float),
        np.asarray(means, dtype=float),
        np.asarray(stds, dtype=float),
    )


def draw_panel_a(ax, data_dir):
    configs = load_cdf_errors(data_dir)
    if not configs:
        raise FileNotFoundError(f'No CDF data in {data_dir}')
    for errors, label, style in configs:
        sorted_err = np.sort(errors)
        cdf = np.arange(1, len(sorted_err) + 1) / len(sorted_err)
        ax.plot(
            sorted_err, cdf * 100,
            linewidth=style['linewidth'],
            color=style['color'],
            linestyle=style['linestyle'],
            label=label,
            alpha=0.9,
            drawstyle='steps-post',
        )
        print(
            f"  {label}: median={np.percentile(errors, 50):.2f}°, "
            f"90th={np.percentile(errors, 90):.2f}°"
        )
    ax.axhline(y=50, color='gray', linestyle=':', linewidth=1.0, alpha=0.4)
    ax.axhline(y=90, color='gray', linestyle=':', linewidth=1.0, alpha=0.4)
    ax.set_xlabel('Absolute Error (degrees)', fontsize=15)
    ax.set_ylabel('Cumulative Probability (%)', fontsize=15)
    ax.legend(fontsize=11, loc='lower right', framealpha=0.95, frameon=True, borderaxespad=0)
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax.set_ylim(0, 100)
    ax.set_xlim(0, 20)
    ax.tick_params(labelsize=14)


def draw_panel_b(ax, data_dir):
    configs = load_angle_errors_filtered(data_dir)
    dw = load_dw3000_angle_errors(data_dir)
    if dw is not None:
        configs.append((dw[0], dw[1], dw[2], '2-antenna DW3000', STYLE_DW3000['color']))
        print('Loaded bars 2-antenna DW3000')
    if not configs:
        ax.text(0.5, 0.5, 'No angle error data', ha='center', va='center', transform=ax.transAxes)
        return

    n_angles = 5
    n_configs = len(configs)
    bar_width = 0.6 / n_configs
    x_positions = np.arange(n_angles) * 0.735
    disp = [0.0, 10.0, 20.0, 30.0, 40.0]
    for i, (angles, means, stds, label, color) in enumerate(configs):
        angle_to_mean = {float(a): m for a, m in zip(angles, means)}
        angle_to_std = {float(a): s for a, s in zip(angles, stds)}
        means_plot = np.array([angle_to_mean.get(a, np.nan) for a in disp])
        stds_plot = np.array([angle_to_std.get(a, 0.0) for a in disp])
        offset = (i - (n_configs - 1) / 2) * bar_width
        ax.bar(
            x_positions + offset, means_plot, bar_width,
            yerr=stds_plot, color=color, alpha=0.8,
            edgecolor='white', linewidth=1.5, label=label, capsize=3,
            error_kw={'elinewidth': 1.5, 'alpha': 0.8, 'capthick': 1.5},
        )
    ax.set_xticks(x_positions)
    ax.set_xticklabels(['0°', '10°', '20°', '30°', '40°'])
    bar_group_width = bar_width * n_configs
    ax.set_xlim(
        left=x_positions[0] - bar_group_width * 0.5,
        right=x_positions[-1] + bar_group_width * 0.5,
    )
    ax.set_xlabel('True Angle (degrees)', fontsize=15)
    ax.set_ylabel('Absolute Error (degrees)', fontsize=15)
    ax.legend(fontsize=11, loc='upper left', framealpha=0.95, frameon=True, borderaxespad=0)
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5, axis='y')
    ax.set_ylim(bottom=0)
    ax.tick_params(labelsize=14)


def plot_figure10ab(data_dir=None, output_basename='figure10ab', reprocess=False):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))

    print('Building DW3000 Absolute_Error from pdoa_data_*d.csv ...')
    build_pdoa_error_csv(data_dir)
    ensure_ula_processed(data_dir, force=reprocess)

    # Two panels of the combined 1x4 figure (~5.0 wide each)
    fig, axes = plt.subplots(1, 2, figsize=(10.0, 4.5))
    draw_panel_a(axes[0], data_dir)
    draw_panel_b(axes[1], data_dir)

    for ax in axes:
        leg = ax.get_legend()
        if leg:
            leg.set_zorder(0)
        for spine in ax.spines.values():
            spine.set_zorder(1)

    titles = [
        '(a) Angular accuracy (Localization)',
        '(b) Angular accuracy vs. angle of arrivals',
    ]
    for ax, title in zip(axes, titles):
        ax.text(0.5, -0.28, title, transform=ax.transAxes, fontsize=16, ha='center', va='top')

    plt.tight_layout(rect=[0, 0.10, 1, 1])

    out_pdf = os.path.join(data_dir, f'{output_basename}.pdf')
    out_png = os.path.join(data_dir, f'{output_basename}.png')
    fig.savefig(out_pdf, bbox_inches='tight', format='pdf')
    fig.savefig(out_png, dpi=300, bbox_inches='tight')
    plt.close(fig)

    print(f'Saved: {out_pdf}')
    print(f'Saved: {out_png}')
    return out_pdf


if __name__ == '__main__':
    reprocess = '--reprocess' in sys.argv
    plot_figure10ab(reprocess=reprocess)
