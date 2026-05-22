#!/usr/bin/env bash
# Phase 6m+n+o — parallel stable_* training launcher.
#
# Runs each named legend's configs/stable_<legend>.json concurrently,
# each on its own thread pool of self-play workers. Per-legend logs
# are preserved AND a master combined log is written so you can tail
# one file to monitor all.
#
# Outputs:
#   logs/train_stable_<legend>.log      — per-legend
#   logs/train_stable_master.log        — combined, prefixed [legend]
#
# Usage (from repo root):
#   bash scripts/train_stable_parallel.sh [legend1 legend2 ...]
#
#   Defaults to: rengar miss_fortune
#
# Examples:
#   bash scripts/train_stable_parallel.sh                       # rengar+mf
#   bash scripts/train_stable_parallel.sh khazix rengar          # custom
#   bash scripts/train_stable_parallel.sh rengar                 # single
#
# Monitor:
#   tail -f logs/train_stable_master.log
#
# Stop everything:
#   pkill -9 -f riftbound_runner

set -u

REPO="${REPO:-/workspace/riftbound}"
[ -d "$REPO" ] || REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

BIN="./build-torch/riftbound_runner"
if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found. Build it with:" >&2
    echo "  cmake --build build-torch --target riftbound_runner" >&2
    exit 1
fi

mkdir -p logs checkpoints
: > logs/train_stable_master.log

launch_legend() {
    local legend="$1"
    local cfg="configs/stable_${legend}.json"
    local log="logs/train_stable_${legend}.log"

    if [ ! -f "$cfg" ]; then
        echo "WARN: no config at $cfg, skipping $legend" >&2
        return
    fi

    : > "$log"

    # Pipeline:
    #   stdbuf -oL   — force line-buffering on the runner so output
    #                  isn't lost in the pipe block buffer when redirected
    #   sed          — prefix every line with [legend]
    #   tee -a       — write to per-legend log AND pass to master
    #   >> master    — O_APPEND with line-sized writes is atomic on Linux
    #                  (PIPE_BUF=4096), so concurrent writes from two
    #                  legends don't interleave at byte boundaries
    setsid nohup bash -c "
        stdbuf -oL '$BIN' --config '$cfg' 2>&1 |
        stdbuf -oL sed 's/^/[$legend] /' |
        tee -a '$log' >> logs/train_stable_master.log
    " < /dev/null > /dev/null 2>&1 &

    local pid=$!
    echo "  $legend  PID=$pid  log=$log"
}

if [ $# -gt 0 ]; then
    legends=("$@")
else
    legends=(rengar miss_fortune)
fi

echo "Launching parallel stable training for: ${legends[*]}"
for legend in "${legends[@]}"; do
    launch_legend "$legend"
done

echo
echo "Master log:    tail -f $REPO/logs/train_stable_master.log"
echo "Per-legend:    tail -f $REPO/logs/train_stable_*.log"
echo "Stop all:      pkill -9 -f riftbound_runner"
echo
echo "PIDs:"
pgrep -af riftbound_runner | head -4 | sed 's/^/  /'
