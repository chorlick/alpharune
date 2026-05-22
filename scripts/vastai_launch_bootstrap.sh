#!/usr/bin/env bash
# Launch parallel GPU bootstrap training for all 5 legends on a vast.ai
# instance. Writes per-legend logs + an aggregated master log.
#
# Each legend gets one process; the single 4090 is time-sliced across
# them via PyTorch's normal CUDA scheduling. With 192 CPU threads, MCTS
# rollouts in self-play won't compete (the GPU is the contended resource).
#
# Usage:
#   bash scripts/vastai_launch_bootstrap.sh [legends...]
# Default = all 5: virtuoso miss_fortune gloomist khazix rengar
#
# Estimated wallclock @ this hardware (192 cores, 1×RTX 4090):
#   per legend: ~2-3 hr for 20-iter bootstrap + 30 model-driven iters
#   parallel:   ~3-4 hr total (single-GPU contention during training)
# Cost @ $0.092/hr: < $0.50 for the full run.

set -euo pipefail

REPO="${REPO:-/workspace/riftbound}"
cd "$REPO"

LEGENDS=("$@")
if [ ${#LEGENDS[@]} -eq 0 ]; then
    LEGENDS=(virtuoso miss_fortune gloomist khazix rengar)
fi

TS=$(date +%Y%m%d_%H%M%S)
LOG_DIR="$REPO/logs/bootstrap_$TS"
mkdir -p "$LOG_DIR"

MASTER_LOG="$LOG_DIR/master.log"

# Master-log header — establish the master log BEFORE any worker writes
# its own log, so subsequent appenders never race against creation.
{
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo "Riftbound GPU bootstrap — master log"
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo "Timestamp:     $TS"
  echo "Repo:          $REPO"
  echo "Commit:        $(git rev-parse --short HEAD) ($(git rev-parse --abbrev-ref HEAD))"
  echo "Hostname:      $(hostname)"
  echo "CPU cores:     $(nproc)"
  echo "GPU:           $(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo CPU-only)"
  echo "VRAM:          $(nvidia-smi --query-gpu=memory.total --format=csv,noheader 2>/dev/null || echo n/a)"
  echo "Legends:       ${LEGENDS[*]}"
  echo "Configs:       configs/gpu_bootstrap_<legend>.json"
  echo "Output dir:    $LOG_DIR"
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo ""
} | tee "$MASTER_LOG"

# Pre-flight: verify each config exists.
for L in "${LEGENDS[@]}"; do
    CFG="configs/gpu_bootstrap_${L}.json"
    if [ ! -f "$CFG" ]; then
        echo "FATAL: missing config $CFG" | tee -a "$MASTER_LOG"
        exit 1
    fi
done

# Pre-flight: verify the build target exists.
RUNNER="build/riftbound_runner"
if [ ! -x "$RUNNER" ]; then
    echo "FATAL: $RUNNER not built — run scripts/vastai_setup.sh first" | tee -a "$MASTER_LOG"
    exit 1
fi

# Launch each legend in background. Each writes to its own log; we
# append a launch-line to the master so the order is recoverable.
declare -A PIDS
for L in "${LEGENDS[@]}"; do
    CFG="configs/gpu_bootstrap_${L}.json"
    LOG="$LOG_DIR/${L}.log"
    echo "  $(date '+%H:%M:%S')  LAUNCH  $L  →  $LOG" | tee -a "$MASTER_LOG"
    stdbuf -oL -eL "$RUNNER" --config "$CFG" > "$LOG" 2>&1 &
    PIDS[$L]=$!
done

echo "" | tee -a "$MASTER_LOG"
echo "All $((${#LEGENDS[@]})) processes launched. PIDs: ${PIDS[*]}" | tee -a "$MASTER_LOG"
echo "Tail any single legend:" | tee -a "$MASTER_LOG"
for L in "${LEGENDS[@]}"; do
    echo "  tail -f $LOG_DIR/${L}.log" | tee -a "$MASTER_LOG"
done
echo "" | tee -a "$MASTER_LOG"
echo "Watch all progress (iters + losses):" | tee -a "$MASTER_LOG"
echo "  tail -f $LOG_DIR/*.log | grep -E 'iter|loss|PROMOTE|REJECT|ERROR'" | tee -a "$MASTER_LOG"
echo "" | tee -a "$MASTER_LOG"

# Block until all finish; record per-legend exit status to master log.
echo "── waiting for runs to complete ──" | tee -a "$MASTER_LOG"
for L in "${LEGENDS[@]}"; do
    PID=${PIDS[$L]}
    if wait $PID; then
        echo "  $(date '+%H:%M:%S')  DONE    $L  (pid=$PID exit=0)" | tee -a "$MASTER_LOG"
    else
        EC=$?
        echo "  $(date '+%H:%M:%S')  FAIL    $L  (pid=$PID exit=$EC)" | tee -a "$MASTER_LOG"
    fi
done

echo "" | tee -a "$MASTER_LOG"
echo "════════════════════════════════════════════════════════════════════════════════"  | tee -a "$MASTER_LOG"
echo "All runs complete. Validating final checkpoints ..." | tee -a "$MASTER_LOG"
echo "════════════════════════════════════════════════════════════════════════════════" | tee -a "$MASTER_LOG"

# Validate the final checkpoint of each legend against
# scripts/validate_checkpoint.sh. Pass criteria documented in that
# script.
for L in "${LEGENDS[@]}"; do
    CKPT_DIR="checkpoints/gpu_${L}"
    # Prefer best.pt (validation-promoted); fall back to latest iter_*.pt.
    if [ -f "$CKPT_DIR/best.pt" ]; then
        CKPT="$CKPT_DIR/best.pt"
    else
        CKPT=$(ls -t $CKPT_DIR/iter_*.pt 2>/dev/null | head -1)
    fi
    if [ -z "$CKPT" ] || [ ! -f "$CKPT" ]; then
        echo "  $L: NO CHECKPOINT FOUND in $CKPT_DIR" | tee -a "$MASTER_LOG"
        continue
    fi

    # Pick the SELF deck for the matchup deck (legend-appropriate).
    case "$L" in
        virtuoso)     DECK="decks/jhin_test.json" ;;
        miss_fortune) DECK="decks/miss_fortune_test.json" ;;
        gloomist)     DECK="decks/vex_pre_con.json" ;;
        khazix)       DECK="decks/khazix_test.json" ;;
        rengar)       DECK="decks/rengar_test.json" ;;
        *) DECK="decks/jhin_test.json" ;;
    esac

    echo "" | tee -a "$MASTER_LOG"
    echo "── VALIDATE $L ($CKPT vs $DECK) ──" | tee -a "$MASTER_LOG"
    VLOG="$LOG_DIR/validate_${L}.log"
    if bash scripts/validate_checkpoint.sh "$CKPT" "$DECK" > "$VLOG" 2>&1; then
        echo "  $L: PASS" | tee -a "$MASTER_LOG"
    else
        echo "  $L: FAIL (see $VLOG)" | tee -a "$MASTER_LOG"
    fi
    tail -10 "$VLOG" | sed 's/^/    /' | tee -a "$MASTER_LOG"
done

echo "" | tee -a "$MASTER_LOG"
echo "Master log: $MASTER_LOG" | tee -a "$MASTER_LOG"
echo "Per-legend logs: $LOG_DIR/{legend}.log" | tee -a "$MASTER_LOG"
echo "Validation logs: $LOG_DIR/validate_{legend}.log" | tee -a "$MASTER_LOG"
