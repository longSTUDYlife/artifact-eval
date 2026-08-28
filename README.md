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
| GPU | Optional. Needed only if you retrain Fig. 14(d). |

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
python regenerate_all.py --only 14d --eval
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
| Fig. 13(a)(b) | `13a` | Env-1 scatter. `--env 2\|3\|4`: that environment’s errors in the terminal | 15–40 min (Env-1); 20–60 min extra env |
| Fig. 13(e)(f) | `13e` | Env-1 trajectory. `--env 2\|3\|4`: that environment’s errors in the terminal | 10–30 min (Env-1); 10–25 min extra env |
| Fig. 14(d) | `14d` | HAR confusion matrices | 2–8 min |

`--all` is about 1–3 hours on CPU. Fig. 14(d) loads the provided models
and tests on CPU. Retrain is optional (see §5).

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

**Fig. 13(a)(b)** `--only 13a` is Env-1, 8RX: median 4.3 cm, 90th 8.1 cm.
`--env 2|3|4` prints that environment:

| Env | median | 90th |
|-----|--------|------|
| 2 | 5.2 cm | 10.2 cm |
| 3 | 6.9 cm | 12.1 cm |
| 4 | 5.7 cm | 10.7 cm |

**Fig. 13(e)(f)** `--only 13e` is Env-1, 8RX: median 6.4 cm, 90th 17.4 cm.
`--env 2|3|4` prints that environment:

| Env | median | 90th |
|-----|--------|------|
| 2 | 8.2 cm | 17.1 cm |
| 3 | 10.0 cm | 23.0 cm |
| 4 | 7.4 cm | 16.6 cm |

**Fig. 14(d)** RD 0.51, RD+RA 0.84, RD+RA+RE 0.99.
Train / val / test files: `1,2,5,6` / `3` / `4` (`seed=42`).
Labels: `bow`, `slap_L`, `slap_R`, `smash`, `volleyball`.

---

## 5. Optional: retrain Fig. 14(d)

The CPU image is enough to reproduce the figure: it already contains
the three trained models and the test file `Figure14d/filtered/4.mat`.

```bash
docker run --rm -v "$PWD/outputs:/artifact/outputs" \
  whatcanisay/ultralego-ae:cpu \
  python regenerate_all.py --only 14d --eval
```

The other five recordings (`1.mat`, `2.mat`, `3.mat`, `5.mat`, `6.mat`)
are in the GitHub repo but **not** in the CPU image, so a `docker pull`
image cannot retrain. To retrain you must rebuild from a clone:

1. Clone this repository. Check that all six files exist:

   ```bash
   ls Figure14d/filtered/*.mat
   # 1.mat  2.mat  3.mat  4.mat  5.mat  6.mat
   ```

2. Edit `.dockerignore` in the repo root. The CPU image omits the
   training files with these five lines (leave them as they are if you
   only want eval):

   ```
   Figure14d/filtered/1.mat
   Figure14d/filtered/2.mat
   Figure14d/filtered/3.mat
   Figure14d/filtered/5.mat
   Figure14d/filtered/6.mat
   ```

   **Delete or comment out those five lines** so `docker build` copies
   them. Do not add a line for `4.mat`: it is not listed, so it is
   already included.

3. Build a CUDA image and train:

   ```bash
   docker build --platform linux/amd64 --build-arg USE_CUDA=1 \
     -t whatcanisay/ultralego-ae:gpu .
   docker run --gpus all -e FIGURE14D_TRAIN=1 --rm \
     -v "$PWD/outputs:/artifact/outputs" whatcanisay/ultralego-ae:gpu \
     python regenerate_all.py --only 14d
   ```

Use the same file split as above. The ordering RD < RD+RA < RD+RA+RE
should hold; exact accuracies may differ slightly from the provided
models. `FIGURE14D_TRAIN=0` plots the stored confusion matrices and
does not run the network.

---

## 6. Data

```
curve_raw_npy/<figure>/<curve>/raw.npz   # CIR / samples used by each figure
Figure14d/checkpoints/*.pth              # Fig. 14(d) trained models
Figure14d/filtered/{1..6}.mat            # HAR maps (test = 4.mat)
Figure14d/cms/*.npy                      # stored Fig. 14(d) confusion matrices
curve_raw_npy/Figure13a/env{2,3,4}/      # Fig. 13 extra environments
curve_raw_npy/Figure13e/env{2,3,4}/
3d_model/Shell.STL                       # 3D-printed module shell (~47×48×47 mm)
firmware/                                # radino32 core + sketch + CIR logger
```

The STL is for inspecting or printing the module housing. Figure
scripts do not use it.

```python
import numpy as np
d = np.load("curve_raw_npy/Figure10c/8RX-ULA/raw.npz")
cir = d["cir"]  # [port, angle, trial, frame, tap]
```

---

## 7. Firmware (optional)

`firmware/` is the radino32 testbed path (modified In-Circuit core
`1.0.3/`, `dw1000_ranging_demo.ino`, `MultiPort_CIR_Logger.py`).
It is **not** used to regenerate figures. Setup: install the official
Arduino package, then replace `1.0.3`; roles and serial logging are
in [`firmware/README.md`](firmware/README.md).
