#!/usr/bin/env python3
"""
Figure 14(d) HAR trainer (RD / RD+RA / RD+RA+RE).

Default split is --split_mode file (whole .mat files, seed=42,
val_ratio=0.15, test_ratio=0.15). With the 6 filtered mats this is
train 4 / val 1 / test 1.

Multi-file UWB HAR training script with configurable modalities.

Supported modalities:
  - rd : range-doppler      all_rd shape [N, 181, 83]
  - ra : range-azimuth      all_ra shape [N, 181, 64]
  - re : range-elevation    all_re shape [N, 181, 64]

Supported modality combinations:
  - rd
  - ra
  - re
  - rd,ra
  - rd,re
  - ra,re
  - rd,ra,re

Supported split modes:
  - subject : split by subject identity
  - file    : split by file
  - loso    : leave-one-subject-out

Each .mat file is expected to contain:
  - action_list
  - all_labels
  - all_rd
  - all_ra
  - all_re
  - range_axis
  - velocity_axis
  - person

Features:
- supports MATLAB old .mat and MATLAB v7.3 HDF5 .mat
- multi-file loading
- global label unification by action name
- modality-configurable CNN + LSTM
- TensorBoard logging
- side-by-side validation/test confusion-matrix heatmaps
- dedicated colorbar axis to avoid overlap
- per-subject test accuracy
"""

import os
import glob
import json
import time
import random
import argparse
from typing import Dict, List, Tuple, Any

import numpy as np
import scipy.io as sio
import h5py
import matplotlib.pyplot as plt
from sklearn.metrics import confusion_matrix, classification_report

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from torch.utils.tensorboard import SummaryWriter


# ============================================================
# Utilities
# ============================================================

VALID_MODALITIES = {"rd", "ra", "re"}
MODALITY_TO_KEY = {
    "rd": "all_rd",
    "ra": "all_ra",
    "re": "all_re",
}


def set_seed(seed: int = 42):
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)
    torch.cuda.manual_seed_all(seed)


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def save_json(obj: Dict[str, Any], path: str):
    with open(path, "w") as f:
        json.dump(obj, f, indent=2)


def parse_modalities(modality_str: str) -> List[str]:
    mods = [m.strip().lower() for m in modality_str.split(",") if m.strip()]
    if len(mods) == 0:
        raise ValueError("No modalities specified")
    if not set(mods).issubset(VALID_MODALITIES):
        raise ValueError(f"Invalid modalities: {mods}. Valid: {sorted(VALID_MODALITIES)}")
    seen = set()
    ordered = []
    for m in mods:
        if m not in seen:
            ordered.append(m)
            seen.add(m)
    return ordered


# ============================================================
# MAT loading
# ============================================================

def matlab_cellstr_to_list(cell_array) -> List[str]:
    result = []
    flat = np.ravel(cell_array)
    for x in flat:
        if isinstance(x, np.ndarray):
            try:
                if x.dtype.kind in ["U", "S"]:
                    result.append("".join(np.ravel(x).tolist()))
                else:
                    vals = np.ravel(x)
                    chars = []
                    for c in vals:
                        try:
                            ci = int(c)
                            if ci != 0:
                                chars.append(chr(ci))
                        except Exception:
                            pass
                    result.append("".join(chars) if chars else str(x))
            except Exception:
                result.append(str(x))
        else:
            result.append(str(x))
    return result


def _decode_matlab_string_h5(x):
    x = np.array(x)
    if x.dtype.kind in ("u", "i"):
        return "".join(chr(int(c)) for c in x.flatten() if int(c) != 0)
    elif x.dtype.kind in ("S", "U"):
        return "".join(x.astype(str).flatten().tolist())
    return str(x)


def _read_cell_array_strings_h5(f, cell_dataset):
    out = []
    refs = np.ravel(cell_dataset[()])
    for ref in refs:
        obj = f[ref]
        out.append(_decode_matlab_string_h5(obj[()]))
    return out


def _transpose_to_nhw(arr: np.ndarray, expected_hw: Tuple[int, int], name: str) -> np.ndarray:
    H, W = expected_hw
    if arr.ndim != 3:
        raise ValueError(f"{name}: expected 3D array, got {arr.shape}")

    if arr.shape[1:] == (H, W):
        return arr
    if arr.shape[:2] == (H, W):
        return np.transpose(arr, (2, 0, 1))
    if arr.shape[1:] == (W, H):
        return np.transpose(arr, (0, 2, 1))
    if arr.shape[:2] == (W, H):
        return np.transpose(arr, (2, 1, 0))

    dims = list(arr.shape)
    if H in dims and W in dims:
        h_idx = dims.index(H)
        w_idx = dims.index(W)
        n_idx = [i for i in range(3) if i not in [h_idx, w_idx]][0]
        return np.transpose(arr, (n_idx, h_idx, w_idx))

    raise ValueError(f"{name}: cannot infer transpose for shape {arr.shape}, expected [N,{H},{W}]")


