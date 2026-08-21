#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Compute pooled phase-std (rad) for the four phase-coherence categories:

  sensing       <- sensing/cable_data_port{1,2}_fp_mp.csv
                   (same algorithm as compute_cable_phase_fluctuation.m)
  localization  <- AoA_accuracy microbenchmark PortPair=[4,5], angles=-40:10:40
                   (MATLAB pooled_residual_std_45.m)
  uloc          <- uloc/processing phase stability, antenna 1 vs 0
  dw3000        <- DWM1002/0d_0m + 10d_0m pooled residual PDOA

Returns values rounded to 3 decimals for plotting (same display style as before).
"""

from __future__ import annotations

import csv
import json
import math
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

HERE = Path(__file__).resolve().parent
SENSING_DIR = HERE / "sensing"
LOCALIZATION_DIR = HERE / "localization"
DWM_DIR = HERE / "DWM1002"
ULOC_DIR = HERE / "uloc" / "processing"
AOA_DIR = LOCALIZATION_DIR / "AoA_accuracy(Localization)"

MATLAB_BIN = os.environ.get(
    "MATLAB_BIN", "/Applications/MATLAB_R2025a.app/bin/matlab"
)
ULOC_PYTHON = ULOC_DIR / "venv" / "bin" / "python"

# Match compute_cable_phase_fluctuation.m
SENSING_FIRST_CIR_TAP = 699
SENSING_PORT1_REL_OFFSET = 1584 / 64
SENSING_PORT2_REL_OFFSET = 1778 / 64
SENSING_ALIGNED_FRAME_RANGE = (7000, 9000)
SENSING_UPSAMPLE = 64


def _stdev(vals: list[float]) -> float:
    return statistics.stdev(vals) if len(vals) > 1 else float("nan")


def _wrap_phase(phase: np.ndarray) -> np.ndarray:
    return np.angle(np.exp(1j * phase))


def _load_sensing_port_csv(csv_file: Path) -> dict:
    """Load Sequence / firstPath / rxPreamCount / CIR from a port CSV."""
    with csv_file.open(newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise RuntimeError(f"Empty CSV: {csv_file}")
        names = list(reader.fieldnames)
        real_cols = sorted(
            [c for c in names if c.startswith("CIR_real_")],
            key=lambda c: int(c.replace("CIR_real_", "")),
        )
        imag_cols = sorted(
            [c for c in names if c.startswith("CIR_imag_")],
            key=lambda c: int(c.replace("CIR_imag_", "")),
        )
        real_idx = [int(c.replace("CIR_real_", "")) for c in real_cols]
        imag_idx = [int(c.replace("CIR_imag_", "")) for c in imag_cols]
        if real_idx != imag_idx:
            raise RuntimeError(f"CIR real/imag columns do not match in {csv_file}")
        if "firstPath" not in names or "rxPreamCount" not in names:
            raise RuntimeError(f"Missing firstPath/rxPreamCount in {csv_file}")

        sequences: list[float] = []
        first_path: list[float] = []
        rx_pream: list[float] = []
        cir_rows: list[list[complex]] = []
        has_seq = "Sequence" in names
        for i, row in enumerate(reader):
            sequences.append(float(row["Sequence"]) if has_seq else float(i))
            first_path.append(float(row["firstPath"]))
            rx_pream.append(float(row["rxPreamCount"]))
            cir_rows.append(
                [
                    complex(float(row[rc]), float(row[ic]))
                    for rc, ic in zip(real_cols, imag_cols)
                ]
            )

    return {
        "sequence": np.asarray(sequences, dtype=float),
        "first_path": np.asarray(first_path, dtype=float),
        "rx_pream_count": np.asarray(rx_pream, dtype=float),
        "cir": np.asarray(cir_rows, dtype=np.complex128),
    }


def _align_two_by_sequence(seq1: np.ndarray, seq2: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Port of align_two_by_sequence from compute_cable_phase_fluctuation.m (1-based rows → 0-based)."""
    if seq1.size == 0 or seq2.size == 0:
        return np.array([], dtype=int), np.array([], dtype=int)

    min_start_seq = int(min(seq1[0], seq2[0]))
    n_padded = max(seq1.size, seq2.size)
    seq_range = (min_start_seq + np.arange(n_padded)) % 256

    seqs = [seq1, seq2]
    ptrs = [0, 0]
    aligned: list[list[int]] = [[], []]

    for seq in seq_range:
        seq = int(seq)
        row_indices = [math.nan, math.nan]
        for port_idx in range(2):
            port_seq = seqs[port_idx]
            ptr = ptrs[port_idx]
            if ptr < port_seq.size and int(port_seq[ptr]) == seq:
                row_indices[port_idx] = ptr
                ptr += 1
            else:
                while ptr < port_seq.size:
                    seq_orig = int(port_seq[ptr])
                    forward_dist = (
                        seq_orig - seq if seq_orig >= seq else seq_orig - seq + 256
                    )
                    if forward_dist >= 128:
                        ptr += 1
                    else:
                        break
            ptrs[port_idx] = ptr
        if not any(math.isnan(x) for x in row_indices):
            aligned[0].append(int(row_indices[0]))
            aligned[1].append(int(row_indices[1]))

    return np.asarray(aligned[0], dtype=int), np.asarray(aligned[1], dtype=int)


