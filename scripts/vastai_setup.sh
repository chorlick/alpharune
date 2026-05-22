#!/usr/bin/env bash
# vast.ai instance setup for Riftbound training.
#
# Run on a fresh Ubuntu 22.04 instance (CUDA-capable, e.g. RTX 4090).
# Idempotent — safe to re-run.
#
# Usage:
#   bash vastai_setup.sh [git-ref]
# Default git-ref = master.

set -euo pipefail

REF="${1:-master}"
REPO_URL="${REPO_URL:-https://github.com/chrishorlick/automated-riftbound.git}"
WORK="${WORK:-/workspace/riftbound}"

echo "── apt deps ──"
sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    build-essential cmake ninja-build git \
    libboost-all-dev nlohmann-json3-dev \
    python3 python3-pip curl

echo "── repo ──"
if [ ! -d "$WORK" ]; then
    git clone "$REPO_URL" "$WORK"
fi
cd "$WORK"
# If this is a real git repo, sync to REF. If it was rsync'd in (no .git
# or no objects), skip — the working tree is the source of truth.
if [ -d "$WORK/.git" ] && git rev-parse --git-dir >/dev/null 2>&1 && \
   git -c gc.auto=0 rev-parse HEAD >/dev/null 2>&1; then
    git fetch --all
    git checkout "$REF"
    git pull --ff-only origin "$REF" || true
else
    echo "  Working tree is rsync'd (no usable .git). Skipping fetch/checkout."
fi

echo "── build (Release, OpenSpiel, LibTorch CUDA cu121) ──"
# First configure fetches OpenSpiel (~10s) + LibTorch CUDA (~2.5 GB).
# RIFTBOUND_LIBTORCH_VARIANT=cu121 is REQUIRED to get the CUDA build —
# default is "cpu" which silently builds without GPU support and then
# the trainer errors at runtime with "torch::cuda is not available".
# Re-runs are fast.
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DRIFTBOUND_BUILD_OPENSPIEL=ON \
    -DRIFTBOUND_BUILD_LIBTORCH=ON \
    -DRIFTBOUND_LIBTORCH_VARIANT=cu121
cmake --build build -j --target \
    riftbound_runner riftbound_openspiel riftbound_tests

echo "── unit-test sanity ──"
cd build && RIFTBOUND_ROOT=.. ./riftbound_tests --gtest_brief=1 | tail -5
cd ..

echo "── GPU sanity ──"
python3 -c "import sys; print('Python:', sys.version)"
nvidia-smi | head -20 || echo "WARN: nvidia-smi not available — CPU-only fallback"

echo ""
echo "✓ Setup complete. Repo at $WORK"
echo ""
echo "Next: bash scripts/vastai_launch_bootstrap.sh"