def load_mat_file(mat_path: str) -> Dict[str, Any]:
    try:
        data = sio.loadmat(mat_path)

        action_list = matlab_cellstr_to_list(data["action_list"])
        all_labels = np.asarray(data["all_labels"]).reshape(-1).astype(np.int64)

        all_rd = _transpose_to_nhw(np.asarray(data["all_rd"]).astype(np.float32), (181, 83), "all_rd")
        all_ra = _transpose_to_nhw(np.asarray(data["all_ra"]).astype(np.float32), (181, 64), "all_ra")
        all_re = _transpose_to_nhw(np.asarray(data["all_re"]).astype(np.float32), (181, 64), "all_re")

        range_axis = np.asarray(data["range_axis"]).reshape(-1)
        velocity_axis = np.asarray(data["velocity_axis"]).reshape(-1)

        person = data.get("person", None)
        if person is not None:
            try:
                person = matlab_cellstr_to_list(person)[0]
            except Exception:
                person = str(person)
        else:
            raise KeyError(f"'person' not found in {mat_path}")

    except NotImplementedError:
        with h5py.File(mat_path, "r") as f:
            action_list = _read_cell_array_strings_h5(f, f["action_list"])
            all_labels = np.array(f["all_labels"]).astype(np.int64).reshape(-1)

            all_rd = _transpose_to_nhw(np.array(f["all_rd"]).astype(np.float32), (181, 83), "all_rd")
            all_ra = _transpose_to_nhw(np.array(f["all_ra"]).astype(np.float32), (181, 64), "all_ra")
            all_re = _transpose_to_nhw(np.array(f["all_re"]).astype(np.float32), (181, 64), "all_re")

            range_axis = np.array(f["range_axis"]).reshape(-1)
            velocity_axis = np.array(f["velocity_axis"]).reshape(-1)

            if "person" not in f:
                raise KeyError(f"'person' not found in {mat_path}")
            person = _decode_matlab_string_h5(f["person"][()])

    all_labels = all_labels - 1

    assert all_rd.ndim == 3 and all_rd.shape[1:] == (181, 83), f"{mat_path}: bad all_rd shape {all_rd.shape}"
    assert all_ra.ndim == 3 and all_ra.shape[1:] == (181, 64), f"{mat_path}: bad all_ra shape {all_ra.shape}"
    assert all_re.ndim == 3 and all_re.shape[1:] == (181, 64), f"{mat_path}: bad all_re shape {all_re.shape}"

    N = len(all_labels)
    assert N == all_rd.shape[0], f"{mat_path}: labels and RD length mismatch"
    assert N == all_ra.shape[0], f"{mat_path}: labels and RA length mismatch"
    assert N == all_re.shape[0], f"{mat_path}: labels and RE length mismatch"
    assert all_labels.min() >= 0, f"{mat_path}: labels should be >=0 after MATLAB->Python conversion"

    # Use 1.mat → "1" as the public id; ignore names stored inside the .mat.
    file_id = os.path.splitext(os.path.basename(mat_path))[0]

    return {
        "file_path": mat_path,
        "file_name": os.path.basename(mat_path),
        "person": file_id,
        "action_list_local": action_list,
        "all_labels_local": all_labels,
        "all_rd": all_rd,
        "all_ra": all_ra,
        "all_re": all_re,
        "range_axis": range_axis,
        "velocity_axis": velocity_axis,
    }


# ============================================================
# Global label mapping
# ============================================================

def build_global_action_map(raw_records: List[Dict[str, Any]]) -> Tuple[List[str], Dict[str, int]]:
    action_names = set()
    for rec in raw_records:
        for name in rec["action_list_local"]:
            action_names.add(str(name))
    global_action_list = sorted(action_names)
    action_to_idx = {name: i for i, name in enumerate(global_action_list)}
    return global_action_list, action_to_idx


def convert_local_labels_to_global(rec: Dict[str, Any], action_to_idx: Dict[str, int]) -> Dict[str, Any]:
    local_names = rec["action_list_local"]
    local_labels = rec["all_labels_local"]

    global_labels = np.zeros_like(local_labels, dtype=np.int64)
    for i, local_idx in enumerate(local_labels):
        action_name = str(local_names[int(local_idx)])
        global_labels[i] = action_to_idx[action_name]

    out = dict(rec)
    out["all_labels"] = global_labels
    return out


# ============================================================
# Segments and sequences
# ============================================================

def find_label_segments(labels: np.ndarray) -> List[Tuple[int, int, int]]:
    segments = []
    if len(labels) == 0:
        return segments

    start = 0
    current = labels[0]
    for i in range(1, len(labels)):
        if labels[i] != current:
            segments.append((start, i, int(current)))
            start = i
            current = labels[i]
    segments.append((start, len(labels), int(current)))
    return segments