def _phase_at_lde_and_target(
    cir_row: np.ndarray,
    rx_pream_count: float,
    lde_abs_idx: float,
    relative_offset: float,
    first_cir_tap: int = SENSING_FIRST_CIR_TAP,
    upsample: int = SENSING_UPSAMPLE,
) -> tuple[float, float]:
    """Return (phase_lde, phase_target); NaN if out of range."""
    nbins = cir_row.size
    lde_local_tap = lde_abs_idx - first_cir_tap + 1
    target_local_tap = lde_local_tap + relative_offset
    if (
        not math.isfinite(lde_local_tap)
        or lde_local_tap < 1
        or lde_local_tap > nbins
        or target_local_tap < 1
        or target_local_tap > nbins
    ):
        return math.nan, math.nan

    # resample_poly matches MATLAB resample(x, 64, 1) (FIR); FFT resample does not.
    x_up = resample_poly(cir_row, upsample, 1) / rx_pream_count
    # MATLAB 1-based indices → Python 0-based
    lde_up_idx = int(round(lde_local_tap * upsample)) - 1
    target_up_idx = lde_up_idx + int(round(relative_offset * upsample))
    if lde_up_idx < 0 or lde_up_idx >= x_up.size:
        return math.nan, math.nan
    if target_up_idx < 0 or target_up_idx >= x_up.size:
        return math.nan, math.nan
    return float(np.angle(x_up[lde_up_idx])), float(np.angle(x_up[target_up_idx]))


def compute_dw3000(data_dir: Path = DWM_DIR) -> dict:
    """Pooled residual std (rad) from DWM1002 JSONL logs or packed npy."""
    residuals: list[float] = []
    per_file = []
    curve_raw = HERE.parent / "curve_raw_npy" / "Figure11" / "dw3000" / "raw.npz"
    curve_dict = None
    if curve_raw.is_file():
        z = np.load(curve_raw)
        curve_dict = {k: z[k] for k in z.files}
    for name in ("0d_0m", "10d_0m"):
        npz_path = data_dir / "npy" / f"{name}.npz"
        json_path = data_dir / name
        vals_deg: list[float] = []
        key = f"{name}_pdoa_raw_deg"
        if curve_dict is not None and key in curve_dict:
            vals_deg = np.asarray(curve_dict[key], dtype=float).tolist()
            src_name = f"raw.npz:{key}"
        elif npz_path.is_file():
            vals_deg = np.load(npz_path)["pdoa_raw_deg"].astype(float).tolist()
            src_name = npz_path.name
        elif json_path.is_file():
            with json_path.open() as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        rec = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    p = rec.get("pdoa_raw_deg")
                    if p is not None:
                        vals_deg.append(float(p))
            src_name = json_path.name
        else:
            raise FileNotFoundError(f"Missing DWM1002 file: {name}")
        if len(vals_deg) < 2:
            raise RuntimeError(f"Too few PDOA samples in {src_name}")
        mu = statistics.mean(vals_deg)
        residuals.extend(v - mu for v in vals_deg)
        per_file.append((src_name, len(vals_deg), math.radians(_stdev(vals_deg))))
    std_rad = math.radians(_stdev(residuals))
    return {
        "name": "dw3000",
        "std_rad": std_rad,
        "n": len(residuals),
        "detail": per_file,
    }


