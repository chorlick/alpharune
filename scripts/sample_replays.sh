#!/usr/bin/env bash
# Sample replays from a trained checkpoint.
#
# Generates HTML replay files of the model playing itself (default) or
# against a chosen opponent — useful for spot-checking trained-model
# behavior without perturbing training. Uses riftbound_openspiel's
# --render-html path, so games are played deterministically by the
# model (BestChild over MCTS visits, no Dirichlet noise — what the
# model "actually thinks").
#
# Usage:
#   bash scripts/sample_replays.sh <ckpt.pt> [game_count] [opponent_spec]
#
# Examples:
#   # 5 games of iter_29 model playing itself
#   bash scripts/sample_replays.sh checkpoints/stable_khazix/iter_29.pt
#
#   # 10 games against random
#   bash scripts/sample_replays.sh checkpoints/stable_khazix/iter_29.pt 10 random
#
#   # 5 games against MCTS:sims=20 (no model)
#   bash scripts/sample_replays.sh checkpoints/stable_khazix/iter_29.pt 5 mcts:sims=20
#
# Outputs:
#   replays/<legend>_<iter>/replay_*.html
#   Each HTML is self-contained — open in any browser; arrow keys
#   navigate decision-by-decision, click card names for art.

set -euo pipefail

REPO="${REPO:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$REPO"

CKPT="${1:?usage: $0 <ckpt.pt> [game_count] [opponent_spec]}"
GAMES="${2:-5}"
OPPONENT="${3:-model:sims=40,path=$CKPT}"   # default: mirror
SIMS="${SIMS:-40}"
SEED="${SEED:-42}"

if [ ! -f "$CKPT" ]; then
    echo "ERROR: checkpoint not found: $CKPT" >&2
    exit 1
fi

# Derive output dir from checkpoint path. e.g.
# checkpoints/stable_khazix/iter_29.pt → replays/stable_khazix_iter_29/
LEGEND_DIR=$(basename "$(dirname "$CKPT")")
ITER=$(basename "$CKPT" .pt)
OUTDIR="replays/${LEGEND_DIR}_${ITER}"

# Map legend dir to deck path. Override with DECK=... env var if your
# legend dir name doesn't match a deck file.
case "$LEGEND_DIR" in
    *khazix*)   DECK="${DECK:-decks/khazix_test.json}";;
    *rengar*)   DECK="${DECK:-decks/rengar_test.json}";;
    *jhin*)     DECK="${DECK:-decks/jhin_test.json}";;
    *gloomist*) DECK="${DECK:-decks/leblanc_test.json}";;  # historical
    *)          DECK="${DECK:?DECK env var required for legend dir '$LEGEND_DIR'}";;
esac

if [ ! -f "$DECK" ]; then
    echo "ERROR: deck not found: $DECK" >&2
    exit 1
fi

mkdir -p "$OUTDIR"

BIN="./build-torch/src/openspiel/riftbound_openspiel"
[ -x "$BIN" ] || { echo "ERROR: $BIN not built. Configure with -DRIFTBOUND_BUILD_OPENSPIEL=ON" >&2; exit 1; }

echo "Sampling $GAMES games:"
echo "  ckpt:     $CKPT"
echo "  deck:     $DECK"
echo "  agent1:   model:sims=$SIMS,path=$CKPT"
echo "  agent2:   $OPPONENT"
echo "  outdir:   $OUTDIR"
echo

# riftbound_openspiel writes HTMLs into ./replays/ by default. Run from
# that dir then move the produced files into the per-checkpoint dir.
"$BIN" \
    --agent1 "model:sims=$SIMS,path=$CKPT" \
    --agent2 "$OPPONENT" \
    --deck1 "$DECK" --deck2 "$DECK" \
    --games "$GAMES" --seed "$SEED" --threads 1 \
    --render-html

# Move the just-produced replays into the per-checkpoint folder.
# Heuristic: any replay_*.html older than this script's start time stays;
# newer ones move. Using a marker file is more robust.
moved=0
for f in replays/replay_*.html; do
    [ -f "$f" ] || continue
    # Move only if recently created (last 5 minutes — safe margin)
    if [ $(( $(date +%s) - $(stat -c %Y "$f") )) -lt 300 ]; then
        mv "$f" "$OUTDIR/"
        moved=$((moved+1))
    fi
done

echo
echo "Wrote $moved replays to $OUTDIR/"
ls -la "$OUTDIR/"