def build_sequence_index_for_record(rec_idx: int, rec: Dict[str, Any], seq_len: int, stride: int):
    labels = rec["all_labels"]
    samples = []

    for s, e, y in find_label_segments(labels):
        if e - s < seq_len:
            continue
        for start in range(s, e - seq_len + 1, stride):
            end = start + seq_len
            if np.all(labels[start:end] == y):
                samples.append((rec_idx, start, end, y))

    return samples


def build_sample_index(records: List[Dict[str, Any]], seq_len: int, stride: int):
    all_samples = []
    for rec_idx, rec in enumerate(records):
        all_samples.extend(build_sequence_index_for_record(rec_idx, rec, seq_len, stride))
    return all_samples


# ============================================================
# Splits
# ============================================================

def split_subjects(subjects: List[str], val_ratio: float, test_ratio: float, seed: int):
    subjects = sorted(subjects)
    rng = np.random.default_rng(seed)
    shuffled = subjects.copy()
    rng.shuffle(shuffled)

    n = len(shuffled)
    n_test = max(1, int(round(n * test_ratio))) if n >= 3 else 1
    n_val = max(1, int(round(n * val_ratio))) if n >= 4 else 1

    if n_test + n_val >= n:
        n_test = 1
        n_val = 1 if n >= 3 else 0

    test_subjects = shuffled[:n_test]
    val_subjects = shuffled[n_test:n_test + n_val]
    train_subjects = shuffled[n_test + n_val:]

    if len(train_subjects) == 0:
        raise RuntimeError("No training subjects left. Need more subjects or different split ratios.")

    return train_subjects, val_subjects, test_subjects


def split_files(file_names: List[str], val_ratio: float, test_ratio: float, seed: int):
    files = sorted(file_names)
    rng = np.random.default_rng(seed)
    shuffled = files.copy()
    rng.shuffle(shuffled)

    n = len(shuffled)
    n_test = max(1, int(round(n * test_ratio))) if n >= 3 else 1
    n_val = max(1, int(round(n * val_ratio))) if n >= 4 else 1

    if n_test + n_val >= n:
        n_test = 1
        n_val = 1 if n >= 3 else 0

    test_files = shuffled[:n_test]
    val_files = shuffled[n_test:n_test + n_val]
    train_files = shuffled[n_test + n_val:]

    if len(train_files) == 0:
        raise RuntimeError("No training files left. Need more files or different split ratios.")

    return train_files, val_files, test_files


def build_record_splits(records: List[Dict[str, Any]], args):
    unique_subjects = sorted(set(rec["person"] for rec in records))
    unique_files = sorted(rec["file_name"] for rec in records)

    if args.split_mode == "subject":
        train_subj, val_subj, test_subj = split_subjects(
            unique_subjects, args.val_ratio, args.test_ratio, args.seed
        )
        train_records = [r for r in records if r["person"] in train_subj]
        val_records = [r for r in records if r["person"] in val_subj]
        test_records = [r for r in records if r["person"] in test_subj]
        split_info = {
            "train_subjects": train_subj,
            "val_subjects": val_subj,
            "test_subjects": test_subj,
        }

    elif args.split_mode == "file":
        train_files, val_files, test_files = split_files(
            unique_files, args.val_ratio, args.test_ratio, args.seed
        )
        train_records = [r for r in records if r["file_name"] in train_files]
        val_records = [r for r in records if r["file_name"] in val_files]
        test_records = [r for r in records if r["file_name"] in test_files]
        split_info = {
            "train_files": train_files,
            "val_files": val_files,
            "test_files": test_files,
        }

    elif args.split_mode == "loso":
        if not args.test_subject:
            raise ValueError("--test_subject is required when split_mode=loso")
        if args.test_subject not in unique_subjects:
            raise ValueError(f"test_subject '{args.test_subject}' not found. Available: {unique_subjects}")

        train_val_subjects = [s for s in unique_subjects if s != args.test_subject]
        if len(train_val_subjects) < 2:
            raise RuntimeError("LOSO requires at least two non-test subjects for train/val.")

        train_subj, val_subj, _ = split_subjects(train_val_subjects, args.val_ratio, 0.0, args.seed)
        test_subj = [args.test_subject]

        train_records = [r for r in records if r["person"] in train_subj]
        val_records = [r for r in records if r["person"] in val_subj]
        test_records = [r for r in records if r["person"] in test_subj]
        split_info = {
            "train_subjects": train_subj,
            "val_subjects": val_subj,
            "test_subjects": test_subj,
        }

    else:
        raise ValueError(f"Unsupported split mode: {args.split_mode}")

    if len(train_records) == 0 or len(val_records) == 0 or len(test_records) == 0:
        raise RuntimeError(
            f"Empty split detected: train={len(train_records)}, val={len(val_records)}, test={len(test_records)}"
        )

    return train_records, val_records, test_records, split_info


# ============================================================
# Normalization
# ============================================================