def _load_sensing_port_npz(npz_path: Path) -> dict:
    z = np.load(npz_path, allow_pickle=False)
    return {
        "sequence": np.asarray(z["sequence"], dtype=float),
        "first_path": np.asarray(z["first_path"], dtype=float),
        "rx_pream_count": np.asarray(z["rx_pream_count"], dtype=float),
        "cir": np.asarray(z["cir"], dtype=np.complex128),
    }


def compute_sensing(data_dir: Path = SENSING_DIR) -> dict:
    """
    Std of two-port Double_Difference (rad) from raw cable CIR CSVs.

    Matches compute_cable_phase_fluctuation.m:
      relative_phase = phase(LDE+offset) - phase(LDE)
      double_difference = wrap(rel1 - rel2)
    """
    port1_csv = data_dir / "cable_data_port1_fp_mp.csv"
    port2_csv = data_dir / "cable_data_port2_fp_mp.csv"
    curve_raw = HERE.parent / "curve_raw_npy" / "Figure11" / "sensing" / "raw.npz"
    port1_npz = data_dir / "npy" / "cable_data_port1_fp_mp.npz"
    port2_npz = data_dir / "npy" / "cable_data_port2_fp_mp.npz"
    if port1_csv.is_file() and port2_csv.is_file():
        p1 = _load_sensing_port_csv(port1_csv)
        p2 = _load_sensing_port_csv(port2_csv)
        src_note = f"{port1_csv.name}+{port2_csv.name}"
    elif curve_raw.is_file():
        d = np.load(curve_raw)
        p1 = {
            "sequence": np.asarray(d["port1_sequence"], dtype=float),
            "first_path": np.asarray(d["port1_first_path"], dtype=float),
            "rx_pream_count": np.asarray(d["port1_rx_pream_count"], dtype=float),
            "cir": np.asarray(d["port1_cir"], dtype=np.complex128),
        }
        p2 = {
            "sequence": np.asarray(d["port2_sequence"], dtype=float),
            "first_path": np.asarray(d["port2_first_path"], dtype=float),
            "rx_pream_count": np.asarray(d["port2_rx_pream_count"], dtype=float),
            "cir": np.asarray(d["port2_cir"], dtype=np.complex128),
        }
        src_note = "curve_raw_npy/.../sensing/raw.npz"
    elif port1_npz.is_file() and port2_npz.is_file():
        p1 = _load_sensing_port_npz(port1_npz)
        p2 = _load_sensing_port_npz(port2_npz)
        src_note = f"{port1_npz.name}+{port2_npz.name}"
    else:
        raise FileNotFoundError(
            f"Missing sensing raw CSV/NPY under {data_dir} or curve_raw_npy"
        )
    idx1, idx2 = _align_two_by_sequence(p1["sequence"], p2["sequence"])
    if idx1.size == 0:
        raise RuntimeError("No aligned frames between sensing port 1 and port 2")

    frame_start = max(1, SENSING_ALIGNED_FRAME_RANGE[0])
    frame_end = min(SENSING_ALIGNED_FRAME_RANGE[1], int(idx1.size))
    if frame_start > frame_end:
        raise RuntimeError(
            f"Selected aligned frame range {SENSING_ALIGNED_FRAME_RANGE} "
            f"outside available 1-{idx1.size}"
        )
    # MATLAB uses 1-based aligned_frame_index
    sel = np.arange(frame_start - 1, frame_end)

    vals: list[float] = []
    for i1, i2 in zip(idx1[sel], idx2[sel]):
        ph_lde1, ph_t1 = _phase_at_lde_and_target(
            p1["cir"][i1],
            float(p1["rx_pream_count"][i1]),
            float(p1["first_path"][i1]),
            SENSING_PORT1_REL_OFFSET,
        )
        ph_lde2, ph_t2 = _phase_at_lde_and_target(
            p2["cir"][i2],
            float(p2["rx_pream_count"][i2]),
            float(p2["first_path"][i2]),
            SENSING_PORT2_REL_OFFSET,
        )
        if not all(math.isfinite(x) for x in (ph_lde1, ph_t1, ph_lde2, ph_t2)):
            continue
        dd = float(_wrap_phase(np.array([(ph_t1 - ph_lde1) - (ph_t2 - ph_lde2)]))[0])
        if math.isfinite(dd):
            vals.append(dd)

    if len(vals) < 2:
        raise RuntimeError("Too few valid Double_Difference samples for sensing")
    return {
        "name": "sensing",
        "std_rad": _stdev(vals),
        "n": len(vals),
        "detail": (
            f"{src_note}, "
            f"aligned frames {frame_start}-{frame_end}"
        ),
    }


