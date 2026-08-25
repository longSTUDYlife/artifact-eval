#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Figure 14(d): HAR confusion matrices (RD / RD+RA / RD+RA+RE).

Default / --eval:
  load packed checkpoints/, test 4.mat on CPU, plot CMs.

Env:
  FIGURE14D_TRAIN=1   retrain from filtered/*.mat (needs all 6 files;
                      rebuild a CUDA image locally — not the default CPU image)
  FIGURE14D_TRAIN=0   plot packed cms/ (skip the network)
  SMOKE=1             1 epoch × 3 if retraining
  FIGURE14D_EPOCHS=N  override epoch count (default 40)
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TRAINER = HERE / "main-rd-ra-re-multifile.py"
CKPT_DIR = HERE / "checkpoints"
MANIFEST = CKPT_DIR / "manifest.json"
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
    return _truthy(_flag("FIGURE14D_TRAIN"))


def want_cms_only() -> bool:
    explicit = _flag("FIGURE14D_TRAIN")
    return explicit is not None and not _truthy(explicit)


def checkpoints_ready(fig_dir: Path | None = None) -> bool:
    fig_dir = Path(fig_dir) if fig_dir is not None else HERE
    ckpt_dir = fig_dir / "checkpoints"
    manifest = ckpt_dir / "manifest.json"
    if not manifest.is_file():
        return False
    with manifest.open() as f:
        meta = json.load(f)
    for panel in meta.get("panels", []):
        if not (ckpt_dir / panel["ckpt"]).is_file():
            return False
    return True


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
            "Rebuild Docker with --build-arg USE_CUDA=1, or omit FIGURE14D_TRAIN "
            "to eval packed checkpoints."
        ) from exc

    import torch

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Figure14d train device={device} epochs={epochs} split=file")
    if device == "cpu":
        print("CPU training is slow (hours). Rebuild with --build-arg USE_CUDA=1.")

    runs = fig_dir / "runs"
    runs.mkdir(exist_ok=True)
    data_dir = fig_dir / "filtered"
    mats = sorted(data_dir.glob("*.mat"))
    if len(mats) != 6:
        raise FileNotFoundError(
            f"Retrain needs 6 .mat files in {data_dir}, found {len(mats)}. "
            "The CPU image ships 4.mat only; comment the other filtered/*.mat "
            "lines out of .dockerignore and rebuild with --build-arg USE_CUDA=1."
        )

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


def _trainer(fig_dir: Path):
    import importlib.util

    path = fig_dir / "main-rd-ra-re-multifile.py"
    spec = importlib.util.spec_from_file_location("fig14d_trainer", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run_eval(fig_dir: Path) -> Path:
    import torch

    trainer = _trainer(fig_dir)
    device = torch.device("cpu")
    print(f"Figure14d eval device={device} (packed checkpoints, test 4.mat)")

    ckpt_dir = fig_dir / "checkpoints"
    with (ckpt_dir / "manifest.json").open() as f:
        meta = json.load(f)
    data_dir = str(fig_dir / "filtered")
    test_file = meta.get("test_file", "4.mat")
    runs = fig_dir / "runs"
    runs.mkdir(exist_ok=True)

    first_ckpt = ckpt_dir / meta["panels"][0]["ckpt"]
    first = trainer.load_torch_checkpoint(str(first_ckpt), device)
    action_list = list(first["action_list"])
    print(f"Loading test file {test_file} once for all three models...")
    test_records = trainer.load_eval_test_records(data_dir, test_file, action_list)

    for panel, expected in zip(meta["panels"], PAPER_ACC):
        ckpt_path = ckpt_dir / panel["ckpt"]
        res = trainer.evaluate_checkpoint(
            str(ckpt_path),
            data_dir,
            str(runs),
            device=device,
            test_records=test_records,
            run_name=f"{panel['key']}_file_eval",
            test_file=test_file,
        )
        got = float(res["final_test_acc"])
        print(
            f"  {panel['title']}: acc={got:.4f}  paper={float(panel.get('paper_acc', expected)):.4f}"
        )
    return runs


def plot_cms(fig_dir: Path, from_runs: Path | None) -> None:
    if str(fig_dir) not in sys.path:
        sys.path.insert(0, str(fig_dir))
    from plot_figure14d import plot_figure14d

    plot_figure14d(from_runs=str(from_runs) if from_runs else None)


def regenerate_figure14d(
    fig_dir: Path | None = None,
    mode: str | None = None,
) -> None:
    """mode: 'eval' | 'train' | 'cms'. Default: eval if checkpoints exist."""
    fig_dir = Path(fig_dir) if fig_dir is not None else HERE
    if mode is None:
        if want_train():
            mode = "train"
        elif want_cms_only():
            mode = "cms"
        elif checkpoints_ready(fig_dir):
            mode = "eval"
        else:
            mode = "cms"

    if mode == "train":
        runs = run_training(fig_dir, epoch_count())
        plot_cms(fig_dir, runs)
        print("Paper packed acc (for reference):", PAPER_ACC)
        print("Retrained numbers come from runs/; GPU nondeterminism may differ slightly.")
        return

    if mode == "eval":
        try:
            import torch  # noqa: F401
        except ImportError:
            print("PyTorch missing; plotting packed cms/ instead of checkpoint eval.")
            plot_cms(fig_dir, None)
            return
        try:
            runs = run_eval(fig_dir)
        except Exception as exc:
            print(f"Checkpoint eval failed ({exc}); plotting packed cms/.")
            plot_cms(fig_dir, None)
            return
        plot_cms(fig_dir, runs)
        print("Paper packed acc (for reference):", PAPER_ACC)
        return

    print("Figure14d: plotting packed cms/ (FIGURE14D_TRAIN=0).")
    plot_cms(fig_dir, None)


if __name__ == "__main__":
    regenerate_figure14d(HERE)
