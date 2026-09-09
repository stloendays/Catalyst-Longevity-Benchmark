#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  run_server_qvac_lora.sh --data FILE --output FILE [options]

Required:
  --data FILE              QVAC messages-only SFT JSONL
  --output FILE            Adapter output path (.gguf)

Options:
  --trainer FILE           llama-finetune-lora binary
  --model FILE             Base Q4 GGUF
  --rank N                 LoRA rank (default: 4)
  --alpha N                LoRA alpha (default: 8)
  --modules LIST           Target modules (default: attn_q,attn_v)
  --context N              Context length (default: 576)
  --batch N                Batch size (default: 16)
  --ubatch N               Micro-batch size (default: 8)
  --epochs N               Epochs (default: 1)
  --learning-rate VALUE    Learning rate (default: 1e-5)
  --seed N                 Seed (default: 20260908)
  --timeout-seconds N      Hard training timeout (default: 7200)
  --checkpoint-steps N     Save checkpoint every N steps (default: 2)
  --checkpoint-dir DIR     Checkpoint directory (default: OUTPUT.checkpoints)
  --log FILE               Training log path (default: OUTPUT.log)
  --result FILE            Result metadata path (default: OUTPUT.result.txt)
  --install FILE           Copy successful adapter to this final path
  --keep-service-running   Do not stop rhocodec-llm.service (unsafe on low RAM)
EOF
}

TRAINER=/home/ubuntu/qvac-trainer-step3/llama-finetune-lora
MODEL=/opt/rhocodec-llm/models/Qwen3.5-2B-Q4_K_M.gguf
DATA=
OUTPUT=
RANK=4
ALPHA=8
MODULES=attn_q,attn_v
CONTEXT=576
BATCH=16
UBATCH=8
EPOCHS=1
LR=1e-5
SEED=20260908
TIMEOUT_SECONDS=7200
CHECKPOINT_STEPS=2
CHECKPOINT_DIR=
LOG=
RESULT=
INSTALL=
STOP_SERVICE=1

while [ "$#" -gt 0 ]; do
  case "$1" in
    --trainer) TRAINER=$2; shift 2 ;;
    --model) MODEL=$2; shift 2 ;;
    --data) DATA=$2; shift 2 ;;
    --output) OUTPUT=$2; shift 2 ;;
    --rank) RANK=$2; shift 2 ;;
    --alpha) ALPHA=$2; shift 2 ;;
    --modules) MODULES=$2; shift 2 ;;
    --context) CONTEXT=$2; shift 2 ;;
    --batch) BATCH=$2; shift 2 ;;
    --ubatch) UBATCH=$2; shift 2 ;;
    --epochs) EPOCHS=$2; shift 2 ;;
    --learning-rate) LR=$2; shift 2 ;;
    --seed) SEED=$2; shift 2 ;;
    --timeout-seconds) TIMEOUT_SECONDS=$2; shift 2 ;;
    --checkpoint-steps) CHECKPOINT_STEPS=$2; shift 2 ;;
    --checkpoint-dir) CHECKPOINT_DIR=$2; shift 2 ;;
    --log) LOG=$2; shift 2 ;;
    --result) RESULT=$2; shift 2 ;;
    --install) INSTALL=$2; shift 2 ;;
    --keep-service-running) STOP_SERVICE=0; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [ -z "$DATA" ] || [ -z "$OUTPUT" ]; then
  usage >&2
  exit 2
fi
if [ -z "$LOG" ]; then LOG="${OUTPUT}.log"; fi
if [ -z "$RESULT" ]; then RESULT="${OUTPUT}.result.txt"; fi
if [ -z "$CHECKPOINT_DIR" ]; then CHECKPOINT_DIR="${OUTPUT}.checkpoints"; fi

for path in "$TRAINER" "$MODEL" "$DATA"; do
  if [ ! -s "$path" ]; then
    echo "Required file missing or empty: $path" >&2
    exit 3
  fi
done

mkdir -p "$(dirname "$OUTPUT")" "$(dirname "$LOG")" "$(dirname "$RESULT")" "$CHECKPOINT_DIR"
rm -f "$OUTPUT" "$LOG" "$RESULT"

MODEL_SHA=$(sha256sum "$MODEL" | awk '{print $1}')
DATA_SHA=$(sha256sum "$DATA" | awk '{print $1}')
WAS_ACTIVE=0