def _load_phase_std_cache() -> dict | None:
    cache = HERE / "phase_std_cache.json"
    if not cache.is_file():
        return None
    try:
        return json.loads(cache.read_text())
    except json.JSONDecodeError:
        return None


def _decavewave_lde_v2(complex_signal: np.ndarray) -> float:
    """Port of pooled_residual_std_45.m::Decavewave_LDE_v2."""
    re = np.abs(np.real(complex_signal))
    im = np.abs(np.imag(complex_signal))
    amplitude = np.maximum(re, im) + 0.25 * np.minimum(re, im)
    max_amp_index = int(np.argmax(amplitude))  # 0-based
    gradient = np.concatenate([np.diff(amplitude), [0.0]])
    search = gradient[: max_amp_index + 1]
    max_g_index = int(np.argmax(search))  # 0-based
    # MATLAB indices are 1-based in the formula below
    max_g_matlab = max_g_index + 1
    if max_g_matlab <= 1 or max_g_matlab >= gradient.size:
        return float(max_g_matlab)  # MATLAB returns max_g_index (1-based)
    denom = gradient[max_g_index] - min(
        gradient[max_g_index - 1], gradient[max_g_index + 1]
    )
    frac_ts = 0.0 if denom == 0 else 0.5 * (
        gradient[max_g_index + 1] - gradient[max_g_index - 1]
    ) / denom
    # fp_index = max_g_index + frac_ts - 1 + 0.5  (MATLAB 1-based max_g_index)
    return float(max_g_matlab + frac_ts - 1 + 0.5)


def _seq_to_global(seq: np.ndarray) -> np.ndarray:
    """Unwrap 0–255 Sequence to monotonically increasing global index."""
    seq = np.asarray(seq, dtype=float)
    g = np.zeros(seq.size, dtype=float)
    if seq.size == 0:
        return g
    g[0] = seq[0]
    cycle = 0
    for i in range(1, seq.size):
        if seq[i] <= seq[i - 1]:
            cycle += 1
        g[i] = 256 * cycle + seq[i]
    return g


def compute_localization_from_cir_pack() -> dict:
    """
    Exact port of pooled_residual_std_45.m (PortPair=[4,5], angles=-40:10:40).

    Uses Decavewave_LDE_v2 + firstPath ratio phase, pooled residual std.
    """
    npz = HERE.parent / "curve_raw_npy" / "Figure11" / "localization" / "raw.npz"
    if not npz.is_file():
        raise FileNotFoundError(f"Missing {npz}")

    z = np.load(npz)
    cir = z["cir"]  # [2, n_ang, F, T] ports 4,5
    n_frames = z["n_frames"]
    angles = z["angles_deg"]
    seq = z["sequence"]
    first_path = z["first_path"]
    rx_pream = z["rx_pream_count"]
    first_cir_tap = 0
    upsample = 64

    residuals: list[float] = []
    n_total = 0
    for ia in range(len(angles)):
        n0 = int(n_frames[0, ia])
        n1 = int(n_frames[1, ia])
        g0 = _seq_to_global(seq[0, ia, :n0])
        g1 = _seq_to_global(seq[1, ia, :n1])
        phase_diffs: list[float] = []
        j = 0  # pointer into port5 (index 1)
        for i in range(n0):
            while j < n1 and g0[i] > g1[j]:
                j += 1
            if j >= n1:
                break
            if g0[i] != g1[j]:
                continue
            ones = []
            valid = True
            for ip, fi in ((0, i), (1, j)):
                row = cir[ip, ia, fi].astype(np.complex128)
                rx = float(rx_pream[ip, ia, fi])
                if not math.isfinite(rx) or rx == 0:
                    valid = False
                    break
                up = resample_poly(row, upsample, 1) / rx
                fp = float(first_path[ip, ia, fi]) - first_cir_tap
                fp_u = int(round(fp * upsample))  # MATLAB 1-based
                cpy = row.copy()
                z_end = min(cpy.size, int(round(fp)) + 20)
                cpy[:z_end] = 0
                sec = _decavewave_lde_v2(cpy)
                sec_u = int(round(sec * upsample))
                if (
                    fp_u < 1
                    or fp_u > up.size
                    or sec_u < 1
                    or sec_u > up.size
                    or up[sec_u - 1] == 0
                ):
                    valid = False
                    break
                ones.append(up[fp_u - 1] / up[sec_u - 1])
            if valid and len(ones) == 2:
                phase_diffs.append(float(np.angle(ones[0] / ones[1])))
            j += 1

        if len(phase_diffs) < 2:
            continue
        mu = float(np.mean(phase_diffs))
        residuals.extend(v - mu for v in phase_diffs)
        n_total += len(phase_diffs)

    if len(residuals) < 2:
        raise RuntimeError("Too few localization residuals from CIR pack")
    return {
        "name": "localization",
        "std_rad": _stdev(residuals),
        "n": len(residuals),
        "detail": f"from {npz.name} PortPair=[4,5] Decavewave (n_frames={n_total})",
    }


