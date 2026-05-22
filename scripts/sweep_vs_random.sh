#!/usr/bin/env bash
# Sweep a legend's checkpoints against Random to find where training
# regressed. Prints a one-line summary per iter. Cheap discriminator
# (matchup 1 only); use scripts/validate_checkpoint.sh for the full
# 3-matchup validity bar once a candidate is picked.
#
# Usage:
#   bash scripts/sweep_vs_random.sh <legend> <deck.json>
# Example:
#   bash scripts/sweep_vs_random.sh stable_khazix decks/khazix_test.json
#
# Optional env:
#   GAMES=20    games per matchup
#   MODEL_SIMS=10
#   SEED=42

set -uo pipefail

REPO="${REPO:-/home/pirateporkchop/code/automated-riftbound}"
GAMES="${GAMES:-20}"
MODEL_SIMS="${MODEL_SIMS:-10}"
SEED="${SEED:-42}"
LEGEND="${1:?legend dir required (e.g. stable_khazix)}"
DECK="${2:?deck path required}"

cd "$REPO"
BIN="./build-torch/src/openspiel/riftbound_openspiel"
mkdir -p logs

printf "Sweep: %s vs random  (games=%d, model_sims=%d, seed=%d)\n\n" \
    "$LEGEND" "$GAMES" "$MODEL_SIMS" "$SEED"
printf "  %-8s  %-12s  %s\n" "iter" "P1_win%" "log"
printf "  %-8s  %-12s  %s\n" "----" "-------" "---"

for ckpt in $(ls "checkpoints/${LEGEND}"/iter_*.pt 2>/dev/null \
              | sort -t_ -k2 -n); do
    iter=$(basename "$ckpt" .pt | sed 's/iter_//')
    log="logs/sweep_${LEGEND}_iter_${iter}.log"
    timeout 900 "$BIN" \
        --agent1 "model:sims=${MODEL_SIMS},path=${ckpt}" \
        --agent2 "random" \
        --deck1 "$DECK" --deck2 "$DECK" \
        --games "$GAMES" --seed "$SEED" --threads 1 \
        > "$log" 2>&1
    wr=$(grep -E "^  P1 wins" "$log" | head -1 | grep -oE "[0-9]+%" | tr -d '%')
    [[ -z "$wr" ]] && wr="?"
    printf "  iter_%-4s  %-12s  %s\n" "$iter" "${wr}%" "$log"
done
