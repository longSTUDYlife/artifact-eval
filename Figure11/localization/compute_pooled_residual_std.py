#!/usr/bin/env python3
"""
Pooled residual std across phase-contrast distance groups.

For each file: y - mean(y)
Concatenate residuals, then compute sample std (ddof=1).
"""

from __future__ import annotations

from pathlib import Path
import csv
import math
import statistics

HERE = Path(__file__).resolve().parent
FILES = [
    HERE / "phase_contrast_256_64_4ft.txt",
    HERE / "phase_contrast_256_64_6ft.txt",
    HERE / "phase_contrast_256_64_8ft.txt",
]


def load_delta_phi(path: Path) -> list[float]:
    vals: list[float] = []
    with path.open(newline="") as f:
        for row in csv.DictReader(f):
            try:
                v = float(row["delta_phi_rad"])
            except (ValueError, KeyError):
                continue
            if math.isfinite(v):
                vals.append(v)
    return vals


def main() -> None:
    residuals: list[float] = []
    print(f"{'file':<40} {'n':>5} {'mean':>10} {'std':>10} {'std_deg':>10}")
    print("-" * 80)

    for path in FILES:
        y = load_delta_phi(path)
        if not y:
            raise SystemExit(f"No valid data in {path}")
        mu = statistics.fmean(y)
        r = [v - mu for v in y]
        residuals.extend(r)
        s = statistics.stdev(y)
        print(f"{path.name:<40} {len(y):5d} {mu:10.6f} {s:10.6f} {math.degrees(s):10.4f}")

    pooled_std = statistics.stdev(residuals)
    pooled_mean = statistics.fmean(residuals)

    print("-" * 80)
    print(f"pooled residual n   = {len(residuals)}")
    print(f"pooled residual mean= {pooled_mean:.6e}  (should be ~0)")
    print(f"pooled residual std = {pooled_std:.6f} rad  ({math.degrees(pooled_std):.4f} deg)")


if __name__ == "__main__":
    main()
