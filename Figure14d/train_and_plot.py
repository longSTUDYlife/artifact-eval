#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 14(d): train RD / RD+RA / RD+RA+RE (file split) then plot CMs.

Env:
  FIGURE14D_TRAIN=1   force retrain (CPU or GPU)
  FIGURE14D_TRAIN=0   plot packed cms/ (no training)
  unset               retrain if CUDA is available, else packed cms
  SMOKE=1             1 epoch × 3 (pipeline check, numbers will not match paper)
  FIGURE14D_EPOCHS=N  override epoch count (default 40)
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TRAINER = HERE / "main-rd-ra-re-multifile.py"
MODALITIES = ("rd", "rd,ra", "rd,ra,re")
PAPER_ACC = (0.5119, 0.8373, 0.9898)


def _flag(name: str) -> str | None:
    v = os.environ.get(name)
    if v is None:
        return None
    return v.strip().lower()


def _truthy(v: str | None) -> bool:
    return v in ("1", "true", "yes", "on")


def want_train() -> bool:
    explicit = _flag("FIGURE14D_TRAIN")
    if explicit is not None:
        return _truthy(explicit)
    try:
        import torch
        return bool(torch.cuda.is_available())
    except Exception:
        return False


def epoch_count() -> int:
    if _truthy(_flag("SMOKE")):
        return 1
    raw = os.environ.get("FIGURE14D_EPOCHS", "40")
    return max(1, int(raw))


def run_training(fig_dir: Path, epochs: int) -> Path:
    try:
        import torch  # noqa: F401
    except ImportError as exc:
        raise SystemExit(
            "PyTorch is required to retrain Figure14d. "
            "Rebuild Docker with torch, or set FIGURE14D_TRAIN=0 to plot packed cms."
        ) from exc

    import torch

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Figure14d train device={device} epochs={epochs} split=file")
    if device == "cpu":
        print("CPU training is slow (hours). GPU: docker run --gpus all ...")

    runs = fig_dir / "runs"
    runs.mkdir(exist_ok=True)
    data_dir = fig_dir / "filtered"
    mats = sorted(data_dir.glob("*.mat"))
    if len(mats) != 6:
        raise FileNotFoundError(f"Need 6 .mat files in {data_dir}, found {len(mats)}")

    for mods in MODALITIES:
        cmd = [
            sys.executable,
            str(TRAINER),
            "--data_dir", str(data_dir),
            "--out_dir", str(runs),
            "--split_mode", "file",
            "--modalities", mods,
            "--seed", "42",
            "--epochs", str(epochs),
        ]
        print("\n========", " ".join(cmd[-6:]), "========")
        subprocess.check_call(cmd, cwd=str(fig_dir))
    return runs


def plot_cms(fig_dir: Path, from_runs: Path | None) -> None:
    if str(fig_dir) not in sys.path:
        sys.path.insert(0, str(fig_dir))
    from plot_figure14d import plot_figure14d

    plot_figure14d(from_runs=str(from_runs) if from_runs else None)


def regenerate_figure14d(fig_dir: Path | None = None) -> None:
    fig_dir = Path(fig_dir) if fig_dir is not None else HERE
    if want_train():
        runs = run_training(fig_dir, epoch_count())
        plot_cms(fig_dir, runs)
        print("Paper packed acc (for reference):", PAPER_ACC)
        print("Retrained numbers come from runs/; GPU nondeterminism may differ slightly.")
    else:
        print(
            "Figure14d: plotting packed cms/ "
            "(set FIGURE14D_TRAIN=1 to retrain; GPU docker auto-trains)."
        )
        plot_cms(fig_dir, None)


if __name__ == "__main__":
    regenerate_figure14d(HERE)
