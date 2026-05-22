#!/usr/bin/env bash
# Phase 6k-3 — validate a V2 checkpoint against the "valid model" bar.
#
# Usage:
#   bash scripts/validate_checkpoint.sh <ckpt.pt> <deck.json>
#
# Runs 3 matchups (N=20 games each for speed; bump GAMES env for tighter):
#   1. model+MCTS vs random
#   2. model+MCTS vs MCTS:sims=10
#   3. model:sims=10 vs model:sims=1   (search-helps sanity)
#
# Pass criteria (must hit ALL):
#   1. >= 60% overall vs random (P1 has ~60% first-mover advantage, so
#      model must at least match it when also playing P1)
#   2. >= 45% vs MCTS:sims=10 (don't lose badly to pure-search MCTS)
#   3. >= 55% on model:sims=10 vs model:sims=1 (search helps)
#
# Exit 0 on PASS, 1 on FAIL.

set -euo pipefail

REPO="${REPO:-/home/pirateporkchop/code/automated-riftbound}"
GAMES="${GAMES:-20}"
MODEL_SIMS="${MODEL_SIMS:-10}"
MCTS_SIMS="${MCTS_SIMS:-10}"
SEED="${SEED:-42}"

CKPT="${1:?ckpt path required}"
DECK="${2:?deck path required}"

cd "$REPO"
BIN="./build-torch/src/openspiel/riftbound_openspiel"

if [[ ! -f "$CKPT" ]]; then
    echo "ERROR: checkpoint not found: $CKPT" >&2
    exit 2
fi

# Helper: run a matchup, return P1 win rate as integer percent.
win_rate() {
    local agent1="$1" agent2="$2" tag="$3"
    # Include legend dir in the tag so parallel runs on iter_N.pt from
    # different legends don't stomp on the same log file (e.g.
    # checkpoints/stable_khazix/iter_19.pt and stable_rengar/iter_19.pt
    # both produce basename "iter_19" — bug found 2026-05-18).
    local legend
    legend=$(basename "$(dirname "$CKPT")")
    local log="logs/validate_${tag}_${legend}_$(basename "$CKPT" .pt).log"
    timeout 600 "$BIN" \
        --agent1 "$agent1" --agent2 "$agent2" \
        --deck1 "$DECK" --deck2 "$DECK" \
        --games "$GAMES" --seed "$SEED" --threads 1 \
        > "$log" 2>&1
    grep -E "^  P1 wins" "$log" | head -1 | grep -oE "[0-9]+%" | tr -d '%'
}

echo "Validating: $CKPT"
echo "Deck:       $DECK"
echo "Games:      $GAMES  (seed=$SEED)"
echo

mkdir -p logs

wr_random=$(win_rate "model:sims=$MODEL_SIMS,path=$CKPT" "random" "vs_random")
echo "Matchup 1 — model+MCTS vs Random:    P1 win rate = ${wr_random}%   (need >= 60)"

wr_mcts=$(win_rate "model:sims=$MODEL_SIMS,path=$CKPT" "mcts:sims=$MCTS_SIMS" "vs_mcts")
echo "Matchup 2 — model+MCTS vs MCTS:s=$MCTS_SIMS: P1 win rate = ${wr_mcts}%   (need >= 45)"

wr_self=$(win_rate "model:sims=$MODEL_SIMS,path=$CKPT" "model:sims=1,path=$CKPT" "search_helps")
echo "Matchup 3 — model:s=$MODEL_SIMS vs model:s=1:    P1 win rate = ${wr_self}%   (need >= 55)"

pass=true
if (( wr_random < 60 )); then echo "  FAIL: vs random < 60%";              pass=false; fi
if (( wr_mcts   < 45 )); then echo "  FAIL: vs MCTS < 45%";                pass=false; fi
if (( wr_self   < 55 )); then echo "  FAIL: model:s=10 vs model:s=1 < 55%"; pass=false; fi

if $pass; then
    echo
    echo "PASS — checkpoint is valid"
    exit 0
else
    echo
    echo "FAIL — checkpoint does not meet validity bar"
    exit 1
fi