def compute_norm_stats_from_records(records: List[Dict[str, Any]], key: str, log_power: bool = True):
    chunks = []
    for rec in records:
        x = np.abs(rec[key]).astype(np.float32)
        if log_power:
            x = np.log1p(x)
        chunks.append(x)

    x_all = np.concatenate(chunks, axis=0)
    mean = float(x_all.mean())
    std = float(x_all.std())
    std = max(std, 1e-6)
    return mean, std


def preprocess_single_map(x: np.ndarray, mean: float, std: float, log_power: bool = True):
    x = np.abs(x).astype(np.float32)
    if log_power:
        x = np.log1p(x)
    x = (x - mean) / std
    return x


# ============================================================
# Dataset
# ============================================================

class MultiModalSequenceDataset(Dataset):
    def __init__(self, records, samples, modalities, stats, log_power=True):
        self.records = records
        self.samples = samples
        self.modalities = modalities
        self.stats = stats
        self.log_power = log_power

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        rec_idx, start, end, label = self.samples[idx]
        rec = self.records[rec_idx]

        out = {}
        for mod in self.modalities:
            key = MODALITY_TO_KEY[mod]
            x = preprocess_single_map(
                rec[key][start:end],
                self.stats[f"{mod}_mean"],
                self.stats[f"{mod}_std"],
                self.log_power,
            )
            out[mod] = torch.from_numpy(x[:, None, :, :]).float()

        out["label"] = torch.tensor(label, dtype=torch.long)
        return out


# ============================================================
# Models
# ============================================================

class FrameCNN(nn.Module):
    def __init__(self, in_channels=1, feature_dim=128):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(in_channels, 16, 3, padding=1),
            nn.BatchNorm2d(16),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),

            nn.Conv2d(16, 32, 3, padding=1),
            nn.BatchNorm2d(32),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),

            nn.Conv2d(32, 64, 3, padding=1),
            nn.BatchNorm2d(64),
            nn.ReLU(inplace=True),
            nn.MaxPool2d(2),

            nn.Conv2d(64, 96, 3, padding=1),
            nn.BatchNorm2d(96),
            nn.ReLU(inplace=True),

            nn.AdaptiveAvgPool2d((4, 4)),
        )
        self.fc = nn.Sequential(
            nn.Flatten(),
            nn.Linear(96 * 4 * 4, feature_dim),
            nn.ReLU(inplace=True),
            nn.Dropout(0.3),
        )

    def forward(self, x):
        return self.fc(self.features(x))


class MultiBranchCNNLSTM(nn.Module):
    def __init__(
        self,
        modalities,
        num_classes,
        branch_feature_dim=128,
        fusion_dim=256,
        lstm_hidden_dim=128,
        lstm_layers=2,
        bidirectional=False,
        dropout=0.3,
    ):
        super().__init__()
        self.modalities = modalities

        self.branches = nn.ModuleDict()
        for mod in modalities:
            self.branches[mod] = FrameCNN(in_channels=1, feature_dim=branch_feature_dim)

        input_dim = branch_feature_dim * len(modalities)

        self.fusion = nn.Sequential(
            nn.Linear(input_dim, fusion_dim),
            nn.ReLU(inplace=True),
            nn.Dropout(dropout),
        )

        self.lstm = nn.LSTM(
            input_size=fusion_dim,
            hidden_size=lstm_hidden_dim,
            num_layers=lstm_layers,
            batch_first=True,
            dropout=dropout if lstm_layers > 1 else 0.0,
            bidirectional=bidirectional,
        )

        out_dim = lstm_hidden_dim * (2 if bidirectional else 1)
        self.classifier = nn.Sequential(
            nn.Linear(out_dim, 128),
            nn.ReLU(inplace=True),
            nn.Dropout(dropout),
            nn.Linear(128, num_classes),
        )

    def forward(self, batch):
        feats = []
        B = None
        T = None

        for mod in self.modalities:
            x = batch[mod]
            b, t, c, h, w = x.shape
            if B is None:
                B, T = b, t
            else:
                assert B == b and T == t

            x = x.view(B * T, c, h, w)
            f = self.branches[mod](x)
            f = f.view(B, T, -1)
            feats.append(f)

        fused = torch.cat(feats, dim=-1)
        fused = self.fusion(fused)

        lstm_out, _ = self.lstm(fused)
        last_out = lstm_out[:, -1, :]
        logits = self.classifier(last_out)
        return logits


# ============================================================
# Training / evaluation
# ============================================================

def move_batch_to_device(batch, device):
    return {k: v.to(device) for k, v in batch.items()}


def run_one_epoch(model, loader, criterion, optimizer, device):
    model.train()
    total_loss, total_correct, total_count = 0.0, 0, 0

    for batch in loader:
        batch = move_batch_to_device(batch, device)
        y = batch["label"]

        optimizer.zero_grad()
        logits = model(batch)
        loss = criterion(logits, y)
        loss.backward()
        optimizer.step()

        total_loss += loss.item() * y.size(0)
        total_correct += (logits.argmax(dim=1) == y).sum().item()
        total_count += y.size(0)

    return total_loss / max(total_count, 1), total_correct / max(total_count, 1)


