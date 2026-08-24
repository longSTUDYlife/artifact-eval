#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Pack per-curve RAW measurement data → one NumPy file per curve folder.

Layout:
  curve_raw_npy/
    <figure>/<curve_id>/raw.npz   # exactly one file per curve
    manifest.json

Load:
  d = np.load('.../raw.npz')
  cir = d['cir']
"""

from __future__ import annotations

import csv
import json
import shutil
import sys
from pathlib import Path

import numpy as np

from extra_env_config import LOC_ENVS, SENSE_ENVS, loc_filename, sense_filename

HERE = Path(__file__).resolve().parent
OUT = HERE / "curve_raw_npy"
RAW_NAME = "raw.npz"  # one compressed NumPy archive per curve folder
PORTS_8 = list(range(1, 9))


def _save_curve(figure: str, curve: str, arrays: dict, manifest: dict) -> Path:
    curve_dir = OUT / figure / curve
    if curve_dir.exists():
        shutil.rmtree(curve_dir)
    curve_dir.mkdir(parents=True)
    out = curve_dir / RAW_NAME
    # notes as 0-d unicode arrays so everything is npz-friendly (no pickle)
    packed = {}
    for k, v in arrays.items():
        if isinstance(v, str):
            packed[k] = np.asarray(v)
        else:
            packed[k] = np.asarray(v)
    np.savez_compressed(out, **packed)
    info = {
        "figure": figure,
        "curve": curve,
        "file": str(out.relative_to(OUT)),
        "bytes": out.stat().st_size,
        "keys": sorted(packed.keys()),
    }
    manifest["curves"].append(info)
    print(f"  {figure}/{curve}/{RAW_NAME}  ({out.stat().st_size/1e6:.1f} MB)")
    return out


def _register_existing(figure: str, curve: str, manifest: dict) -> Path:
    out = OUT / figure / curve / RAW_NAME
    if not out.is_file():
        raise FileNotFoundError(out)
    info = {
        "figure": figure,
        "curve": curve,
        "file": str(out.relative_to(OUT)),
        "bytes": out.stat().st_size,
        "note": "existing pack kept",
    }
    manifest["curves"].append(info)
    print(f"  {figure}/{curve}/{RAW_NAME}  (kept, {out.stat().st_size/1e6:.1f} MB)")
    return out


def _hardlink_curve(figure: str, curve: str, src: Path, manifest: dict) -> Path:
    curve_dir = OUT / figure / curve
    if curve_dir.exists():
        shutil.rmtree(curve_dir)
    curve_dir.mkdir(parents=True)
    out = curve_dir / RAW_NAME
    try:
        out.hardlink_to(src)
    except OSError:
        shutil.copy2(src, out)
    info = {
        "figure": figure,
        "curve": curve,
        "file": str(out.relative_to(OUT)),
        "bytes": out.stat().st_size,
        "note": f"same file as {src.parent.name}/{RAW_NAME}",
    }
    manifest["curves"].append(info)
    print(f"  {figure}/{curve}/{RAW_NAME}  (shared → {src.parent.name})")
    return out


def _load_cir_csv(csv_path: Path) -> dict:
    try:
        import pandas as pd

        df = pd.read_csv(csv_path)
        names = list(df.columns)
        real_cols = sorted(
            [c for c in names if c.startswith("CIR_real_")],
            key=lambda c: int(c.replace("CIR_real_", "")),
        )
        imag_cols = sorted(
            [c for c in names if c.startswith("CIR_imag_")],
            key=lambda c: int(c.replace("CIR_imag_", "")),
        )
        cir = df[real_cols].to_numpy(np.float32) + 1j * df[imag_cols].to_numpy(np.float32)
        out = {
            "cir": cir.astype(np.complex64),
            "sequence": (
                df["Sequence"].to_numpy(np.float64) if "Sequence" in names else np.arange(len(df), dtype=np.float64)
            ),
        }
        if "firstPath" in names:
            out["first_path"] = df["firstPath"].to_numpy(np.float64)
        if "rxPreamCount" in names:
            out["rx_pream_count"] = df["rxPreamCount"].to_numpy(np.float64)
        if "firstPathAmp2" in names:
            out["first_path_amp2"] = df["firstPathAmp2"].to_numpy(np.float64)
        if "PacketType" in names:
            out["packet_type"] = df["PacketType"].to_numpy(np.int32)
        if "firstPathAmp1" in names:
            out["first_path_amp1"] = df["firstPathAmp1"].to_numpy(np.float64)
        return out
    except ImportError:
        pass

    with csv_path.open(newline="") as f:
        reader = csv.DictReader(f)
        if not reader.fieldnames:
            raise ValueError(f"Empty CSV: {csv_path}")
        names = list(reader.fieldnames)
        real_cols = sorted(
            [c for c in names if c.startswith("CIR_real_")],
            key=lambda c: int(c.replace("CIR_real_", "")),
        )
        imag_cols = sorted(
            [c for c in names if c.startswith("CIR_imag_")],
            key=lambda c: int(c.replace("CIR_imag_", "")),
        )
        seq, fp, rx, cir = [], [], [], []
        amp2, ptype, amp1 = [], [], []
        has_seq = "Sequence" in names
        has_fp = "firstPath" in names
        has_rx = "rxPreamCount" in names
        has_amp2 = "firstPathAmp2" in names
        has_ptype = "PacketType" in names
        has_amp1 = "firstPathAmp1" in names
        for i, row in enumerate(reader):
            seq.append(float(row["Sequence"]) if has_seq else float(i))
            if has_fp:
                fp.append(float(row["firstPath"]))
            if has_rx:
                rx.append(float(row["rxPreamCount"]))
            if has_amp2:
                amp2.append(float(row["firstPathAmp2"]))
            if has_ptype:
                ptype.append(int(float(row["PacketType"])))
            if has_amp1:
                amp1.append(float(row["firstPathAmp1"]))
            re_v = np.fromiter(
                (float(row[c]) for c in real_cols), dtype=np.float32, count=len(real_cols)
            )
            im_v = np.fromiter(
                (float(row[c]) for c in imag_cols), dtype=np.float32, count=len(imag_cols)
            )
            cir.append(re_v + 1j * im_v)
    out = {
        "cir": np.asarray(cir, dtype=np.complex64),
        "sequence": np.asarray(seq, dtype=np.float64),
    }
    if fp:
        out["first_path"] = np.asarray(fp, dtype=np.float64)
    if rx:
        out["rx_pream_count"] = np.asarray(rx, dtype=np.float64)
    if amp2:
        out["first_path_amp2"] = np.asarray(amp2, dtype=np.float64)
    if ptype:
        out["packet_type"] = np.asarray(ptype, dtype=np.int32)
    if amp1:
        out["first_path_amp1"] = np.asarray(amp1, dtype=np.float64)
    return out


def _stack_cir_grid(ports: list[int], angles: list[int], load_fn) -> dict:
    """Stack CIR into [port, angle, frame, tap] with n_frames mask (pad with 0)."""
    blocks = {}
    max_frames = 0
    n_taps = None
    for port in ports:
        for ang in angles:
            blk = load_fn(port, ang)
            blocks[(port, ang)] = blk
            max_frames = max(max_frames, blk["cir"].shape[0])
            if n_taps is None:
                n_taps = int(blk["cir"].shape[1])

    cir = np.zeros((len(ports), len(angles), max_frames, n_taps), dtype=np.complex64)
    sequence = np.zeros((len(ports), len(angles), max_frames), dtype=np.float64)
    first_path = np.zeros_like(sequence)
    rx_pream = np.zeros_like(sequence)
    n_frames = np.zeros((len(ports), len(angles)), dtype=np.int32)
    has_rx = False

    for ip, port in enumerate(ports):
        for ia, ang in enumerate(angles):
            blk = blocks[(port, ang)]
            n = blk["cir"].shape[0]
            n_frames[ip, ia] = n
            cir[ip, ia, :n] = blk["cir"]
            sequence[ip, ia, :n] = blk["sequence"]
            if "first_path" in blk:
                first_path[ip, ia, :n] = blk["first_path"]
            if "rx_pream_count" in blk:
                rx_pream[ip, ia, :n] = blk["rx_pream_count"]
                has_rx = True

    out = {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports, dtype=np.int32),
        "angles_deg": np.asarray(angles, dtype=np.float64),
        "sequence": sequence,
        "first_path": first_path,
    }
    if has_rx:
        out["rx_pream_count"] = rx_pream
    return out


def pack_figure10ab(manifest: dict) -> None:
    fig = "Figure10ab"
    raw_dir = HERE / "Figure10ab" / "raw"
    print(f"=== {fig}: ULA (shared by 8/4/2RX) ===")
    ports = list(range(1, 9))
    angles = [-40, -30, -20, -10, 0, 10, 20, 30, 40]

    def load_ula(port: int, ang: int) -> dict:
        name = f"antenna_data_port{port}_8ports_concurrent_localization_aoa_accuracy_{ang}.csv"
        print(f"    {name}")
        return _load_cir_csv(raw_dir / name)

    payload = _stack_cir_grid(ports, angles, load_ula)
    payload["note"] = (
        "Shared multiport CIR for 8RX/4RX/2RX-ULA; MATLAB selects port subsets."
    )
    src = _save_curve(fig, "8RX-ULA", payload, manifest)
    _hardlink_curve(fig, "4RX-ULA", src, manifest)
    _hardlink_curve(fig, "2RX-ULA", src, manifest)

    print(f"=== {fig}: DW3000 ===")
    cols_data: dict[str, list[np.ndarray]] = {}
    angles_dw = []
    for ang in (0, 10, 20, 30, 40):
        csv_path = HERE / "Figure10ab" / f"pdoa_data_{ang}d.csv"
        if not csv_path.is_file():
            continue
        with csv_path.open(newline="") as f:
            rows = list(csv.DictReader(f))
        angles_dw.append(ang)
        for c in rows[0].keys() if rows else []:
            try:
                arr = np.asarray([float(r[c]) for r in rows], dtype=np.float64)
            except ValueError:
                continue
            cols_data.setdefault(c, []).append(arr)
    max_n = max(len(v[0]) for v in cols_data.values())
    stacked = {"angles_deg": np.asarray(angles_dw, dtype=np.float64)}
    for c, parts in cols_data.items():
        mat = np.full((len(parts), max_n), np.nan, dtype=np.float64)
        ns = np.zeros(len(parts), dtype=np.int32)
        for i, a in enumerate(parts):
            mat[i, : a.size] = a
            ns[i] = a.size
        stacked[c] = mat
        stacked[f"n_{c}"] = ns
    _save_curve(fig, "DW3000", stacked, manifest)


def pack_dynamic_range(manifest: dict) -> None:
    fig = "Figure12"
    csv_path = HERE / "Figure12" / "dynamic_range_all_groups.csv"
    print(f"=== {fig} ===")
    with csv_path.open(newline="") as f:
        rows = list(csv.DictReader(f))
    range_m = np.asarray([float(r["range_m"]) for r in rows], dtype=np.float64)
    group = np.asarray([float(r["group"]) for r in rows], dtype=np.float64)
    _save_curve(
        fig,
        "SCR",
        {
            "range_m": range_m,
            "DR_dB": np.asarray([float(r["DR_scr_dB"]) for r in rows], dtype=np.float64),
            "group": group,
            "note": "Per-window samples WITH SCR (before sliding-window agg)",
        },
        manifest,
    )
    _save_curve(
        fig,
        "noSCR",
        {
            "range_m": range_m,
            "DR_dB": np.asarray([float(r["DR_raw_dB"]) for r in rows], dtype=np.float64),
            "group": group,
            "note": "Per-window samples WITHOUT SCR (before sliding-window agg)",
        },
        manifest,
    )


def _parse_complex_list(path: Path) -> np.ndarray:
    vals = []
    for line in path.read_text().splitlines():
        s = line.strip().replace(" ", "")
        if not s:
            continue
        vals.append(complex(s.replace("i", "j")))
    return np.asarray(vals, dtype=np.complex128)


def pack_figure10c(manifest: dict) -> None:
    fig = "Figure10c"
    raw_dir = HERE / "Figure10c" / "raw"
    print(f"=== {fig}: ULA (shared by 8/4/2RX, env1 trials 1–3) ===")
    ports = list(range(1, 9))
    angles = [-40, -30, -20, -10, 0, 10, 20, 30, 40]
    trials = [1, 2, 3]
    blocks = {}
    max_frames = 0
    n_taps = None
    for port in ports:
        for ang in angles:
            for trial in trials:
                name = f"antenna_data_port{port}_8ports_sensing_env1_{ang}_{trial}.csv"
                print(f"    {name}")
                blk = _load_cir_csv(raw_dir / name)
                blocks[(port, ang, trial)] = blk
                max_frames = max(max_frames, blk["cir"].shape[0])
                if n_taps is None:
                    n_taps = int(blk["cir"].shape[1])

    cir = np.zeros((len(ports), len(angles), len(trials), max_frames, n_taps), dtype=np.complex64)
    first_path = np.zeros((len(ports), len(angles), len(trials), max_frames), dtype=np.float64)
    rx_pream = np.zeros_like(first_path)
    amp2 = np.zeros_like(first_path)
    ptype = np.zeros((len(ports), len(angles), len(trials), max_frames), dtype=np.int32)
    n_frames = np.zeros((len(ports), len(angles), len(trials)), dtype=np.int32)
    for ip, port in enumerate(ports):
        for ia, ang in enumerate(angles):
            for it, trial in enumerate(trials):
                blk = blocks[(port, ang, trial)]
                n = blk["cir"].shape[0]
                n_frames[ip, ia, it] = n
                cir[ip, ia, it, :n] = blk["cir"]
                if "first_path" in blk:
                    first_path[ip, ia, it, :n] = blk["first_path"]
                if "rx_pream_count" in blk:
                    rx_pream[ip, ia, it, :n] = blk["rx_pream_count"]
                if "first_path_amp2" in blk:
                    amp2[ip, ia, it, :n] = blk["first_path_amp2"]
                if "packet_type" in blk:
                    ptype[ip, ia, it, :n] = blk["packet_type"]

    calib = _parse_complex_list(raw_dir / "spatial_phase_avg_complex_v3_angle0.csv")
    payload = {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports, dtype=np.int32),
        "angles_deg": np.asarray(angles, dtype=np.float64),
        "trials": np.asarray(trials, dtype=np.int32),
        "first_path": first_path,
        "rx_pream_count": rx_pream,
        "first_path_amp2": amp2,
        "packet_type": ptype,
        "calib": calib,
        "note": (
            "202603122 env1 CIR [port, angle, trial, frame, tap]; "
            "sync on firstPathAmp2 after PacketType==1; calib=spatial_phase_avg_complex_v3_angle0"
        ),
    }
    src = _save_curve(fig, "8RX-ULA", payload, manifest)
    _hardlink_curve(fig, "4RX-ULA", src, manifest)
    _hardlink_curve(fig, "2RX-ULA", src, manifest)


def _stack_loc_cells(env: int) -> dict:
    cfg = LOC_ENVS[env]
    cells = list(cfg["cells"])
    blocks = {}
    max_frames = 0
    n_taps = None
    for port in PORTS_8:
        for ang, dist in cells:
            path = loc_filename(env, port, ang, dist)
            if not path.is_file():
                raise FileNotFoundError(path)
            print(f"    {path.name}")
            blk = _load_cir_csv(path)
            blocks[(port, ang, dist)] = blk
            max_frames = max(max_frames, blk["cir"].shape[0])
            if n_taps is None:
                n_taps = int(blk["cir"].shape[1])
    n_cells = len(cells)
    cir = np.zeros((len(PORTS_8), n_cells, max_frames, n_taps), dtype=np.complex64)
    sequence = np.zeros((len(PORTS_8), n_cells, max_frames), dtype=np.float64)
    ptype = np.zeros_like(sequence, dtype=np.int32)
    amp1 = np.zeros_like(sequence)
    n_frames = np.zeros((len(PORTS_8), n_cells), dtype=np.int32)
    for ip, port in enumerate(PORTS_8):
        for ic, (ang, dist) in enumerate(cells):
            blk = blocks[(port, ang, dist)]
            n = blk["cir"].shape[0]
            n_frames[ip, ic] = n
            cir[ip, ic, :n] = blk["cir"]
            sequence[ip, ic, :n] = blk["sequence"]
            if "packet_type" in blk:
                ptype[ip, ic, :n] = blk["packet_type"]
            if "first_path_amp1" in blk:
                amp1[ip, ic, :n] = blk["first_path_amp1"]
    return {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(PORTS_8, dtype=np.int32),
        "angles_deg": np.asarray([c[0] for c in cells], dtype=np.float64),
        "dists_m": np.asarray([c[1] for c in cells], dtype=np.float64),
        "sequence": sequence,
        "packet_type": ptype,
        "first_path_amp1": amp1,
        "phase_deg": np.asarray(cfg["phase_deg"], dtype=np.float64),
        "paper_env": np.int32(env),
        "note": cfg["note"],
    }


def _stack_sense_pairs(env: int) -> dict:
    cfg = SENSE_ENVS[env]
    pairs = list(cfg["pairs"])
    blocks = {}
    max_frames = 0
    n_taps = None
    for port in PORTS_8:
        for ang, trial in pairs:
            path = sense_filename(env, port, ang, trial)
            if not path.is_file():
                raise FileNotFoundError(path)
            print(f"    {path.name}")
            blk = _load_cir_csv(path)
            blocks[(port, ang, trial)] = blk
            max_frames = max(max_frames, blk["cir"].shape[0])
            if n_taps is None:
                n_taps = int(blk["cir"].shape[1])
    n_pairs = len(pairs)
    cir = np.zeros((len(PORTS_8), n_pairs, max_frames, n_taps), dtype=np.complex64)
    first_path = np.zeros((len(PORTS_8), n_pairs, max_frames), dtype=np.float64)
    rx_pream = np.zeros_like(first_path)
    amp2 = np.zeros_like(first_path)
    ptype = np.zeros((len(PORTS_8), n_pairs, max_frames), dtype=np.int32)
    n_frames = np.zeros((len(PORTS_8), n_pairs), dtype=np.int32)
    for ip, port in enumerate(PORTS_8):
        for iq, (ang, trial) in enumerate(pairs):
            blk = blocks[(port, ang, trial)]
            n = blk["cir"].shape[0]
            n_frames[ip, iq] = n
            cir[ip, iq, :n] = blk["cir"]
            if "first_path" in blk:
                first_path[ip, iq, :n] = blk["first_path"]
            if "rx_pream_count" in blk:
                rx_pream[ip, iq, :n] = blk["rx_pream_count"]
            if "first_path_amp2" in blk:
                amp2[ip, iq, :n] = blk["first_path_amp2"]
            if "packet_type" in blk:
                ptype[ip, iq, :n] = blk["packet_type"]
    calib = _parse_complex_list(cfg["calib"])
    return {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(PORTS_8, dtype=np.int32),
        "angles_deg": np.asarray([p[0] for p in pairs], dtype=np.float64),
        "trials": np.asarray([p[1] for p in pairs], dtype=np.int32),
        "first_path": first_path,
        "rx_pream_count": rx_pream,
        "first_path_amp2": amp2,
        "packet_type": ptype,
        "calib": calib,
        "paper_env": np.int32(env),
        "sync": cfg["sync"],
        "extract": cfg["extract"],
        "min_range_m": np.float64(cfg["min_range_m"]),
        "skip_windows": np.int32(cfg["skip_windows"]),
        "keep_windows": np.int32(cfg["keep_windows"]),
        "note": cfg["note"],
    }


def pack_loc_extra_envs(manifest: dict) -> None:
    fig = "Figure13a"
    for env in (2, 3, 4):
        print(f"=== {fig}: Env-{env} kept localization cells ===")
        payload = _stack_loc_cells(env)
        _save_curve(fig, f"env{env}", payload, manifest)


def pack_sense_extra_envs(manifest: dict) -> None:
    fig = "Figure13e"
    for env in (2, 3, 4):
        print(f"=== {fig}: Env-{env} kept sensing trials ===")
        payload = _stack_sense_pairs(env)
        _save_curve(fig, f"env{env}", payload, manifest)


def pack_figure13e(manifest: dict) -> None:
    fig = "Figure13e"
    src = OUT / "Figure10c" / "8RX-ULA" / RAW_NAME
    env1 = OUT / fig / "8RX-ULA" / RAW_NAME
    if env1.is_file():
        print(f"=== {fig}: keep existing Env-1 pack ===")
        _register_existing(fig, "8RX-ULA", manifest)
    else:
        if not src.is_file():
            print(f"=== {fig}: Figure10c pack missing, packing it first ===")
            extra: dict = {"curves": []}
            pack_figure10c(extra)
            src = OUT / "Figure10c" / "8RX-ULA" / RAW_NAME
            have = {(c.get("figure"), c.get("curve")) for c in manifest["curves"]}
            for c in extra["curves"]:
                if (c.get("figure"), c.get("curve")) not in have:
                    manifest["curves"].append(c)
        print(f"=== {fig}: same CIR as Figure10c (8RX env1 track) ===")
        _hardlink_curve(fig, "8RX-ULA", src, manifest)
    pack_sense_extra_envs(manifest)


def pack_figure10d(manifest: dict) -> None:
    fig = "Figure10d"
    raw_dir = HERE / "Figure10d" / "raw"
    print(f"=== {fig}: ULA (shared by 8/4/2RX, tworef aoa=0 times=3) ===")
    ports = list(range(1, 9))
    blocks = []
    max_frames = 0
    n_taps = None
    for port in ports:
        name = f"antenna_data_port{port}_8ports_sensing_car_square_tworef_0_3.csv"
        print(f"    {name}")
        blk = _load_cir_csv(raw_dir / name)
        blocks.append(blk)
        max_frames = max(max_frames, blk["cir"].shape[0])
        if n_taps is None:
            n_taps = int(blk["cir"].shape[1])
    cir = np.zeros((len(ports), max_frames, n_taps), dtype=np.complex64)
    first_path = np.zeros((len(ports), max_frames), dtype=np.float64)
    rx_pream = np.zeros_like(first_path)
    n_frames = np.zeros(len(ports), dtype=np.int32)
    for ip, blk in enumerate(blocks):
        n = blk["cir"].shape[0]
        n_frames[ip] = n
        cir[ip, :n] = blk["cir"]
        if "first_path" in blk:
            first_path[ip, :n] = blk["first_path"]
        if "rx_pream_count" in blk:
            rx_pream[ip, :n] = blk["rx_pream_count"]
    calib = _parse_complex_list(raw_dir / "calibration.csv")
    payload = {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports, dtype=np.int32),
        "first_path": first_path,
        "rx_pream_count": rx_pream,
        "calib": calib,
        "target_win": np.int32(14),
        "target_range_m": np.float64(3.1828125),
        "note": (
            "20260218 square two-corner-reflectors CIR [port, frame, tap]; "
            "row-aligned; 4RX=ports 1-4, 2RX=ports 1-2; FFT RA window 14 @ ~3.18 m"
        ),
    }
    src = _save_curve(fig, "8RX-ULA", payload, manifest)
    _hardlink_curve(fig, "4RX-ULA", src, manifest)
    _hardlink_curve(fig, "2RX-ULA", src, manifest)


def pack_figure13a(manifest: dict) -> None:
    fig = "Figure13a"
    raw_dir = HERE / "Figure13a" / "raw"
    env1 = OUT / fig / "8RX-ULA" / RAW_NAME
    if env1.is_file():
        print(f"=== {fig}: keep existing Env-1 pack ===")
        _register_existing(fig, "8RX-ULA", manifest)
        pack_loc_extra_envs(manifest)
        return
    print(f"=== {fig}: 8RX env1 localization grid ===")
    ports = list(range(1, 9))
    angles = [-40, -30, -20, -10, 0, 10, 20, 30, 40]
    dists = [1, 2, 3, 4]
    blocks = {}
    max_frames = 0
    n_taps = None
    for port in ports:
        for ang in angles:
            for dist in dists:
                name = f"antenna_data_port{port}_8ports_concurrent_localization_accuracy_{ang}_{dist}.csv"
                print(f"    {name}")
                blk = _load_cir_csv(raw_dir / name)
                blocks[(port, ang, dist)] = blk
                max_frames = max(max_frames, blk["cir"].shape[0])
                if n_taps is None:
                    n_taps = int(blk["cir"].shape[1])

    cir = np.zeros((len(ports), len(angles), len(dists), max_frames, n_taps), dtype=np.complex64)
    sequence = np.zeros((len(ports), len(angles), len(dists), max_frames), dtype=np.float64)
    ptype = np.zeros((len(ports), len(angles), len(dists), max_frames), dtype=np.int32)
    amp1 = np.zeros_like(sequence)
    n_frames = np.zeros((len(ports), len(angles), len(dists)), dtype=np.int32)
    for ip, port in enumerate(ports):
        for ia, ang in enumerate(angles):
            for idd, dist in enumerate(dists):
                blk = blocks[(port, ang, dist)]
                n = blk["cir"].shape[0]
                n_frames[ip, ia, idd] = n
                cir[ip, ia, idd, :n] = blk["cir"]
                sequence[ip, ia, idd, :n] = blk["sequence"]
                if "packet_type" in blk:
                    ptype[ip, ia, idd, :n] = blk["packet_type"]
                if "first_path_amp1" in blk:
                    amp1[ip, ia, idd, :n] = blk["first_path_amp1"]

    payload = {
        "cir": cir,
        "n_frames": n_frames,
        "ports": np.asarray(ports, dtype=np.int32),
        "angles_deg": np.asarray(angles, dtype=np.float64),
        "dists_m": np.asarray(dists, dtype=np.float64),
        "sequence": sequence,
        "packet_type": ptype,
        "first_path_amp1": amp1,
        "note": (
            "20260209 env1 CIR [port, angle, dist, frame, tap]; "
            "PacketType!=1; distance=firstPathAmp1 (cm); 8RX-ULA only"
        ),
    }
    _save_curve(fig, "8RX-ULA", payload, manifest)
    pack_loc_extra_envs(manifest)


def pack_phase_coherance(manifest: dict) -> None:
    fig = "Figure11"

    print(f"=== {fig}: sensing ===")
    sense = {}
    for port, name in ((1, "cable_data_port1_fp_mp.csv"), (2, "cable_data_port2_fp_mp.csv")):
        blk = _load_cir_csv(HERE / "Figure11" / "sensing" / name)
        sense[f"port{port}_cir"] = blk["cir"]
        sense[f"port{port}_sequence"] = blk["sequence"]
        if "first_path" in blk:
            sense[f"port{port}_first_path"] = blk["first_path"]
        if "rx_pream_count" in blk:
            sense[f"port{port}_rx_pream_count"] = blk["rx_pream_count"]
    _save_curve(fig, "sensing", sense, manifest)

    print(f"=== {fig}: localization ===")
    loc_src = HERE / "Figure11" / "localization" / "AoA_accuracy(Localization)"
    ports = [4, 5]
    angles = list(range(-40, 50, 10))

    def load_loc(port: int, ang: int) -> dict:
        name = f"antenna_data_port{port}_8ports_concurrent_localization_aoa_accuracy_{ang}.csv"
        print(f"    {name}")
        return _load_cir_csv(loc_src / name)

    loc = _stack_cir_grid(ports, angles, load_loc)
    loc["note"] = "PortPair=[4,5], aoa=-40:10:40"
    _save_curve(fig, "localization", loc, manifest)

    print(f"=== {fig}: uloc ===")
    uloc_log = (
        HERE
        / "Figure11"
        / "uloc"
        / "processing"
        / "data"
        / "example_data"
        / "uloc_zero_0_20210427_181016_207A357A4653.log"
    )
    uloc: dict = {"source_log": uloc_log.name}
    try:
        for _alias, _typ in (("complex", complex), ("float", float), ("int", int), ("bool", bool)):
            if not hasattr(np, _alias):
                setattr(np, _alias, _typ)
        sys.path.insert(0, str(HERE / "Figure11" / "uloc" / "processing"))
        from load import parse  # type: ignore
        from algo import cir as cir_mod  # type: ignore

        ap = parse.load_log(str(uloc_log), interp_cir=8, suppress=True)
        ap = parse.reformat_ap_data(ap_data=ap, interp_cir=8, select_tag_addr="0000")
        ap = cir_mod.extract_fp(ap)
        cir_fp = np.asarray(ap["cir_fp"])
        uloc["cir_fp"] = cir_fp.astype(np.complex64)
        uloc["phase_rel_rad"] = np.angle(cir_fp / cir_fp[:, [0]]).astype(np.float64)
        uloc["note"] = "cir_fp from uloc_zero log"
        print(f"    cir_fp {cir_fp.shape}")
    except Exception as exc:  # noqa: BLE001
        legacy = HERE / "Figure11" / "uloc" / "processing" / "phase_rel_deg.npy"
        if legacy.is_file():
            uloc["phase_rel_deg"] = np.load(legacy).astype(np.float64)
            uloc["note"] = f"legacy phase_rel_deg ({exc})"
        else:
            uloc["note"] = f"parse failed: {exc}"
        print(f"    fallback: {uloc['note'][:80]}")
    _save_curve(fig, "uloc", uloc, manifest)

    print(f"=== {fig}: dw3000 ===")
    dw = {}
    for name in ("0d_0m", "10d_0m"):
        src = HERE / "Figure11" / "DWM1002" / name
        pdoa = []
        with src.open() as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    rec = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if rec.get("pdoa_raw_deg") is not None:
                    pdoa.append(float(rec["pdoa_raw_deg"]))
        dw[f"{name}_pdoa_raw_deg"] = np.asarray(pdoa, dtype=np.float64)
        print(f"    {name}: N={len(pdoa)}")
    _save_curve(fig, "dw3000", dw, manifest)


PACKERS = {
    "Figure10ab": pack_figure10ab,
    "Figure10c": pack_figure10c,
    "Figure10d": pack_figure10d,
    "Figure11": pack_phase_coherance,
    "Figure12": pack_dynamic_range,
    "Figure13a": pack_figure13a,
    "Figure13e": pack_figure13e,
}

PACKER_ALIASES = {
    "phase_coherance": "Figure11",
    "phase": "Figure11",
    "11": "Figure11",
    "figure11": "Figure11",
    "dynamic_range": "Figure12",
    "dr": "Figure12",
    "12": "Figure12",
    "figure12": "Figure12",
    "Figure11a": "Figure13a",
    "11a": "Figure13a",
    "figure13a": "Figure13a",
    "Figure12a": "Figure13e",
    "12a": "Figure13e",
    "figure13e": "Figure13e",
}


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    only = None
    if "--only" in argv:
        i = argv.index("--only")
        only = [PACKER_ALIASES.get(x.strip(), x.strip()) for x in argv[i + 1].split(",") if x.strip()]
        unknown = [x for x in only if x not in PACKERS]
        if unknown:
            raise SystemExit(f"Unknown figure(s): {unknown}. Choose from {list(PACKERS)}")

    targets = list(PACKERS) if not only else only
    OUT.mkdir(parents=True, exist_ok=True)
    man_path = OUT / "manifest.json"
    curves: list = []
    if only and man_path.is_file():
        prev = json.loads(man_path.read_text())
        drop = set(targets)
        curves = [c for c in prev.get("curves", []) if c.get("figure") not in drop]

    manifest: dict = {"root": "curve_raw_npy", "file_name": RAW_NAME, "curves": curves}
    for name in targets:
        PACKERS[name](manifest)

    seen_ino: set[int] = set()
    unique = 0
    for c in manifest["curves"]:
        p = OUT / c["file"]
        ino = p.stat().st_ino
        if ino not in seen_ino:
            seen_ino.add(ino)
            unique += c["bytes"]
    manifest["n_curves"] = len(manifest["curves"])
    manifest["total_bytes_unique"] = unique
    man_path.write_text(json.dumps(manifest, indent=2))
    print()
    print(f"Curves: {manifest['n_curves']}, unique ~{unique/1e6:.1f} MB")
    print(f"Each curve folder contains only: {RAW_NAME}")
    print("Load: d = np.load('.../raw.npz'); cir = d['cir']")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
