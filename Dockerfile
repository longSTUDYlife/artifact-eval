# syntax=docker/dockerfile:1
# Artifact image for Done academic figures
# (Figure10ab, Figure10c, Figure10d, Figure11, Figure12,
#  Figure13a, Figure13e, Figure14d)
FROM python:3.11-slim-bookworm

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    MPLCONFIGDIR=/tmp/matplotlib \
    USE_PHASE_STD_CACHE=0 \
    FIGURE10AB_FROM_CIR=1 \
    FIGURE10C_FROM_CIR=1 \
    FIGURE10D_FROM_CIR=1 \
    FIGURE13A_FROM_CIR=1 \
    FIGURE13E_FROM_CIR=1 \
    FORCE_CIR_REPROCESS=1 \
    DEBIAN_FRONTEND=noninteractive \
    PIP_DEFAULT_TIMEOUT=3600 \
    PIP_RETRIES=30

# USE_CUDA=1 installs the CUDA wheel (linux/amd64 + NVIDIA GPU).
# Default is CPU torch (Mac ARM Docker, CPU-only Linux).
ARG USE_CUDA=0

RUN apt-get update && apt-get install -y --no-install-recommends \
        libfreetype6 \
        libpng16-16 \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /artifact

COPY requirements.txt .
RUN --mount=type=cache,target=/root/.cache/pip \
    pip install -r requirements.txt

# Separate layer so a flaky CUDA wheel download does not redo numpy/matplotlib.
RUN --mount=type=cache,target=/root/.cache/pip \
    if [ "$USE_CUDA" = "1" ]; then \
      pip install torch --index-url https://download.pytorch.org/whl/cu124; \
    else \
      pip install torch --index-url https://download.pytorch.org/whl/cpu; \
    fi

# Figure folders + regenerate scripts (raw CSVs excluded via .dockerignore;
# per-curve raw packs under curve_raw_npy/ are included)
COPY . .

RUN mkdir -p /tmp/matplotlib /artifact/outputs \
    && chmod +x regenerate_all.py Figure14d/train_and_plot.py

CMD ["python", "regenerate_all.py"]
