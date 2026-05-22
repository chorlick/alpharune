#!/usr/bin/env bash
# Phase 6k-1 diagnostic sweep — for each saved checkpoint of each legend,
# run model+MCTS vs random for N games and report win rate. The goal is
# to localize WHEN training broke (monotone decline vs sudden cliff).
#
# Usage:
#   bash scripts/diagnostic_sweep.sh
#
# Tunables via env:
#   GAMES         (default 20)
#   MODEL_SIMS    (default 5)
#   SEED          (default 42)

set -euo pipefail

REPO="${REPO:-/home/pirateporkchop/code/automated-riftbound}"
GAMES="${GAMES:-20}"
MODEL_SIMS="${MODEL_SIMS:-5}"
SEED="${SEED:-42}"

cd "$REPO"
BIN="./build-torch/src/openspiel/riftbound_openspiel"
mkdir -p logs/diag

declare -A DECK_FOR
DECK_FOR["baseline_gloomist"]="decks/vex_pre_con.json"
DECK_FOR["baseline_virtuoso"]="decks/jhin_test.json"

probe_one() {
    local legend="$1"
    local iter="$2"
    local deck="${DECK_FOR[$legend]}"
    local ckpt="$REPO/checkpoints/$legend/${iter}.pt"
    local log="logs/diag/${legend}_${iter}_vs_random.log"

    if [[ ! -f "$ckpt" ]]; then
        echo "MISSING $ckpt"
        return
    fi
    echo "RUNNING $legend $iter -> $log"
    timeout 600 "$BIN" \
        --agent1 "model:sims=$MODEL_SIMS,path=$ckpt" \
        --agent2 random \
        --deck1 "$deck" --deck2 "$deck" \
        --games "$GAMES" --seed "$SEED" --threads 1 \
        > "$log" 2>&1
    grep -E "P1 wins|P2 wins|Draws|P1 decisive" "$log" | head -4
}

# Sweep
for legend in baseline_gloomist baseline_virtuoso; do
    echo "############################################################"
    echo "# $legend — model vs random, $GAMES games, sims=$MODEL_SIMS"
    echo "############################################################"
    for iter in iter_0 iter_5 iter_10 iter_14; do
        probe_one "$legend" "$iter"
        echo
    done
done

# Summary table
echo
echo "============================================================"
echo "SUMMARY: P1 (model) win rate vs random — per iter per legend"
echo "============================================================"
printf "%-25s %-10s %-10s %-10s %-10s\n" "Legend" "iter_0" "iter_5" "iter_10" "iter_14"
for legend in baseline_gloomist baseline_virtuoso; do
    row="$legend"
    for iter in iter_0 iter_5 iter_10 iter_14; do
        log="logs/diag/${legend}_${iter}_vs_random.log"
        if [[ -f "$log" ]]; then
            wr=$(grep -E "^  P1 wins" "$log" | head -1 | grep -oE "[0-9]+%")
            row="$row|$wr"
        else
            row="$row|--"
        fi
    done
    echo "$row" | awk -F"|" '{printf "%-25s %-10s %-10s %-10s %-10s\n", $1, $2, $3, $4, $5}'
done
