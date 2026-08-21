#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Build pdoa_error_data.csv for Figure 10a from DW3000 per-angle CSVs
(pdoa_data_<angle>d.csv in this folder / originally Figures/Figure10).

Absolute_Error = |angle_deg - true_angle|, pooled over 0/10/20/30/40°.
"""

from __future__ import annotations

import csv
from pathlib import Path

ANGLES = (0, 10, 20, 30, 40)


def build_pdoa_error_csv(data_dir: Path | str | None = None, out_name: str = "pdoa_error_data.csv") -> Path:
    if data_dir is None:
        data_dir = Path(__file__).resolve().parent
    else:
        data_dir = Path(data_dir)

    rows_out: list[float] = []
    for ang in ANGLES:
        path = data_dir / f"pdoa_data_{ang}d.csv"
        if not path.is_file():
            raise FileNotFoundError(f"Missing DW3000 raw CSV: {path}")
        with path.open(newline="") as f:
            reader = csv.DictReader(f)
            if "angle_deg" not in (reader.fieldnames or []):
                raise RuntimeError(f"Expected angle_deg column in {path.name}")
            for row in reader:
                try:
                    est = float(row["angle_deg"])
                except (TypeError, ValueError):
                    continue
                rows_out.append(abs(est - float(ang)))
        print(f"  {path.name}: +{len(rows_out)} cumulative")

    out_path = data_dir / out_name
    with out_path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Absolute_Error"])
        for v in rows_out:
            w.writerow([f"{v:.16g}"])
    print(f"Wrote {out_path} (N={len(rows_out)})")
    return out_path


if __name__ == "__main__":
    build_pdoa_error_csv()