restore_service() {
  if [ "$WAS_ACTIVE" -eq 1 ]; then
    sudo systemctl start rhocodec-llm.service || true
  fi
}
trap restore_service EXIT INT TERM

if [ "$STOP_SERVICE" -eq 1 ] && systemctl is-active --quiet rhocodec-llm.service; then
  WAS_ACTIVE=1
  sudo systemctl stop rhocodec-llm.service
fi

echo "SERVER_QVAC_TRAIN_START=$(date -Is)"
echo "BASE_MODEL_SHA256=$MODEL_SHA"
echo "DATA_SHA256=$DATA_SHA"
free -h || true

START=$(date +%s)
set +e
timeout "$TIMEOUT_SECONDS" "$TRAINER" \
  -m "$MODEL" \
  -f "$DATA" \
  --assistant-loss-only \
  --num-epochs "$EPOCHS" \
  --lora-rank "$RANK" \
  --lora-alpha "$ALPHA" \
  --lora-modules "$MODULES" \
  --lora-seed "$SEED" \
  --learning-rate "$LR" \
  --weight-decay 0 \
  --lr-scheduler constant \
  --checkpoint-save-steps "$CHECKPOINT_STEPS" \
  --checkpoint-save-dir "$CHECKPOINT_DIR" \
  --output-adapter "$OUTPUT" \
  -c "$CONTEXT" -b "$BATCH" -ub "$UBATCH" -ngl 0 -fa off \
  2>&1 | tee "$LOG"
RC=${PIPESTATUS[0]}
set -e
END=$(date +%s)
ELAPSED=$((END - START))

if [ "$RC" -ne 0 ]; then
  echo "SERVER_QVAC_TRAIN_RC=$RC" >&2
  echo "CHECKPOINT_DIR=$CHECKPOINT_DIR" >&2
  find "$CHECKPOINT_DIR" -maxdepth 2 -type f -printf '%p %s bytes\n' 2>/dev/null || true
  exit "$RC"
fi
if [ ! -s "$OUTPUT" ]; then
  echo "Trainer returned success but adapter is missing: $OUTPUT" >&2
  exit 4
fi

ADAPTER_SHA=$(sha256sum "$OUTPUT" | awk '{print $1}')
ADAPTER_BYTES=$(stat -c '%s' "$OUTPUT")

if [ -n "$INSTALL" ]; then
  sudo install -d -m 755 "$(dirname "$INSTALL")"
  sudo install -m 644 "$OUTPUT" "$INSTALL"
  INSTALLED_SHA=$(sha256sum "$INSTALL" | awk '{print $1}')
  if [ "$INSTALLED_SHA" != "$ADAPTER_SHA" ]; then
    echo "Installed adapter hash mismatch" >&2
    exit 5
  fi
fi

{
  echo "base_model=$MODEL"
  echo "base_model_sha256=$MODEL_SHA"
  echo "data=$DATA"
  echo "data_sha256=$DATA_SHA"
  echo "adapter=$OUTPUT"
  echo "adapter_sha256=$ADAPTER_SHA"
  echo "adapter_bytes=$ADAPTER_BYTES"
  echo "installed_adapter=$INSTALL"
  echo "elapsed_seconds=$ELAPSED"
  echo "seed=$SEED"
  echo "rank=$RANK"
  echo "alpha=$ALPHA"
  echo "modules=$MODULES"
  echo "context=$CONTEXT"
  echo "batch=$BATCH"
  echo "ubatch=$UBATCH"
  echo "epochs=$EPOCHS"
  echo "learning_rate=$LR"
  echo "checkpoint_steps=$CHECKPOINT_STEPS"
  echo "checkpoint_dir=$CHECKPOINT_DIR"
} | tee "$RESULT"

restore_service
WAS_ACTIVE=0
trap - EXIT INT TERM

if [ "$STOP_SERVICE" -eq 1 ]; then
  for _ in $(seq 1 30); do
    if curl -fsS http://127.0.0.1:18080/v1/models >/dev/null 2>&1; then
      echo "INFERENCE_SERVICE=HEALTHY"
      break
    fi
    sleep 2
  done
  systemctl is-active rhocodec-llm.service >/dev/null
  curl -fsS http://127.0.0.1:18080/v1/models >/dev/null
fi

echo "SERVER_QVAC_TRAIN=PASS"