@torch.no_grad()
def evaluate(model, loader, criterion, device):
    model.eval()
    total_loss, total_correct, total_count = 0.0, 0, 0
    all_preds, all_targets = [], []

    for batch in loader:
        batch = move_batch_to_device(batch, device)
        y = batch["label"]

        logits = model(batch)
        loss = criterion(logits, y)

        preds = logits.argmax(dim=1)
        total_loss += loss.item() * y.size(0)
        total_correct += (preds == y).sum().item()
        total_count += y.size(0)

        all_preds.append(preds.cpu().numpy())
        all_targets.append(y.cpu().numpy())

    all_preds = np.concatenate(all_preds) if all_preds else np.array([])
    all_targets = np.concatenate(all_targets) if all_targets else np.array([])
    return total_loss / max(total_count, 1), total_correct / max(total_count, 1), all_preds, all_targets


# ============================================================
# Confusion matrix plotting
# ============================================================

def _prepare_cm_for_plot(cm, normalize=False):
    if cm.size == 0:
        return cm.astype(np.float64)
    cm_plot = cm.astype(np.float64)
    if normalize:
        row_sums = cm_plot.sum(axis=1, keepdims=True)
        row_sums[row_sums == 0] = 1.0
        cm_plot = cm_plot / row_sums
    return cm_plot


def plot_single_confusion_on_ax(ax, cm, class_names, normalize=False, title=""):
    if cm.size == 0:
        ax.set_title(title)
        ax.axis("off")
        return None

    cm_plot = _prepare_cm_for_plot(cm, normalize=normalize)

    im = ax.imshow(cm_plot, interpolation="nearest", cmap="Blues", aspect="auto")
    ax.set_title(title)
    ax.set_xticks(np.arange(len(class_names)))
    ax.set_yticks(np.arange(len(class_names)))
    ax.set_xticklabels(class_names, rotation=45, ha="right", rotation_mode="anchor")
    ax.set_yticklabels(class_names)
    ax.set_xlabel("Predicted label")
    ax.set_ylabel("True label")

    thresh = cm_plot.max() / 2.0 if cm_plot.size > 0 else 0.0
    for i in range(cm_plot.shape[0]):
        for j in range(cm_plot.shape[1]):
            txt = f"{cm_plot[i, j]:.2f}" if normalize else str(int(cm[i, j]))
            ax.text(
                j, i, txt,
                ha="center", va="center",
                color="white" if cm_plot[i, j] > thresh else "black",
                fontsize=8
            )

    return im


def plot_two_confusion_matrices_side_by_side(
    cm_left,
    names_left,
    cm_right,
    names_right,
    out_path,
    normalize=False,
    left_title="Validation",
    right_title="Test",
    suptitle="Confusion Matrices",
):
    """
    Save two confusion matrices side by side with a dedicated colorbar axis,
    so the colorbar never overlaps the matrices.
    """
    n_left = max(len(names_left), 1)
    n_right = max(len(names_right), 1)
    max_n = max(n_left, n_right)

    fig_w = max(14, min(1.2 * (n_left + n_right) + 4, 30))
    fig_h = max(7, min(0.9 * max_n + 3, 18))

    fig = plt.figure(figsize=(fig_w, fig_h), constrained_layout=False)
    gs = fig.add_gridspec(1, 3, width_ratios=[1, 1, 0.05], wspace=0.25)

    ax_left = fig.add_subplot(gs[0, 0])
    ax_right = fig.add_subplot(gs[0, 1])
    cax = fig.add_subplot(gs[0, 2])

    im_left = plot_single_confusion_on_ax(
        ax_left, cm_left, names_left, normalize=normalize, title=left_title
    )
    im_right = plot_single_confusion_on_ax(
        ax_right, cm_right, names_right, normalize=normalize, title=right_title
    )

    left_plot = _prepare_cm_for_plot(cm_left, normalize=normalize)
    right_plot = _prepare_cm_for_plot(cm_right, normalize=normalize)

    vmax = 1.0 if normalize else 0.0
    if left_plot.size > 0:
        vmax = max(vmax, float(left_plot.max()))
    if right_plot.size > 0:
        vmax = max(vmax, float(right_plot.max()))
    if vmax <= 0:
        vmax = 1.0

    if im_left is not None:
        im_left.set_clim(0, vmax)
    if im_right is not None:
        im_right.set_clim(0, vmax)

    ref_im = im_right if im_right is not None else im_left
    if ref_im is not None:
        cbar = fig.colorbar(ref_im, cax=cax)
        cbar.ax.set_ylabel("Normalized value" if normalize else "Count", rotation=90, va="bottom")

    fig.suptitle(suptitle, y=0.98)
    fig.subplots_adjust(left=0.06, right=0.96, bottom=0.12, top=0.92, wspace=0.28)
    fig.savefig(out_path, dpi=200, bbox_inches="tight")
    plt.close(fig)


