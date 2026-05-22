#!/usr/bin/env bash
# Print ETA for in-flight stable_* training runs by parsing per-iter
# wallclock from the training log.
#
# Usage:
#   bash scripts/training_eta.sh                # all stable_* legends
#   bash scripts/training_eta.sh khazix         # just one
#   bash scripts/training_eta.sh khazix rengar  # named subset
#
# Looks at logs/train_stable_<legend>.log for completed iters and
# reads iterations count from configs/stable_<legend>.json. Reports
# average per-iter wallclock and projected ETA for the remainder.

set -u

REPO="${REPO:-/workspace/riftbound}"
[ -d "$REPO" ] || REPO="$(cd "$(dirname "$0")/.." && pwd)"

# Pick which legends to report on.
# Default: only legends that have an existing log file (i.e., a run
# actually started). Explicit args override.
if [ $# -gt 0 ]; then
    legends=("$@")
else
    legends=()
    for log in "$REPO"/logs/train_stable_*.log; do
        [ -f "$log" ] || continue
        legend=$(basename "$log" .log)
        legend="${legend#train_stable_}"
        legends+=("$legend")
    done
    if [ ${#legends[@]} -eq 0 ]; then
        echo "No training logs found under $REPO/logs/train_stable_*.log"
        echo "Pass legend names as args to force, or check that runs were launched."
        exit 0
    fi
fi

show_one() {
    local legend="$1"
    local log="$REPO/logs/train_stable_${legend}.log"
    local cfg="$REPO/configs/stable_${legend}.json"

    echo "=== $legend ==="
    if [ ! -f "$log" ]; then
        echo "  no log at $log"
        return
    fi

    # Per-iter self-play wallclock — appended in parens like "(45.2s)"
    local times
    times=$(grep 'self-play' "$log" 2>/dev/null | grep -oE '\([0-9.]+s\)' | tr -d 's()')

    if [ -z "$times" ]; then
        echo "  no iter completed yet"
        tail -3 "$log" | sed 's/^/  /'
        echo
        return
    fi

    # Per-iter train wallclock — also in parens after "train:" line
    local train_times
    train_times=$(grep '  train:' "$log" 2>/dev/null | grep -oE '\([0-9.]+s\)' | tr -d 's()')

    # Sum + avg
    local n=0 sum_sp=0 sum_tr=0
    for t in $times;       do sum_sp=$(echo "$sum_sp + $t" | bc); n=$((n+1)); done
    for t in $train_times; do sum_tr=$(echo "$sum_tr + $t" | bc); done
    local avg_iter
    avg_iter=$(echo "scale=1; ($sum_sp + $sum_tr) / $n" | bc)

    # Total iters from config
    local total=20
    if [ -f "$cfg" ]; then
        local cfg_iters
        cfg_iters=$(grep '"iterations"' "$cfg" | head -1 | grep -oE '[0-9]+')
        [ -n "$cfg_iters" ] && total="$cfg_iters"
    fi

    local iters_started
    iters_started=$(grep -c '=== iter' "$log")
    local remaining=$((total - n))

    local eta_min eta_secs
    eta_secs=$(echo "scale=0; $avg_iter * $remaining / 1" | bc)
    eta_min=$(echo "scale=1; $eta_secs / 60" | bc)

    printf "  completed: %d / %d iters\n" "$n" "$total"
    printf "  avg per iter: %ss (self-play %ss + train %ss across %d iters)\n" \
        "$avg_iter" \
        "$(echo "scale=1; $sum_sp / $n" | bc)" \
        "$(echo "scale=1; $sum_tr / $n" | bc)" "$n"
    printf "  remaining: %d iters → ETA %s min (%ss)\n" \
        "$remaining" "$eta_min" "$eta_secs"
    if [ "$iters_started" -gt "$n" ]; then
        echo "  currently inside iter $((iters_started - 1))"
    fi
    echo
}

for legend in "${legends[@]}"; do
    show_one "$legend"
done

# GPU snapshot (only meaningful on the box that has the GPU)
if command -v nvidia-smi >/dev/null 2>&1; then
    echo "GPU:"
    nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader \
        | sed 's/^/  /'
fi
