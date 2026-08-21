#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 14(d): HAR test confusion matrices (RD / RD+RA / RD+RA+RE).

Default: load packed cms/*.npy (file-split test CMs).
After retraining: python plot_figure14d.py --from_runs runs
"""

from __future__ import annotations

import argparse
import glob
import json
import os

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import rcParams
from matplotlib.gridspec import GridSpec
from mpl_toolkits.axes_grid1 import make_axes_locatable

rcParams['font.family'] = 'serif'
rcParams['font.serif'] = ['Times New Roman', 'Times', 'DejaVu Serif']
rcParams['font.weight'] = 'bold'
rcParams['axes.labelweight'] = 'bold'
rcParams['axes.titleweight'] = 'bold'
rcParams['axes.labelsize'] = 17
rcParams['xtick.labelsize'] = 16
rcParams['ytick.labelsize'] = 16
rcParams['legend.fontsize'] = 13
rcParams['axes.unicode_minus'] = False
rcParams['pdf.fonttype'] = 42
rcParams['ps.fonttype'] = 42

HERE = os.path.dirname(os.path.abspath(__file__))
CMS_DIR = os.path.join(HERE, 'cms')

DISPLAY_NAME = {
    'slam_L': 'slap_L',
    'slam_R': 'slap_R',
}

PANEL_KEYS = ['rd', 'rd+ra', 'rd+ra+re']
PANEL_TITLES = ['RD', 'RD + RA', 'RD + RA + RE']
PAPER_LABELS = ['bow', 'slap_L', 'slap_R', 'smash', 'volleyball']


def _display_label(name: str) -> str:
    return DISPLAY_NAME.get(name, name)


def _cm_accuracy(cm: np.ndarray) -> float:
    total = float(cm.sum())
    if total <= 0:
        return 0.0
    return float(np.trace(cm)) / total


def _latest_results(runs_dir: str, modality_key: str) -> str | None:
    pattern = os.path.join(runs_dir, f'{modality_key}_file_*_results.json')
    files = sorted(glob.glob(pattern))
    return files[-1] if files else None


def load_from_packed(cms_dir: str):
    meta_path = os.path.join(cms_dir, 'meta.json')
    with open(meta_path, 'r') as f:
        meta = json.load(f)
    labels = [_display_label(x) for x in meta.get('labels', PAPER_LABELS)]
    panels = []
    for panel, title in zip(meta['panels'], PANEL_TITLES):
        cm = np.load(os.path.join(cms_dir, panel['cm']))
        acc = panel.get('acc')
        if acc is None:
            acc = _cm_accuracy(cm)
        panels.append({'title': title, 'cm': cm, 'acc': float(acc)})
    return labels, panels, meta


def load_from_runs(runs_dir: str):
    labels = None
    panels = []
    metas = []
    for key, title in zip(PANEL_KEYS, PANEL_TITLES):
        result_path = _latest_results(runs_dir, key)
        if result_path is None:
            raise FileNotFoundError(
                f'No {key}_file_*_results.json under {runs_dir}. '
                'Train with --split_mode file, or omit --from_runs to use cms/.'
            )
        with open(result_path, 'r') as f:
            res = json.load(f)
        cm_path = res.get('test_cm_effective_path', '')
        if not os.path.isabs(cm_path):
            cm_path = os.path.join(os.path.dirname(result_path), os.path.basename(cm_path))
        cm = np.load(cm_path)
        names = [_display_label(n) for n in res.get('test_effective_class_names', PAPER_LABELS)]
        if labels is None:
            labels = names
        panels.append({
            'title': title,
            'cm': cm,
            'acc': float(res['final_test_acc']),
        })
        metas.append({
            'modalities': res.get('modalities'),
            'results': result_path,
            'test_acc': res.get('final_test_acc'),
            'split_info': res.get('split_info'),
        })
    return labels, panels, {'from_runs': metas}


def draw_cms(labels, panels, out_dir, output_basename='figure14d'):
    fig = plt.figure(figsize=(10.5, 3.4), dpi=200)
    gs = GridSpec(1, 3, figure=fig, width_ratios=[1, 1, 1.12], wspace=0.05)
    axes = [fig.add_subplot(gs[0]), fig.add_subplot(gs[1]), fig.add_subplot(gs[2])]
    cmap = plt.cm.Blues

    for idx, (ax, panel) in enumerate(zip(axes, panels)):
        cm = np.asarray(panel['cm'], dtype=float)
        row_sums = cm.sum(axis=1, keepdims=True)
        cm_pct = np.where(row_sums > 0, cm / row_sums * 100.0, 0.0)
        im = ax.imshow(cm_pct, interpolation='nearest', cmap=cmap, aspect='equal', vmin=0, vmax=100)

        if idx == 2:
            divider = make_axes_locatable(ax)
            cax = divider.append_axes('right', size='11%', pad=0.06)
            cbar = fig.colorbar(im, cax=cax)
            cbar.ax.tick_params(labelsize=10)
            cbar.set_label('Percentage (%)', fontsize=11)

        acc = panel['acc']
        ax.set_title(f"{panel['title']} (acc={acc:.4f})", fontsize=11, pad=6)

        tick_marks = np.arange(len(labels))
        ax.set_xticks(tick_marks)
        ax.set_xticklabels(labels, fontsize=10, rotation=45, ha='right')
        ax.set_yticks(tick_marks)
        if idx == 0:
            ax.set_yticklabels(labels, fontsize=10)
            ax.set_ylabel('True label', fontsize=11)
        else:
            ax.set_yticklabels([])
        ax.set_xlabel('Predicted label', fontsize=11)

        thresh = 50.0
        for i in range(cm_pct.shape[0]):
            for j in range(cm_pct.shape[1]):
                val = cm_pct[i, j]
                txt = f'{val:.0f}%' if val > 0 else '0%'
                ax.text(
                    j, i, txt,
                    ha='center', va='center', fontsize=10,
                    color='white' if val > thresh else 'black',
                )

    out_pdf = os.path.join(out_dir, f'{output_basename}.pdf')
    out_png = os.path.join(out_dir, f'{output_basename}.png')
    fig.savefig(out_pdf, bbox_inches='tight', format='pdf')
    fig.savefig(out_png, dpi=300, bbox_inches='tight')
    plt.close(fig)
    print(f'Saved: {out_pdf}')
    print(f'Saved: {out_png}')
    return out_pdf


def plot_figure14d(from_runs=None, output_basename='figure14d'):
    if from_runs:
        labels, panels, meta = load_from_runs(from_runs)
        print(f'Loaded file-split CMs from {from_runs}')
    else:
        labels, panels, meta = load_from_packed(CMS_DIR)
        print(f'Loaded packed CMs from {CMS_DIR}')
    print(f"split_mode={meta.get('split_mode', 'file')}")
    for p in panels:
        n = int(np.asarray(p['cm']).sum())
        print(f"  {p['title']}: N={n}, acc={p['acc']:.4f}")
    return draw_cms(labels, panels, HERE, output_basename=output_basename)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--from_runs',
        default='',
        help='Directory with {rd,rd+ra,rd+ra+re}_file_*_results.json. '
             'Empty = use packed cms/.',
    )
    parser.add_argument('--output_basename', default='figure14d')
    args = parser.parse_args()
    plot_figure14d(
        from_runs=args.from_runs or None,
        output_basename=args.output_basename,
    )


if __name__ == '__main__':
    main()