# ============================================================
# Per-subject evaluation
# ============================================================

def compute_per_subject_accuracy(test_records, test_samples, y_pred):
    per_subject_true = {}
    per_subject_pred = {}

    for i, (rec_idx, _, _, label) in enumerate(test_samples):
        person = test_records[rec_idx]["person"]
        per_subject_true.setdefault(person, []).append(label)
        per_subject_pred.setdefault(person, []).append(int(y_pred[i]))

    results = {}
    for person in sorted(per_subject_true.keys()):
        yt = np.array(per_subject_true[person])
        yp = np.array(per_subject_pred[person])
        results[person] = float((yt == yp).mean()) if len(yt) > 0 else float("nan")
    return results


# ============================================================
# Data pipeline
# ============================================================

def make_datasets_and_loaders(train_records, val_records, test_records, args):
    train_samples = build_sample_index(train_records, args.seq_len, args.train_stride)
    val_samples = build_sample_index(val_records, args.seq_len, args.eval_stride)
    test_samples = build_sample_index(test_records, args.seq_len, args.eval_stride)

    if len(train_samples) == 0 or len(val_samples) == 0 or len(test_samples) == 0:
        raise RuntimeError(
            f"Empty sample split: train={len(train_samples)}, val={len(val_samples)}, test={len(test_samples)}. "
            f"Try reducing seq_len or checking the data."
        )

    stats = {}
    for mod in args.modalities:
        mean, std = compute_norm_stats_from_records(
            train_records,
            MODALITY_TO_KEY[mod],
            log_power=not args.no_log_power
        )
        stats[f"{mod}_mean"] = mean
        stats[f"{mod}_std"] = std

    train_dataset = MultiModalSequenceDataset(train_records, train_samples, args.modalities, stats, log_power=not args.no_log_power)
    val_dataset = MultiModalSequenceDataset(val_records, val_samples, args.modalities, stats, log_power=not args.no_log_power)
    test_dataset = MultiModalSequenceDataset(test_records, test_samples, args.modalities, stats, log_power=not args.no_log_power)

    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True, num_workers=args.num_workers, pin_memory=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size, shuffle=False, num_workers=args.num_workers, pin_memory=True)
    test_loader = DataLoader(test_dataset, batch_size=args.batch_size, shuffle=False, num_workers=args.num_workers, pin_memory=True)

    return train_loader, val_loader, test_loader, train_samples, val_samples, test_samples, stats


def build_model(num_classes, args):
    return MultiBranchCNNLSTM(
        modalities=args.modalities,
        num_classes=num_classes,
        branch_feature_dim=args.branch_feature_dim,
        fusion_dim=args.fusion_dim,
        lstm_hidden_dim=args.lstm_hidden_dim,
        lstm_layers=args.lstm_layers,
        bidirectional=args.bidirectional,
        dropout=args.dropout,
    )


# ============================================================
# Main
# ============================================================