def compute_localization(aoa_dir: Path = AOA_DIR) -> dict:
    """Pooled residual std from AoA microbenchmark [4,5], -40:10:40."""
    prefer_cache = os.environ.get("USE_PHASE_STD_CACHE", "").lower() in (
        "1",
        "true",
        "yes",
    )
    cache = _load_phase_std_cache()
    if prefer_cache and cache and "localization" in cache.get("results", {}):
        info = cache["results"]["localization"]
        return {
            "name": "localization",
            "std_rad": float(info["std_rad"]),
            "n": info.get("n"),
            "detail": f"from phase_std_cache.json ({info.get('detail', '')})",
        }

    # Prefer CIR pack (Docker / no MATLAB)
    try:
        return compute_localization_from_cir_pack()
    except Exception as cir_exc:  # noqa: BLE001
        print(f"localization CIR pack failed ({cir_exc}); trying MATLAB/cache...")

    script = aoa_dir / "pooled_residual_std_45.m"
    try:
        if not script.is_file():
            raise FileNotFoundError(f"Missing localization script: {script}")
        aoa_str = str(aoa_dir).replace("'", "''")
        out = _run_matlab(
            f"cd('{aoa_str}'); pooled_residual_std_45;",
            cwd=aoa_dir,
        )
        m = re.search(
            r"pooled residual std\s*=\s*([0-9.]+)\s*rad",
            out,
            flags=re.IGNORECASE,
        )
        if not m:
            raise RuntimeError(
                "Could not parse localization pooled std from MATLAB output:\n"
                + out[-2000:]
            )
        n_match = re.search(
            r"pooled residual n\s*=\s*(\d+)", out, flags=re.IGNORECASE
        )
        return {
            "name": "localization",
            "std_rad": float(m.group(1)),
            "n": int(n_match.group(1)) if n_match else None,
            "detail": "PortPair=[4,5], aoa=-40:10:40",
        }
    except (FileNotFoundError, RuntimeError) as exc:
        if cache and "localization" in cache.get("results", {}):
            info = cache["results"]["localization"]
            return {
                "name": "localization",
                "std_rad": float(info["std_rad"]),
                "n": info.get("n"),
                "detail": f"cache fallback after: {exc}",
            }
        raise RuntimeError(f"localization failed: CIR={cir_exc}; MATLAB={exc}") from exc


