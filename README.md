# Artifact README (reviewer instructions)

Python-only reproduction of the paper figures in this folder. **No MATLAB license.**
License: MIT (`LICENSE`). Ids follow the camera-ready / revision PDF.

The default container **does not** recompute every figure. Pick one panel at a time.

---

## 1. Hardware

| Item | Requirement |
|------|-------------|
| OS | Linux recommended. Docker Desktop (macOS / Windows) is fine for CPU. |
| CPU | 4+ cores |
| RAM | **8 GB minimum, 16 GB recommended** (Figure13a / Figure14d load large arrays) |
| Disk | **~12 GB** free: ~2 GB HAR `.mat` + ~0.65 GB CIR packs + Docker image / torch |
| GPU | **Optional.** Only needed to *retrain* Figure14d. Plotting 14d from packed CMs is CPU. |
| MATLAB | Not required |

NVIDIA GPU retrain of Figure14d needs `linux/amd64` + `--gpus all` (not available on Mac Air).

---

## 2. Build and enter

```bash
cd Figures/Done          # artifact root (this folder)
docker build -t done-figures-artifact .
mkdir -p outputs
docker run --rm -it -v "$PWD/outputs:/artifact/outputs" done-figures-artifact
```

You should see a **catalog**, not a long CIR job. Then:

```bash
python regenerate_all.py --only 10ab     # pick one
python regenerate_all.py --only 10c,13e  # several
python regenerate_all.py --all           # everything (slow)
python regenerate_all.py --list
```

Older aliases still work: `--only 11a` → Figure13a, `--only 12a` → Figure13e,
`--only phase` → Figure11, `--only dr` → Figure12.

From the host (no interactive shell):

```bash
docker run --rm -v "$PWD/outputs:/artifact/outputs" done-figures-artifact \
  python regenerate_all.py --only 10ab
```

PDFs/PNGs are copied to `outputs/` on the host.

---

## 3. Time per figure (approximate, laptop CPU)

These are order-of-magnitude estimates for a recent laptop/CPU Docker.
**10c and 13e recompute range-azimuth maps from CIR — do not start those first if you only want a kick-the-tires check.**

| `--only` | What it does | Est. time | Notes |
|----------|----------------|-----------|--------|
| `10ab` | CIR → LDE → MVDR CDF/bars | 10–30 min | Fig. 10(a)(b) |
| `10c` | CIR → angle-FFT RA, 2/4/8 RX | **20–60 min** | Fig. 10(c). Slowest. 9 angles × 3 trials × 3 arrays |
| `10d` | CIR → one RA slice | 2–8 min | Fig. 10(d) |
| `11` | Phase-std bars from packs | 1–5 min | Fig. 11. Alias: `phase` |
| `12` | Sliding-window dynamic range | &lt; 1 min | Fig. 12. Alias: `dr` |
| `13a` | CIR → MVDR localization scatter | 15–40 min | Fig. 13(a). Alias: `11a` |
| `13e` | CIR → RA track (8 RX, same CIR as 10c) | **10–30 min** | Fig. 13(e); script also prints Fig. 13(f) numbers. Alias: `12a` |
| `14d` | Plot packed HAR CMs | &lt; 1 min | Fig. 14(d). No training |
| `14d` + `FIGURE14D_TRAIN=1` | Train 3 models, 40 epochs | **~30–60 min GPU / many hours CPU** | See §5 |
| `--all` | All of the above (14d still plot-only unless you set TRAIN) | **~1–3 h CPU** | 10c+13e dominate |

Kick-the-tires (minutes): `--only 14d,12,11,10d` (or the old aliases `14d,dr,phase,10d`).

Fig. 13(b–d) and 13(f–h) have no separate scripts in this package.

---

## 4. Expected numbers

Scripts print these after each run. Small drift vs the table is normal (float / GPU). Large disagreements are not.

### Figure10ab — Fig. 10(a)(b) localization AoA CDF (absolute error)