def main(args):
    set_seed(args.seed)
    ensure_dir(args.out_dir)

    mat_files = sorted(glob.glob(os.path.join(args.data_dir, "*.mat")))
    if len(mat_files) == 0:
        raise FileNotFoundError(f"No .mat files found in {args.data_dir}")

    print(f"Found {len(mat_files)} .mat files")

    raw_records = []
    for p in mat_files:
        rec = load_mat_file(p)
        raw_records.append(rec)
        print(f"Loaded: {rec['file_name']} | person={rec['person']} | N={len(rec['all_labels_local'])}")

    global_action_list, action_to_idx = build_global_action_map(raw_records)
    records = [convert_local_labels_to_global(rec, action_to_idx) for rec in raw_records]

    print("\nGlobal action list:")
    for i, a in enumerate(global_action_list):
        print(f"  {i:2d}: {a}")

    unique_subjects = sorted(set(r["person"] for r in records))
    print(f"\nSubjects ({len(unique_subjects)}): {unique_subjects}")
    print(f"Modalities: {args.modalities}")

    train_records, val_records, test_records, split_info = build_record_splits(records, args)

    print("\nSplit info:")
    print(json.dumps(split_info, indent=2))
    print(f"Train files: {len(train_records)}")
    print(f"Val files  : {len(val_records)}")
    print(f"Test files : {len(test_records)}")

    (
        train_loader, val_loader, test_loader,
        train_samples, val_samples, test_samples,
        stats
    ) = make_datasets_and_loaders(train_records, val_records, test_records, args)

    print("\nSample counts:")
    print(f"  Train: {len(train_samples)}")
    print(f"  Val  : {len(val_samples)}")
    print(f"  Test : {len(test_samples)}")

    print("\nNormalization stats:")
    for mod in args.modalities:
        print(f"  {mod.upper()} mean/std = {stats[f'{mod}_mean']:.6f}, {stats[f'{mod}_std']:.6f}")

    device = torch.device("cuda" if torch.cuda.is_available() and not args.cpu else "cpu")
    print(f"\nUsing device: {device}")
    if device.type == "cuda":
        print(f"GPU: {torch.cuda.get_device_name(0)}")

    model = build_model(len(global_action_list), args).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, mode="min", factor=0.5, patience=5)

    mod_name = "+".join(args.modalities)
    run_name = f"{mod_name}_{args.split_mode}_{time.strftime('%Y%m%d_%H%M%S')}"
    tb_dir = os.path.join(args.out_dir, "tensorboard", run_name)
    ckpt_path = os.path.join(args.out_dir, f"{run_name}_best.pth")
    result_json = os.path.join(args.out_dir, f"{run_name}_results.json")

    val_cm_npy = os.path.join(args.out_dir, f"{run_name}_val_cm_effective.npy")
    test_cm_npy = os.path.join(args.out_dir, f"{run_name}_test_cm_effective.npy")
    cm_pair_png = os.path.join(args.out_dir, f"{run_name}_val_test_cm_side_by_side.png")
    cm_pair_norm_png = os.path.join(args.out_dir, f"{run_name}_val_test_cm_side_by_side_normalized.png")

    writer = SummaryWriter(log_dir=tb_dir)
    print(f"TensorBoard dir: {tb_dir}")

    best_val_acc = -1.0
    best_epoch = -1

    for epoch in range(1, args.epochs + 1):
        train_loss, train_acc = run_one_epoch(model, train_loader, criterion, optimizer, device)
        val_loss, val_acc, _, _ = evaluate(model, val_loader, criterion, device)

        scheduler.step(val_loss)
        lr_cur = optimizer.param_groups[0]["lr"]

        print(
            f"Epoch [{epoch:03d}/{args.epochs:03d}] "
            f"Train Loss: {train_loss:.4f} | Train Acc: {train_acc:.4f} | "
            f"Val Loss: {val_loss:.4f} | Val Acc: {val_acc:.4f} | LR: {lr_cur:.2e}"
        )

        writer.add_scalar("Loss/train", train_loss, epoch)
        writer.add_scalar("Loss/val", val_loss, epoch)
        writer.add_scalar("Accuracy/train", train_acc, epoch)
        writer.add_scalar("Accuracy/val", val_acc, epoch)
        writer.add_scalar("LR", lr_cur, epoch)

        if val_acc > best_val_acc:
            best_val_acc = val_acc
            best_epoch = epoch
            torch.save(
                {
                    "epoch": epoch,
                    "model_state_dict": model.state_dict(),
                    "optimizer_state_dict": optimizer.state_dict(),
                    "val_acc": val_acc,
                    "val_loss": val_loss,
                    "action_list": global_action_list,
                    "stats": stats,
                    "split_info": split_info,
                    "args": vars(args),
                },
                ckpt_path,
            )
            print(f"  Saved best checkpoint to {ckpt_path}")

    writer.close()

    print("\nLoading best checkpoint for final validation/test evaluation...")
    ckpt = torch.load(ckpt_path, map_location=device)
    model.load_state_dict(ckpt["model_state_dict"])

    val_loss, val_acc, val_preds, val_targets = evaluate(model, val_loader, criterion, device)
    test_loss, test_acc, test_preds, test_targets = evaluate(model, test_loader, criterion, device)

    val_effective_indices = np.unique(val_targets)
    val_effective_names = [global_action_list[i] for i in val_effective_indices]
    val_cm_effective = confusion_matrix(val_targets, val_preds, labels=val_effective_indices)
    np.save(val_cm_npy, val_cm_effective)

    test_effective_indices = np.unique(test_targets)
    test_effective_names = [global_action_list[i] for i in test_effective_indices]
    test_cm_effective = confusion_matrix(test_targets, test_preds, labels=test_effective_indices)
    np.save(test_cm_npy, test_cm_effective)

    plot_two_confusion_matrices_side_by_side(
        val_cm_effective,
        val_effective_names,
        test_cm_effective,
        test_effective_names,
        cm_pair_png,
        normalize=False,
        left_title=f"Validation (acc={val_acc:.4f})",
        right_title=f"Test (acc={test_acc:.4f})",
        suptitle=f"Confusion Matrices: {mod_name}, {args.split_mode}"
    )

    plot_two_confusion_matrices_side_by_side(
        val_cm_effective,
        val_effective_names,
        test_cm_effective,
        test_effective_names,
        cm_pair_norm_png,
        normalize=True,
        left_title=f"Validation (acc={val_acc:.4f})",
        right_title=f"Test (acc={test_acc:.4f})",
        suptitle=f"Normalized Confusion Matrices: {mod_name}, {args.split_mode}"
    )

    print("\n========== Final Validation ==========")
    print(f"Val loss   : {val_loss:.4f}")
    print(f"Val acc    : {val_acc:.4f}")

    print("\nValidation effective classes:")
    for idx in val_effective_indices:
        print(f"  {idx:2d}: {global_action_list[idx]}")

    print("\nValidation Classification Report:")
    print(classification_report(
        val_targets,
        val_preds,
        labels=val_effective_indices,
        target_names=val_effective_names,
        digits=4,
        zero_division=0
    ))

    print("\n========== Final Test ==========")
    print(f"Modalities : {mod_name}")
    print(f"Split mode : {args.split_mode}")
    print(f"Best epoch : {best_epoch}")
    print(f"Test loss  : {test_loss:.4f}")
    print(f"Test acc   : {test_acc:.4f}")

    print("\nTest effective classes:")
    for idx in test_effective_indices:
        print(f"  {idx:2d}: {global_action_list[idx]}")

    print("\nTest Classification Report:")
    print(classification_report(
        test_targets,
        test_preds,
        labels=test_effective_indices,
        target_names=test_effective_names,
        digits=4,
        zero_division=0
    ))

    per_subject_acc = compute_per_subject_accuracy(test_records, test_samples, test_preds)
    print("\nPer-subject test accuracy:")
    for person, acc in per_subject_acc.items():
        print(f"  {person}: {acc:.4f}")

    results = {
        "modalities": args.modalities,
        "split_mode": args.split_mode,
        "best_epoch": best_epoch,
        "best_val_acc_during_training": float(best_val_acc),
        "final_val_loss": float(val_loss),
        "final_val_acc": float(val_acc),
        "final_test_loss": float(test_loss),
        "final_test_acc": float(test_acc),
        "action_list": global_action_list,
        "split_info": split_info,
        "train_num_files": len(train_records),
        "val_num_files": len(val_records),
        "test_num_files": len(test_records),
        "train_num_samples": len(train_samples),
        "val_num_samples": len(val_samples),
        "test_num_samples": len(test_samples),
        "stats": stats,
        "val_effective_class_indices": val_effective_indices.tolist(),
        "val_effective_class_names": val_effective_names,
        "test_effective_class_indices": test_effective_indices.tolist(),
        "test_effective_class_names": test_effective_names,
        "per_subject_test_accuracy": per_subject_acc,
        "checkpoint_path": ckpt_path,
        "tensorboard_dir": tb_dir,
        "val_cm_effective_path": val_cm_npy,
        "test_cm_effective_path": test_cm_npy,
        "val_test_cm_side_by_side_png": cm_pair_png,
        "val_test_cm_side_by_side_normalized_png": cm_pair_norm_png,
    }
    save_json(results, result_json)

    print(f"\nSaved results to: {result_json}")
    print(f"Saved side-by-side confusion matrix to: {cm_pair_png}")
    print(f"Saved normalized side-by-side confusion matrix to: {cm_pair_norm_png}")


