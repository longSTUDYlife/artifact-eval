# Artifact

This repository regenerates the key evaluation figures of **UltraLEGO**
from recorded UWB channel impulse responses (CIR) and radar maps.
One Docker image runs the paper’s localization, sensing, and human
activity recognition (HAR) pipelines and writes the corresponding PDFs.
Radios, MATLAB, and testbed access are not required.

License: MIT (`LICENSE`).

---

## 1. Requirements

| | |
|--|--|
| OS | Linux recommended. Docker Desktop (macOS / Windows) works for CPU. |
| CPU | 4+ cores |
| RAM | 8 GB minimum, 16 GB recommended |
| Disk | about 16 GB free |
| GPU | Optional. Needed only to retrain Fig. 14(d). Plotting 14(d) is CPU. |

---

## 2. Build and run

Pull the pre-built CPU image (`linux/amd64`). Building from this
folder is optional.

```bash
docker pull whatcanisay/ultralego-ae:cpu
mkdir -p outputs
docker run --rm -it -v "$PWD/outputs:/artifact/outputs" \
  whatcanisay/ultralego-ae:cpu
```

The container prints a figure catalog. Then:

```bash
python regenerate_all.py --list
python regenerate_all.py --only 10ab
python regenerate_all.py --only 10c,13e
python regenerate_all.py --only 13a --env 2
python regenerate_all.py --only 13e --env 4
python regenerate_all.py --all
```

From the host:

```bash
docker run --rm -v "$PWD/outputs:/artifact/outputs" whatcanisay/ultralego-ae:cpu \
  python regenerate_all.py --only 10ab
```

PDFs and PNGs are copied to `outputs/` on the host.

**Recommended first run** (a few minutes):

```bash
python regenerate_all.py --only 14d,12,11,10d
```

Then regenerate any remaining figure with `--only`.

Optional local CPU rebuild (PyTorch download is 10–30 minutes):

```bash
docker build --platform linux/amd64 -t whatcanisay/ultralego-ae:cpu .
```

---

## 3. Paper figures

| Paper | `--only` | Result | Time (laptop CPU) |
|-------|----------|--------|-------------------|
| Fig. 10(a)(b) | `10ab` | localization AoA CDF and bars | 10–30 min |
| Fig. 10(c) | `10c` | sensing AoA CDF | 20–60 min |
| Fig. 10(d) | `10d` | angular resolution (two reflectors) | 2–8 min |
| Fig. 11 | `11` | phase quality | 1–5 min |
| Fig. 12 | `12` | SCR dynamic range | < 1 min |
| Fig. 13(a)(b) | `13a` | Env-1 scatter and localization error. `--env 2\|3\|4`: that env from CIR, metrics only (no plot) | 15–40 min (Env-1); 20–60 min extra env |
| Fig. 13(e)(f) | `13e` | Env-1 sensing trajectory and error. `--env 2\|3\|4`: that env from CIR, metrics only (no plot) | 10–30 min (Env-1); 10–25 min extra env |
| Fig. 14(d) | `14d` | HAR confusion matrices | < 1 min to plot |

`--all` is about 1–3 hours on CPU. Optional GPU retrain of Fig. 14(d) is
30–60 minutes (see §5).

---

## 4. Expected results

Each script prints the statistics below. Small floating-point variation
is expected; the relative ordering and the qualitative claims should hold.

**Fig. 10(a)** localization AoA (absolute error)

| Curve | median | 90th |
|-------|--------|------|
| 8RX-ULA | 0.52° | 1.61° |
| 4RX-ULA | 0.58° | 2.74° |
| 2RX-ULA | ≈2° | ≈4° |
| DW3000 | comparable to 2RX | |

**Fig. 10(c)** sensing AoA

| Array | median | 90th |
|-------|--------|------|
| 8RX | 1.16° | 3.34° |
| 4RX | 2.23° | 5.18° |
| 2RX | 3.59° | 6.43° |

**Fig. 10(d)** 8RX shows two distinct peaks; 2RX peaks merge.

**Fig. 11** phase std (rad): sensing 0.078, localization 0.112, ULoc 0.126, DW3000 0.098.

**Fig. 12** SCR well above no-SCR (means ≈27 dB vs. ≈4 dB).

**Fig. 13(a)(b)** default (`--only 13a`) is Env-1, 8RX: median 4.3 cm, 90th 8.1 cm, RMSE 5.4 cm. Optional `--env 2|3|4` recomputes that environment from packed CIR (LDE + MVDR + refit distance) and prints N / median / 90th / RMSE; no PDF. Paper reference:

| Env | N | median | 90th | RMSE |
|-----|---|--------|------|------|
| 2 | 15450 | 5.2 cm | 10.2 cm | 6.6 cm |
| 3 | 6760 | 6.9 cm | 12.1 cm | 8.1 cm |
| 4 | 9354 | 5.7 cm | 10.7 cm | 6.9 cm |

**Fig. 13(e)(f)** default (`--only 13e`) is Env-1, 8RX: median 6.4 cm, 90th 17.4 cm. Optional `--env 2|3|4` recomputes that environment from the packed kept trials and prints the same four metrics (no trajectory plot). From-CIR reference:

| Env | N | median | 90th | RMSE |
|-----|---|--------|------|------|
| 2 | 548 | 8.2 cm | 17.1 cm | 10.9 cm |
| 3 | 878 | 10.0 cm | 23.0 cm | 15.2 cm |
| 4 | 1739 | 7.4 cm | 16.6 cm | 10.5 cm |

**Fig. 14(d)** from the provided confusion matrices: RD 0.51, RD+RA 0.84, RD+RA+RE 0.99.
Optional retrain uses the same file split (`seed=42`: train `1,2,5,6` / val `3` / test `4`)
and preserves RD < RD+RA < RD+RA+RE.
Labels: `bow`, `slap_L`, `slap_R`, `smash`, `volleyball`.

---

## 5. Optional: retrain Fig. 14(d)

```bash
docker pull whatcanisay/ultralego-ae:gpu
docker run --gpus all -e FIGURE14D_TRAIN=1 --rm \
  -v "$PWD/outputs:/artifact/outputs" whatcanisay/ultralego-ae:gpu \
  python regenerate_all.py --only 14d
```

Or build the CUDA image locally:

```bash
docker build --platform linux/amd64 --build-arg USE_CUDA=1 \
  -t whatcanisay/ultralego-ae:gpu .
```

CPU retrain (slow): omit `--gpus all` and the CUDA build argument.

---

## 6. Data

```
curve_raw_npy/<figure>/<curve>/raw.npz   # packed CIR / samples
Figure14d/filtered/{1..6}.mat            # HAR RD/RA/RE maps
Figure14d/cms/*.npy                      # test confusion matrices
```

```python
import numpy as np
d = np.load("curve_raw_npy/Figure10c/8RX-ULA/raw.npz")
cir = d["cir"]  # [port, angle, trial, frame, tap]
```

Fig. 13 extra environments (kept cells / trials only):

```
curve_raw_npy/Figure13a/env{2,3,4}/raw.npz   # loc [port, cell, frame, tap]
curve_raw_npy/Figure13e/env{2,3,4}/raw.npz   # sensing [port, pair, frame, tap]
```
