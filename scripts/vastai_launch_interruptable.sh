#!/usr/bin/env bash
# Interruptable-instance launcher with auto-resume.
#
# Same idea as vastai_launch_bootstrap.sh, but each legend's runner is
# wrapped in a restart loop. If the instance is killed mid-iter and you
# spin it back up, re-running this script picks up at the iter AFTER
# the last completed checkpoint — no lost work past the in-flight iter.
#
# Mechanism:
#   1. Per legend, scan $CHECKPOINT_DIR/iter_*.pt for the highest N.
#   2. If found: patch the config in /tmp with `resume_checkpoint =
#      iter_N.pt` and `start_iter = N+1`; if all iters done, skip.
#   3. Run the patched config. Repeat until the config's `iterations`
#      target is reached or runner exits cleanly.
#
# Usage:
#   bash scripts/vastai_launch_interruptable.sh [legends...]
# Default = all 5: virtuoso miss_fortune gloomist khazix rengar
#
# Requires jq. Apt-installed by vastai_setup.sh.

set -euo pipefail

REPO="${REPO:-/workspace/riftbound}"
cd "$REPO"

command -v jq >/dev/null 2>&1 || {
    echo "FATAL: jq not installed. Run: apt-get install -y jq"; exit 1;
}

LEGENDS=("$@")
if [ ${#LEGENDS[@]} -eq 0 ]; then
    LEGENDS=(virtuoso miss_fortune gloomist khazix rengar)
fi

TS=$(date +%Y%m%d_%H%M%S)
LOG_DIR="$REPO/logs/bootstrap_$TS"
mkdir -p "$LOG_DIR"
MASTER_LOG="$LOG_DIR/master.log"

{
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo "Riftbound GPU bootstrap (INTERRUPTABLE) — master log"
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo "Timestamp:     $TS"
  echo "Repo:          $REPO"
  echo "Hostname:      $(hostname)"
  echo "CPU cores:     $(nproc)"
  echo "GPU:           $(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo n/a)"
  echo "Legends:       ${LEGENDS[*]}"
  echo "Mode:          auto-resume from checkpoints/gpu_<legend>/iter_*.pt"
  echo "════════════════════════════════════════════════════════════════════════════════"
  echo ""
} | tee "$MASTER_LOG"

RUNNER="build/riftbound_runner"
if [ ! -x "$RUNNER" ]; then
    echo "FATAL: $RUNNER not built — run scripts/vastai_setup.sh first" | tee -a "$MASTER_LOG"
    exit 1
fi

# Spawn per-legend wrapper. Each wrapper loops until the runner exits
# cleanly OR the runner's iter target is hit.
run_legend() {
    local L="$1"
    local CFG="configs/gpu_bootstrap_${L}.json"
    local LOG="$LOG_DIR/${L}.log"
    local PATCHED="/tmp/cfg_${L}_${TS}.json"
    local CKPT_DIR
    local TOTAL_ITERS

    CKPT_DIR=$(jq -r '.training.checkpoint_dir' "$CFG")
    TOTAL_ITERS=$(jq -r '.training.iterations' "$CFG")
    mkdir -p "$CKPT_DIR"

    local attempt=0
    while true; do
        attempt=$((attempt + 1))

        # Detect latest checkpoint
        local LAST_CKPT
        LAST_CKPT=$(ls -t "$CKPT_DIR"/iter_*.pt 2>/dev/null | head -1 || true)

        if [ -n "$LAST_CKPT" ]; then
            local LAST_ITER
            LAST_ITER=$(basename "$LAST_CKPT" | sed -E 's/iter_([0-9]+)\.pt/\1/')
            local NEXT_ITER=$((LAST_ITER + 1))
            if [ "$NEXT_ITER" -ge "$TOTAL_ITERS" ]; then
                echo "  $(date '+%H:%M:%S')  DONE     $L  (all $TOTAL_ITERS iters complete)" \
                  | tee -a "$MASTER_LOG"
                return 0
            fi
            echo "  $(date '+%H:%M:%S')  RESUME   $L  attempt=$attempt  iter $NEXT_ITER/$TOTAL_ITERS  ckpt=$LAST_CKPT" \
              | tee -a "$MASTER_LOG"
            # Patch the config in /tmp so we don't dirty the repo
            jq --arg ck "$(realpath "$LAST_CKPT")" --argjson si "$NEXT_ITER" \
                '.training.resume_checkpoint = $ck | .training.start_iter = $si' \
                "$CFG" > "$PATCHED"
            CFG_TO_USE="$PATCHED"
        else
            echo "  $(date '+%H:%M:%S')  START    $L  attempt=$attempt  (fresh from iter 0)" \
              | tee -a "$MASTER_LOG"
            CFG_TO_USE="$CFG"
        fi

        # Run; capture exit status. Append output to per-legend log.
        if stdbuf -oL -eL "$RUNNER" --config "$CFG_TO_USE" >> "$LOG" 2>&1; then
            echo "  $(date '+%H:%M:%S')  EXIT_OK  $L  attempt=$attempt" \
              | tee -a "$MASTER_LOG"
            return 0
        else
            local EC=$?
            echo "  $(date '+%H:%M:%S')  EXIT_ERR $L  attempt=$attempt  exit=$EC  (will retry from checkpoint)" \
              | tee -a "$MASTER_LOG"
            # Defensive sleep to avoid hot loop on instant-fail
            sleep 5
        fi
    done
}

# Launch all in parallel
declare -A PIDS
for L in "${LEGENDS[@]}"; do
    echo "  $(date '+%H:%M:%S')  LAUNCH   $L  →  $LOG_DIR/${L}.log" | tee -a "$MASTER_LOG"
    run_legend "$L" &
    PIDS[$L]=$!
done

echo "" | tee -a "$MASTER_LOG"
echo "All ${#LEGENDS[@]} legend wrappers running. PIDs: ${PIDS[*]}" | tee -a "$MASTER_LOG"
echo "" | tee -a "$MASTER_LOG"
echo "Monitor:  tail -f $MASTER_LOG" | tee -a "$MASTER_LOG"
echo "Progress: tail -f $LOG_DIR/*.log | grep -E 'iter|loss|PROMOTE|REJECT|ERROR'" | tee -a "$MASTER_LOG"
echo "" | tee -a "$MASTER_LOG"

# Block until all complete (in interruptable mode they may take
# multiple attempts).
for L in "${LEGENDS[@]}"; do
    wait "${PIDS[$L]}" || true
done

echo "" | tee -a "$MASTER_LOG"
echo "════════════════════════════════════════════════════════════════════════════════"  | tee -a "$MASTER_LOG"
echo "All legends complete. Validating final checkpoints ..." | tee -a "$MASTER_LOG"
echo "════════════════════════════════════════════════════════════════════════════════" | tee -a "$MASTER_LOG"

for L in "${LEGENDS[@]}"; do
    CFG="configs/gpu_bootstrap_${L}.json"
    CKPT_DIR=$(jq -r '.training.checkpoint_dir' "$CFG")
    if [ -f "$CKPT_DIR/best.pt" ]; then
        CKPT="$CKPT_DIR/best.pt"
    else
        CKPT=$(ls -t "$CKPT_DIR"/iter_*.pt 2>/dev/null | head -1)
    fi
    if [ -z "$CKPT" ] || [ ! -f "$CKPT" ]; then
        echo "  $L: NO CHECKPOINT" | tee -a "$MASTER_LOG"
        continue
    fi
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