| Curve | N (this package) | median | 90th |
|-------|------------------|--------|------|
| 8RX-ULA | 16739 | 0.52° | 1.61° |
| 4RX-ULA | 18517 | 0.58° | 2.74° |
| 2RX-ULA | 23474 | 1.95° | 4.13° |
| 2-antenna DW3000 | 5000 | 2.03° | 5.01° |

### Figure10c — Fig. 10(c) sensing AoA CDF (this package’s `aoa_estimates/`)

| Array | N | median | 90th |
|-------|---|--------|------|
| 8RX | 3528 | 1.16° | 3.34° |
| 4RX | 3545 | 2.23° | 5.18° |
| 2RX | 3556 | 3.59° | 6.43° |

### Figure10d — Fig. 10(d) two-reflector RA slice

Window **14**, range **≈ 3.18 m**. Check that 8RX shows two peaks and 2RX does not (qualitative).

### Figure11 — Fig. 11 phase quality (pooled std, rad)

| Category | std (rad) |
|----------|-----------|
| sensing | **0.078** |
| localization | **0.112** |
| uloc | **0.126** |
| DW3000 | **0.098** |

### Figure12 — Fig. 12 SCR results

Per-window samples, then 0.5 m / 0.25 m sliding window. With-SCR mean ≈ **26.6 dB**, no-SCR mean ≈ **4.0 dB** (SCR clearly above no-SCR).

### Figure13a — Fig. 13(a) Env1 localization scatter (8RX)

| | Paper / this package |
|--|----------------------|
| N | **29317** |
| median | **0.043 m** |
| 90th | **0.081 m** |
| RMSE | **0.054 m** |

### Figure13e — Fig. 13(e) Env1 sensing trajectory (8RX)

`min_range = 2.05 m` + `keep_frames.csv` (lost-track frames dropped).
The printed median / 90th are the Fig. **13(f)** numbers.

| | Paper / this package |
|--|----------------------|
| N | **3177** |
| median | **6.4 cm** |
| 90th | **17.4 cm** |

### Figure14d — Fig. 14(d) HAR test CMs (file split, packed)

Split `seed=42`: train `1,2,5,6.mat` / val `3.mat` / test `4.mat`. N_test = **295**.

| Modalities | test acc |
|------------|----------|
| RD | **0.5119** |
| RD+RA | **0.8373** |
| RD+RA+RE | **0.9898** |

Labels in the figure: `bow, slap_L, slap_R, smash, volleyball` (`slam_*` in the `.mat` files).

Retraining is **not bit-exact**. Check: same file split, and RD &lt; RD+RA &lt; RD+RA+RE.

---

## 5. Retrain Figure14d (optional, GPU)

```bash
docker build --platform linux/amd64 --build-arg USE_CUDA=1 -t done-figures-artifact .
docker run --gpus all -e FIGURE14D_TRAIN=1 --rm \
  -v "$PWD/outputs:/artifact/outputs" done-figures-artifact \
  python regenerate_all.py --only 14d
```

CPU (slow):

```bash
docker run -e FIGURE14D_TRAIN=1 --rm \
  -v "$PWD/outputs:/artifact/outputs" done-figures-artifact \
  python regenerate_all.py --only 14d
```

One-epoch pipeline check (numbers will **not** match the table):

```bash
docker run -e FIGURE14D_TRAIN=1 -e SMOKE=1 --rm \
  -v "$PWD/outputs:/artifact/outputs" done-figures-artifact \
  python regenerate_all.py --only 14d
```

---

## 6. Data layout

```
curve_raw_npy/<figure>/<curve>/raw.npz   # CIR / samples (one file per curve)
Figure14d/filtered/{1..6}.mat            # HAR RD/RA/RE maps
Figure14d/cms/*.npy                      # packed test confusion matrices
```

```python
import numpy as np
d = np.load("curve_raw_npy/Figure10c/8RX-ULA/raw.npz")
cir = d["cir"]  # [port, angle, trial, frame, tap]
```

Unique CIR packs ≈ **651 MB**. HAR mats ≈ **2.0 GB**.

Re-pack CIR (maintainers): `python3 pack_curve_raw_to_npy.py`

Each figure folder has a `SOURCE.txt` with the original capture name (relative paths only).
