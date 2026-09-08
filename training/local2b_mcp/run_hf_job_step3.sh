#!/usr/bin/env bash
set -euo pipefail

# Requires: hf auth login, positive Hugging Face Jobs credit, and hf >= 1.30.0.
# The local output directory is synced to the Jobs artifact store and mounted
# read-write so checkpoints/adapters survive the remote Job.

REPO_URL="https://github.com/stloendays/Catalyst-Longevity-Benchmark.git"
BRANCH="${BRANCH:-local2b-qlora-step3}"
FLAVOR="${FLAVOR:-t4-small}"
TIMEOUT="${TIMEOUT:-4h}"
OUTPUT_LOCAL="${OUTPUT_LOCAL:-./build/local2b-step3-hf-output}"
MAX_STEPS="${MAX_STEPS:--1}"
TRAIN_LIMIT="${TRAIN_LIMIT:-}"

mkdir -p "$OUTPUT_LOCAL"

TRAIN_LIMIT_ARG=""
if [[ -n "$TRAIN_LIMIT" ]]; then
  TRAIN_LIMIT_ARG="--train-limit $TRAIN_LIMIT"
fi

hf jobs run \
  --name local2b-qwen35-step3 \
  --flavor "$FLAVOR" \
  --timeout "$TIMEOUT" \
  -v "$OUTPUT_LOCAL:/output:rw" \
  huggingface/trl \
  bash -lc "
    set -euo pipefail
    git clone --depth 1 --branch '$BRANCH' '$REPO_URL' /work/repo
    cd /work/repo
    python -m pip install --upgrade -r training/local2b_mcp/requirements-step3.txt
    rm -rf /work/dataset
    python training/local2b_mcp/generate_stage2_dataset.py --out /work/dataset --train 16000 --val 1600 --test 1600 --seed 20260908
    python training/local2b_mcp/validate_dataset.py /work/dataset
    python training/local2b_mcp/preflight_step3.py --data-dir /work/dataset
    python training/local2b_mcp/train_qlora.py \
      --data-dir /work/dataset \
      --output-dir /output/run \
      --max-steps '$MAX_STEPS' \
      $TRAIN_LIMIT_ARG
  "
