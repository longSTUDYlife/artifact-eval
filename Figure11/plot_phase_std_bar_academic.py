#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Academic-style bar plot of pooled phase-std across systems.

Automatically computes std from:
  sensing / localization / uloc / DWM1002
then draws the same bar chart as before.

Usage:
  python3 plot_phase_std_bar_academic.py
  python3 plot_phase_std_bar_academic.py --recompute-sensing
"""

import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import rcParams

from compute_phase_stds import compute_all, values_for_plot

# Same font/layout as Figures/Done/Figure12/plot_dynamic_range_academic.py
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

LABELS = ['sensing', 'localization', 'uloc', 'dw3000']
LABELS_DISP = ['S.', 'L.', 'ULoc', 'DW3000']

# Colors consistent with other academic figures in this repo
COLORS = ['#5B4E77', '#CD853F', '#2E8B57', '#4A90A4']

FIGSIZE = (3.5, 2.8)
SUBPLOT_RECT = dict(left=0.20, right=0.97, bottom=0.20, top=0.95)


def plot_phase_std_bar(
    data_dir=None,
    output_filename='figure11.pdf',
    values=None,
):
    if data_dir is None:
        data_dir = os.path.dirname(os.path.abspath(__file__))

    if values is None:
        print("Computing phase std from sensing / localization / uloc / DWM1002 ...")
        results = compute_all()
        values = values_for_plot(results, digits=3)
    else:
        values = list(values)

    fig, ax = plt.subplots(figsize=FIGSIZE)

    x = list(range(len(LABELS)))
    bars = ax.bar(
        x, values,
        width=0.65,
        color=COLORS,
        edgecolor='black',
        linewidth=0.8,
        zorder=3,
    )
    for bar, val in zip(bars, values):
        ax.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height(),
            f'{val:.3f}',
            ha='center', va='bottom',
            fontsize=10, fontweight='bold', fontfamily='serif',
            zorder=4,
        )

    ax.set_xticks(x)
    ax.set_xticklabels(LABELS_DISP)
    ax.set_ylabel(r'STD (rad)', fontsize=15, fontweight='bold')
    ax.set_xlabel('')
    ax.set_xlim(-0.5, len(LABELS) - 0.5)

    ax.set_ylim([0, max(values) * 1.25])
    ax.grid(True, axis='y', alpha=0.3, linestyle='--', linewidth=0.5, zorder=0)
    ax.set_axisbelow(True)

    ax.tick_params(labelsize=14)
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontweight('bold')
        label.set_fontfamily('serif')
    for label in ax.get_xticklabels():
        label.set_ha('center')
        label.set_rotation(0)

    for spine in ['top', 'right']:
        ax.spines[spine].set_visible(False)

    fig.subplots_adjust(**SUBPLOT_RECT)

    output_path = os.path.join(data_dir, output_filename)
    plt.savefig(output_path, format='pdf')
    print(f"Academic-style plot saved to: {output_path}")

    png_path = os.path.splitext(output_path)[0] + '.png'
    plt.savefig(png_path, format='png', dpi=300)
    print(f"PNG preview saved to: {png_path}")
    plt.close(fig)

    print("\nValues:")
    for lab, val in zip(LABELS, values):
        print(f"  {lab:<14} {val:.3f} rad")

    return output_path, values


if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print("=" * 80)
    print("Academic-Style Phase Std Bar Plot (auto-computed)")
    print("=" * 80)
    print()

    output_file, _ = plot_phase_std_bar(data_dir=script_dir)

    print("\n" + "=" * 80)
    if output_file:
        print("Phase std bar plot generation completed!")
    else:
        print("Error: Failed to generate plot")
    print("=" * 80)