def compute_uloc(uloc_dir: Path = ULOC_DIR) -> dict:
    """Phase std (rad) of antenna 1 vs antenna 0 on uloc zero calibration log."""
    # Fast path: precomputed relative phase array
    npy_candidates = [
        HERE.parent / "curve_raw_npy" / "Figure11" / "uloc" / "raw.npz",
        HERE / "uloc" / "npy" / "phase_rel_deg.npy",
        uloc_dir / "phase_rel_deg.npy",
    ]
    for npy_path in npy_candidates:
        if not npy_path.is_file():
            continue
        if npy_path.suffix == ".npz":
            d = np.load(npy_path)
            if "phase_rel_rad" in d.files:
                arr = np.degrees(np.asarray(d["phase_rel_rad"]))
            elif "phase_rel_deg" in d.files:
                arr = np.asarray(d["phase_rel_deg"])
            else:
                continue
        else:
            arr = np.load(npy_path)
        if arr.ndim == 2 and arr.shape[1] > 1:
            vals = np.deg2rad(arr[:, 1].astype(float))
        else:
            vals = np.deg2rad(arr.astype(float).ravel())
        if vals.size < 2:
            continue
        return {
            "name": "uloc",
            "std_rad": float(np.std(vals, ddof=1)),
            "n": int(vals.size),
            "detail": f"from {npy_path.name}",
        }

    py = ULOC_PYTHON if ULOC_PYTHON.is_file() else Path(sys.executable)
    code = r"""
import os, sys, numpy as np
sys.path.insert(0, os.getcwd())
from load import parse
from algo import cir
DATA = os.path.join(os.getcwd(), 'data', 'example_data')
zero_file = os.path.join(DATA, 'uloc_zero_0_20210427_181016_207A357A4653.log')
ap = parse.load_log(zero_file, interp_cir=8, suppress=True)
ap = parse.reformat_ap_data(ap_data=ap, interp_cir=8, select_tag_addr='0000')
ap = cir.extract_fp(ap)
cir_fp = ap['cir_fp']
phase_rel = np.angle(cir_fp / cir_fp[:, [0]])
vals = phase_rel[:, 1]
std = float(np.std(vals, ddof=1))
print(f'ULOC_STD_RAD={std:.10f}')
print(f'ULOC_N={len(vals)}')
"""
    proc = subprocess.run(
        [str(py), "-c", code],
        cwd=str(uloc_dir),
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            "ULoc phase_stability failed:\n"
            + (proc.stderr or proc.stdout)[-2000:]
        )
    m = re.search(r"ULOC_STD_RAD=([0-9.eE+-]+)", proc.stdout)
    n_m = re.search(r"ULOC_N=(\d+)", proc.stdout)
    if not m:
        raise RuntimeError("Could not parse ULoc std:\n" + proc.stdout[-2000:])
    return {
        "name": "uloc",
        "std_rad": float(m.group(1)),
        "n": int(n_m.group(1)) if n_m else None,
        "detail": "antenna 1 vs 0 on uloc_zero log",
    }


def _run_matlab(batch_cmd: str, cwd: Path) -> str:
    if not Path(MATLAB_BIN).is_file():
        raise FileNotFoundError(
            f"MATLAB not found at {MATLAB_BIN}. Set MATLAB_BIN env var."
        )
    proc = subprocess.run(
        [MATLAB_BIN, "-batch", batch_cmd],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        check=False,
    )
    out = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0:
        raise RuntimeError(
            f"MATLAB failed (exit {proc.returncode}):\n{out[-3000:]}"
        )
    return out


def compute_all(prefer_cache: bool | None = None) -> dict[str, float]:
    """
    Returns dict with keys sensing, localization, uloc, dw3000 (rad).
    Print a summary table.
    """
    if prefer_cache is None:
        prefer_cache = os.environ.get("USE_PHASE_STD_CACHE", "").lower() in (
            "1",
            "true",
            "yes",
        )
    if prefer_cache:
        cache = _load_phase_std_cache()
        if cache and "plot_values" in cache:
            results = {k: float(v) for k, v in cache["plot_values"].items()}
            print("Loaded phase stds from phase_std_cache.json")
            for name in ("sensing", "localization", "uloc", "dw3000"):
                print(f"{name:<14} {results[name]:10.6f}")
            return results

    results = {}
    order = [
        ("sensing", compute_sensing),
        ("localization", compute_localization),
        ("uloc", compute_uloc),
        ("dw3000", compute_dw3000),
    ]
    print(f"{'category':<14} {'std(rad)':>10} {'n':>8}  note")
    print("-" * 72)
    for name, fn in order:
        info = fn()
        results[name] = info["std_rad"]
        n = info.get("n")
        n_s = f"{n}" if n is not None else "-"
        print(
            f"{name:<14} {info['std_rad']:10.6f} {n_s:>8}  {info.get('detail', '')}"
        )
    return results


def values_for_plot(results: dict[str, float] | None = None, digits: int = 3) -> list[float]:
    """Ordered list matching plot labels: sensing, localization, uloc, dw3000."""
    if results is None:
        results = compute_all()
    keys = ["sensing", "localization", "uloc", "dw3000"]
    return [round(results[k], digits) for k in keys]


if __name__ == "__main__":
    compute_all()
