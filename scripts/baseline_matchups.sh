#!/usr/bin/env bash
# Phase 6k baseline matchups — pit each trained legend's checkpoint
# against random and MCTS in 1v1 mirror matches, report win rates.
#
# Usage:
#   bash scripts/baseline_matchups.sh GLOOMIST_CKPT VIRTUOSO_CKPT
#
# Default games-per-matchup is 20 (override via $GAMES env var).
# Default MCTS opponent sim count is 10 (override via $MCTS_SIMS).

set -euo pipefail

REPO="${REPO:-/home/pirateporkchop/code/automated-riftbound}"
GAMES="${GAMES:-20}"
MCTS_SIMS="${MCTS_SIMS:-10}"
MODEL_SIMS="${MODEL_SIMS:-10}"

GLOOMIST_CKPT="${1:-$REPO/checkpoints/baseline_gloomist/iter_14.pt}"
VIRTUOSO_CKPT="${2:-$REPO/checkpoints/baseline_virtuoso/iter_14.pt}"

cd "$REPO"
BIN="./build-torch/src/openspiel/riftbound_openspiel"

run_match() {
    local label="$1"; shift
    local agent1="$1"; shift
    local agent2="$1"; shift
    local deck1="$1"; shift
    local deck2="$1"; shift
    echo "=== $label ==="
    timeout 600 "$BIN" \
        --agent1 "$agent1" --agent2 "$agent2" \
        --deck1 "$deck1" --deck2 "$deck2" \
        --games "$GAMES" --seed 42 --threads 1 2>&1 | tail -7
    echo
}

if [[ -f "$GLOOMIST_CKPT" ]]; then
    echo "############################################################"
    echo "# Gloomist baselines (checkpoint: $GLOOMIST_CKPT)"
    echo "############################################################"
    run_match "Gloomist (model) vs Random" \
        "model:sims=$MODEL_SIMS,path=$GLOOMIST_CKPT" \
        "random" \
        "decks/vex_pre_con.json" "decks/vex_pre_con.json"
    run_match "Gloomist (model) vs MCTS:sims=$MCTS_SIMS" \
        "model:sims=$MODEL_SIMS,path=$GLOOMIST_CKPT" \
        "mcts:sims=$MCTS_SIMS" \
        "decks/vex_pre_con.json" "decks/vex_pre_con.json"
else
    echo "Gloomist checkpoint not found: $GLOOMIST_CKPT — skipping"
fi

if [[ -f "$VIRTUOSO_CKPT" ]]; then
    echo "############################################################"
    echo "# Virtuoso baselines (checkpoint: $VIRTUOSO_CKPT)"
    echo "############################################################"
    run_match "Virtuoso (model) vs Random" \
        "model:sims=$MODEL_SIMS,path=$VIRTUOSO_CKPT" \
        "random" \
        "decks/jhin_test.json" "decks/jhin_test.json"
    run_match "Virtuoso (model) vs MCTS:sims=$MCTS_SIMS" \
        "model:sims=$MODEL_SIMS,path=$VIRTUOSO_CKPT" \
        "mcts:sims=$MCTS_SIMS" \
        "decks/jhin_test.json" "decks/jhin_test.json"
else
    echo "Virtuoso checkpoint not found: $VIRTUOSO_CKPT — skipping"
fi

echo "============================================================"
echo "BASELINE: random vs random + MCTS vs random for context"
echo "============================================================"
run_match "Random vs Random (Gloomist mirror)" \
    "random" "random" \
    "decks/vex_pre_con.json" "decks/vex_pre_con.json"
run_match "MCTS:sims=$MCTS_SIMS vs Random (Gloomist mirror)" \
    "mcts:sims=$MCTS_SIMS" "random" \
    "decks/vex_pre_con.json" "decks/vex_pre_con.json"
