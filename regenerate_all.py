#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Regenerate selected Done figures from packed measurements (not pre-plotted tables).

Ids follow the camera-ready / revision PDF (MobiCom26 #978).

Default: print the catalog and exit. Nothing is computed until the reviewer
chooses a figure.

  python regenerate_all.py                 # list / interactive menu
  python regenerate_all.py --only 10c
  python regenerate_all.py --only 10ab,13e
  python regenerate_all.py --all
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

# Canonical id → aliases, label, output basenames
CATALOG = [
    {
        "id": "Figure10ab",
        "aliases": ("10ab", "figure10ab"),
        "label": "Fig. 10(a)(b) localization CDF + per-angle bars (CIR → LDE → MVDR)",
        "outputs": ("figure10ab.pdf", "figure10ab.png"),
    },
    {
        "id": "Figure10c",
        "aliases": ("10c", "figure10c"),
        "label": "Fig. 10(c) sensing AoA CDF (CIR → angle-FFT RA)  [slowest]",
        "outputs": ("figure10c.pdf", "figure10c.png"),
    },
    {
        "id": "Figure10d",
        "aliases": ("10d", "figure10d"),
        "label": "Fig. 10(d) two-reflector RA slice (CIR → angle-FFT)",
        "outputs": ("figure10d.pdf", "figure10d.png"),
    },
    {
        "id": "Figure11",
        "aliases": (
            "11",
            "figure11",
            "phase",
            "phase_coherence",
            "phase-coherance",
            "phase_coherance",
        ),
        "label": "Fig. 11 phase-std bars (sensing / loc / uloc / DW3000)",
        "outputs": ("figure11.pdf", "figure11.png"),
    },
    {
        "id": "Figure12",
        "aliases": ("12", "figure12", "dr", "dynamic-range", "dynamic_range"),
        "label": "Fig. 12 SCR dynamic range vs range",
        "outputs": ("figure12.pdf", "figure12.png"),
    },
    {
        "id": "Figure13a",
        "aliases": ("13a", "figure13a", "11a", "figure11a"),
        "label": "Fig. 13(a) Env1 localization scatter (CIR → MVDR)",
        "outputs": ("figure13a.pdf", "figure13a.png"),
    },
    {
        "id": "Figure13e",
        "aliases": ("13e", "figure13e", "12a", "figure12a"),
        "label": "Fig. 13(e) Env1 sensing trajectory (CIR → RA track)  [slow]",
        "outputs": ("figure13e.pdf", "figure13e.png"),
    },
    {
        "id": "Figure14d",
        "aliases": ("14d", "figure14d"),
        "label": "Fig. 14(d) HAR confusion matrices (packed cms; set FIGURE14D_TRAIN=1 to retrain)",
        "outputs": ("figure14d.pdf", "figure14d.png"),
    },
]


def _index() -> dict[str, dict]:
    idx = {}
    for item in CATALOG:
        idx[item["id"].lower()] = item
        for a in item["aliases"]:
            idx[a.lower()] = item
    return idx


def _prepare_env() -> None:
    os.environ["USE_PHASE_STD_CACHE"] = "0"
    os.environ["FIGURE10AB_FROM_CIR"] = "1"
    os.environ["FIGURE10C_FROM_CIR"] = "1"
    os.environ["FIGURE10D_FROM_CIR"] = "1"
    os.environ["FIGURE13A_FROM_CIR"] = "1"
    os.environ["FIGURE13E_FROM_CIR"] = "1"
    os.environ["FORCE_CIR_REPROCESS"] = "1"
    os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")
    Path(os.environ["MPLCONFIGDIR"]).mkdir(parents=True, exist_ok=True)


def _copy_outputs(item: dict) -> None:
    out_dir = ROOT / "outputs"
    out_dir.mkdir(exist_ok=True)
    folder = ROOT / item["id"]
    for name in item["outputs"]:
        src = folder / name
        if src.is_file():
            dest = out_dir / src.name
            dest.write_bytes(src.read_bytes())
            print(f"Copied -> {dest}")


def run_figure10ab() -> None:
    sys.path.insert(0, str(ROOT / "Figure10ab"))
    from plot_figure10ab import plot_figure10ab
    plot_figure10ab(data_dir=str(ROOT / "Figure10ab"), reprocess=True)


def run_figure10c() -> None:
    sys.path.insert(0, str(ROOT / "Figure10c"))
    from plot_figure10c import plot_figure10c
    plot_figure10c(data_dir=str(ROOT / "Figure10c"), reprocess=True)


def run_figure10d() -> None:
    sys.path.insert(0, str(ROOT / "Figure10d"))
    from plot_figure10d import plot_figure10d
    plot_figure10d(data_dir=str(ROOT / "Figure10d"), reprocess=True)


def run_figure11() -> None:
    sys.path.insert(0, str(ROOT / "Figure11"))
    from plot_phase_std_bar_academic import plot_phase_std_bar
    plot_phase_std_bar(data_dir=str(ROOT / "Figure11"))


def run_figure12() -> None:
    sys.path.insert(0, str(ROOT / "Figure12"))
    from plot_dynamic_range_academic import plot_dynamic_range_academic
    plot_dynamic_range_academic(data_dir=str(ROOT / "Figure12"))


def run_figure13a() -> None:
    sys.path.insert(0, str(ROOT / "Figure13a"))
    from plot_figure13a import plot_figure13a
    plot_figure13a(data_dir=str(ROOT / "Figure13a"), reprocess=True)


def run_figure13e() -> None:
    sys.path.insert(0, str(ROOT / "Figure13e"))
    from plot_figure13e import plot_figure13e
    plot_figure13e(data_dir=str(ROOT / "Figure13e"), reprocess=True)


def run_figure14d() -> None:
    sys.path.insert(0, str(ROOT / "Figure14d"))
    from train_and_plot import regenerate_figure14d
    regenerate_figure14d(ROOT / "Figure14d")


RUNNERS = {
    "Figure10ab": run_figure10ab,
    "Figure10c": run_figure10c,
    "Figure10d": run_figure10d,
    "Figure11": run_figure11,
    "Figure12": run_figure12,
    "Figure13a": run_figure13a,
    "Figure13e": run_figure13e,
    "Figure14d": run_figure14d,
}


def print_catalog() -> None:
    print("Available figures (nothing is computed until you pick one):\n")
    for i, item in enumerate(CATALOG, 1):
        alias = item["aliases"][0]
        print(f"  {i}. {item['id']:<16}  --only {alias:<16}  {item['label']}")
    print()
    print("Examples:")
    print("  python regenerate_all.py --only 10ab")
    print("  python regenerate_all.py --only 10c,13e")
    print("  python regenerate_all.py --all")
    print()
    print("Outputs are copied to /artifact/outputs (mount a host folder).")
    print("Figure14d retrain: FIGURE14D_TRAIN=1 python regenerate_all.py --only 14d")


def resolve_ids(tokens: list[str]) -> list[dict]:
    idx = _index()
    seen = []
    seen_ids = set()
    for raw in tokens:
        key = raw.strip().lower()
        if not key:
            continue
        if key not in idx:
            known = ", ".join(item["id"] for item in CATALOG)
            raise SystemExit(f"Unknown figure {raw!r}. Choose from: {known}")
        item = idx[key]
        if item["id"] not in seen_ids:
            seen.append(item)
            seen_ids.add(item["id"])
    if not seen:
        raise SystemExit("No figures selected.")
    return seen


def run_items(items: list[dict]) -> int:
    _prepare_env()
    for item in items:
        print(f"\n=== {item['id']} ===")
        print(item["label"])
        RUNNERS[item["id"]]()
        _copy_outputs(item)
    print("\nDone. PDFs/PNGs are under outputs/")
    return 0


def interactive_menu() -> int:
    print_catalog()
    try:
        raw = input(
            "Select number(s) or id(s), comma-separated; "
            "'all' for everything; Enter/q to quit: "
        ).strip()
    except EOFError:
        return 0
    if raw == "" or raw.lower() in ("q", "quit"):
        print("No figures selected.")
        return 0
    if raw.lower() == "all":
        return run_items(list(CATALOG))
    tokens = []
    for part in raw.split(","):
        part = part.strip()
        if part.isdigit() and 1 <= int(part) <= len(CATALOG):
            tokens.append(CATALOG[int(part) - 1]["id"])
        else:
            tokens.append(part)
    return run_items(resolve_ids(tokens))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Regenerate selected artifact figures. Default: list only, do not compute."
    )
    parser.add_argument(
        "--only",
        default="",
        help="Comma-separated figure ids, e.g. 10ab or 10c,13e",
    )
    parser.add_argument("--all", action="store_true", help="Regenerate every figure (slow)")
    parser.add_argument("--list", action="store_true", help="Print the catalog and exit")
    args = parser.parse_args(argv)

    if args.all:
        return run_items(list(CATALOG))
    if args.only:
        tokens = [t for t in args.only.replace(" ", ",").split(",") if t]
        return run_items(resolve_ids(tokens))
    if args.list or not sys.stdin.isatty():
        print_catalog()
        return 0
    return interactive_menu()


if __name__ == "__main__":
    raise SystemExit(main())