# ============================================================
# CLI
# ============================================================

def parse_args():
    parser = argparse.ArgumentParser()

    here = os.path.dirname(os.path.abspath(__file__))
    parser.add_argument(
        "--data_dir",
        type=str,
        default=os.path.join(here, "filtered"),
        help="Directory containing .mat files (default: ./filtered)",
    )
    parser.add_argument("--out_dir", type=str, default=os.path.join(here, "runs"))

    parser.add_argument(
        "--modalities",
        type=str,
        default="rd,ra,re",
        help="Comma-separated list from {rd,ra,re}, e.g. rd or rd,ra or rd,ra,re"
    )

    parser.add_argument("--split_mode", type=str, choices=["subject", "file", "loso"], default="file")
    parser.add_argument("--test_subject", type=str, default="", help="Required when split_mode=loso")

    parser.add_argument("--seq_len", type=int, default=16)
    parser.add_argument("--train_stride", type=int, default=4)
    parser.add_argument("--eval_stride", type=int, default=8)

    parser.add_argument("--val_ratio", type=float, default=0.15)
    parser.add_argument("--test_ratio", type=float, default=0.15)

    parser.add_argument("--batch_size", type=int, default=16)
    parser.add_argument("--epochs", type=int, default=40)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight_decay", type=float, default=1e-4)

    parser.add_argument("--branch_feature_dim", type=int, default=128)
    parser.add_argument("--fusion_dim", type=int, default=256)

    parser.add_argument("--lstm_hidden_dim", type=int, default=128)
    parser.add_argument("--lstm_layers", type=int, default=2)
    parser.add_argument("--dropout", type=float, default=0.3)
    parser.add_argument("--bidirectional", action="store_true")

    parser.add_argument("--num_workers", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--cpu", action="store_true")
    parser.add_argument("--no_log_power", action="store_true")

    args = parser.parse_args()
    args.modalities = parse_modalities(args.modalities)
    return args


if __name__ == "__main__":
    args = parse_args()
    main(args)