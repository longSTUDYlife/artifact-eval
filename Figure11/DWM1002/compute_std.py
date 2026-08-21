#!/usr/bin/env python3
"""
Compute per-file and pooled phase std for DWM1002 raw-PDOA JSON-Lines logs
(as produced by pdoa_logger/log_pdoa.py -- one JSON object per line, field
"pdoa_raw_deg").

Pooling method (b): each file's own mean is removed first (so different
placement angles / baseline offsets don't inflate the combined number),
then all residuals are concatenated and the std of the pooled residuals
is reported -- this is the same convention used for the ULoc pooled-std
analysis (uloc/processing/phase_stability.py).

Usage:
    python3 compute_std.py                  # auto-discovers 0d_0m, 10d_0m, *.jsonl in this folder
    python3 compute_std.py 0d_0m 10d_0m      # explicit file list
"""

import glob
import json
import math
import os
import statistics
import sys


def load_pdoa_deg(path):
    vals = []
    with open(path) as f:
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
                vals.append(float(p))
    return vals


def discover_files():
    # Always search next to this script (not the caller's CWD).
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = []
    for name in ("0d_0m", "10d_0m"):
        path = os.path.join(here, name)
        if os.path.isfile(path):
            candidates.append(path)
    for pattern in ("*d_*m", "*.jsonl"):
        for path in glob.glob(os.path.join(here, pattern)):
            base = os.path.basename(path)
            # Avoid matching unrelated *.m sources via the loose *d_*m pattern
            if base.endswith(".m"):
                continue
            if path not in candidates:
                candidates.append(path)
    return candidates


def main():
    files = sys.argv[1:] if len(sys.argv) > 1 else discover_files()
    if not files:
        sys.exit("No input files found/specified. Usage: python3 compute_std.py <file1> <file2> ...")

    residuals = []
    n_list = []
    std_list = []

    print(f"{'file':<20} {'n':>6} {'mean(deg)':>10} {'std(deg)':>10} {'std(rad)':>10}")
    for path in files:
        vals = load_pdoa_deg(path)
        if len(vals) < 2:
            print(f"{path:<20}  (skipped: {len(vals)} valid samples)")
            continue
        mean = statistics.mean(vals)
        std = statistics.stdev(vals)
        resid = [v - mean for v in vals]
        residuals.extend(resid)
        n_list.append(len(vals))
        std_list.append(std)
        print(f"{path:<20} {len(vals):>6} {mean:>10.3f} {std:>10.3f} {math.radians(std):>10.5f}")

    if len(n_list) < 1:
        return

    # (b) pooled std: each file's own mean removed first, then pool residuals
    pooled_std = statistics.stdev(residuals) if len(residuals) > 1 else float("nan")

    # cross-check: classic pooled-variance formula (dof-weighted average of within-file variances)
    num = sum((n - 1) * (s ** 2) for n, s in zip(n_list, std_list))
    den = sum(n - 1 for n in n_list)
    pooled_std_classic = math.sqrt(num / den) if den > 0 else float("nan")

    print(f"\nTotal pooled samples: {len(residuals)} ({' + '.join(str(n) for n in n_list)})")
    print(f"Pooled std (each file's own mean removed first): {pooled_std:.3f} deg ({math.radians(pooled_std):.5f} rad)")
    print(f"Classic pooled-std formula (cross-check):         {pooled_std_classic:.3f} deg ({math.radians(pooled_std_classic):.5f} rad)")


if __name__ == "__main__":
    main()
