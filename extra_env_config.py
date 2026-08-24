#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fig. 13 optional Env-2/3/4 (paper labels). Only kept cells / trials are packed.

Filename tags on disk do not match paper labels:
  sensing Env-2 files are tagged env3 (folder 202602282)
  sensing Env-3 files are tagged env2 (folder 20260228)
"""

from __future__ import annotations

from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
MP = REPO / "Realtime_collection" / "MultiPort"

# Paper-reported localization (MATLAB, 8RX MVDR + refit). Python from CIR may
# differ slightly because LDE is re-extracted.
LOC_PAPER = {
    2: {"n": 15450, "median_m": 0.0521, "p90_m": 0.1024, "rmse_m": 0.0662},
    3: {"n": 6760, "median_m": 0.0692, "p90_m": 0.1212, "rmse_m": 0.0810},
    4: {"n": 9354, "median_m": 0.057, "p90_m": 0.107, "rmse_m": 0.069},
}

# Python-from-CIR sensing (kept trials only).
SENSE_FROM_CIR = {
    2: {"n": 548, "median_m": 0.0822, "p90_m": 0.1707, "rmse_m": 0.1090},
    3: {"n": 878, "median_m": 0.1004, "p90_m": 0.2300, "rmse_m": 0.1522},
    4: {"n": 1739, "median_m": 0.0737, "p90_m": 0.1656, "rmse_m": 0.1047},
}

LOC_ENVS = {
    2: {
        "folder": MP / "202602122",
        "file_tag": "",
        "cells": (
            (-10, 1), (-10, 2), (-10, 3),
            (0, 1), (0, 2), (0, 3),
            (10, 1), (10, 2), (10, 3),
        ),
        "phase_deg": (0.00, 102.4, 220.5, -78.1, -188.7, 101.4, 8.9, 249.7),
        "note": "202602122 loc cells [port, cell, frame, tap]; PacketType!=1",
    },
    3: {
        "folder": MP / "20260212",
        "file_tag": "",
        "cells": (
            (-30, 3), (-30, 4),
            (-20, 3), (-20, 4),
            (-10, 3), (-10, 4),
            (0, 3), (10, 3), (20, 3),
        ),
        "phase_deg": (0.00, 102.4, 220.5, -78.1, -188.7, 101.4, 8.9, 249.7),
        "note": "20260212 loc cells [port, cell, frame, tap]; PacketType!=1",
    },
    4: {
        "folder": MP / "20260731",
        "file_tag": "env4_",
        "cells": (
            (0, 2), (0, 3),
            (10, 1), (10, 2), (10, 3),
            (20, 1), (20, 2), (20, 3),
            (30, 1), (30, 2),
        ),
        "phase_deg": (0.0, 150.63, -48.83, 25.90, -68.35, -137.35, 114.46, -13.27),
        "note": "20260731 loc cells (no 4 m) [port, cell, frame, tap]; PacketType!=1",
    },
}

SENSE_ENVS = {
    2: {
        "folder": MP / "202602282",
        "file_pat": "antenna_data_port{port}_8ports_sensing_env3_{ang}_{trial}.csv",
        "calib": MP / "202602282" / "spatial_phase_avg_complex_v3_angle0.csv",
        "pairs": (
            (-10, 3),
            (0, 3), (0, 4),
            (10, 1), (10, 2), (10, 3), (10, 4), (10, 5),
        ),
        "sync": "index",
        "extract": "peak",
        "min_range_m": 2.05,
        "skip_windows": 0,
        "keep_windows": 0,
        "note": (
            "paper Env-2 / files env3 kept trials; index-align PacketType==1; "
            "global peak min_range=2.05 m"
        ),
    },
    3: {
        "folder": MP / "20260228",
        "file_pat": "antenna_data_port{port}_8ports_sensing_env2_{ang}_{trial}.csv",
        "calib": MP / "20260228" / "spatial_phase_avg_complex_v3_angle0.csv",
        "pairs": (
            (0, 1), (0, 3), (0, 4), (0, 5),
            (-10, 1), (-10, 2), (-10, 3), (-10, 5),
            (-20, 2), (-20, 4), (-20, 5),
        ),
        "sync": "index",
        "extract": "peak",
        "min_range_m": 2.05,
        "skip_windows": 0,
        "keep_windows": 0,
        "note": (
            "paper Env-3 / files env2 kept trials; index-align PacketType==1; "
            "global peak min_range=2.05 m"
        ),
    },
    4: {
        "folder": MP / "20260729",
        "file_pat": "antenna_data_port{port}_8ports_sensing_env4_{ang}_{trial}.csv",
        "calib": MP / "20260726" / "spatial_phase_avg_complex_v3_angle0.csv",
        "pairs": tuple(
            (ang, trial)
            for ang in (0, 10, 20, 30, 40)
            for trial in (1, 2, 3)
            if not (ang == 30 and trial == 1)
        ),
        "sync": "amp2",
        "extract": "track",
        "min_range_m": 1.0,
        "skip_windows": 9,
        "keep_windows": 125,
        "note": (
            "paper Env-4 kept trials (drop 30° t1); firstPathAmp2 sync; "
            "continuous tracker; skip 9 keep 125"
        ),
    },
}


def loc_filename(env: int, port: int, ang: int, dist: int) -> Path:
    cfg = LOC_ENVS[env]
    tag = cfg["file_tag"]
    return cfg["folder"] / (
        f"antenna_data_port{port}_8ports_concurrent_localization_accuracy_{tag}{ang}_{dist}.csv"
    )


def sense_filename(env: int, port: int, ang: int, trial: int) -> Path:
    cfg = SENSE_ENVS[env]
    return cfg["folder"] / cfg["file_pat"].format(port=port, ang=ang, trial=trial)
