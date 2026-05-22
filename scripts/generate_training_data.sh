#!/bin/bash
# Generate training data for ML agent training.
#
# Usage:
#   ./scripts/generate_training_data.sh <deck1.json> <deck2.json> [num_games] [threads]
#
# Example (10K MF mirror games on 8 threads):
#   ./scripts/generate_training_data.sh decks/miss_fortune_test.json decks/miss_fortune_test.json 10000 8
#
# Output:
#   training_data/<legend_name>_vs_<legend_name>/game.bin.gameN
#   (one .bin file per game, read directly by train_agent.py)

set -e

DECK1="${1:?Usage: $0 <deck1.json> <deck2.json> [num_games] [threads]}"
DECK2="${2:?Usage: $0 <deck1.json> <deck2.json> [num_games] [threads]}"
NUM_GAMES="${3:-10000}"
THREADS="${4:-$(nproc)}"

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
REGISTRY="$PROJECT_ROOT/cards/registry.json"

# Build Release if not already built
echo "=== Building Release ==="
cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "$PROJECT_ROOT" 2>&1 | tail -3
cmake --build "$BUILD_DIR" 2>&1 | tail -3

# Extract legend names for output directory
# Legend field can be a dict {card_id, name, ...} or a plain integer
LEGEND1=$(python3 -c "
import json
d=json.load(open('$DECK1'))
leg = d['legend']
name = leg['name'] if isinstance(leg, dict) else json.load(open('$REGISTRY'))['cards'][[c['card_id'] for c in json.load(open('$REGISTRY'))['cards']].index(leg)]['name']
print(name.lower().replace(' ','_').replace(',',''))
")
LEGEND2=$(python3 -c "
import json
d=json.load(open('$DECK2'))
leg = d['legend']
name = leg['name'] if isinstance(leg, dict) else json.load(open('$REGISTRY'))['cards'][[c['card_id'] for c in json.load(open('$REGISTRY'))['cards']].index(leg)]['name']
print(name.lower().replace(' ','_').replace(',',''))
")

OUTPUT_DIR="$PROJECT_ROOT/training_data/${LEGEND1}_vs_${LEGEND2}"
mkdir -p "$OUTPUT_DIR"

echo "=== Generating $NUM_GAMES games ==="
echo "  Deck 1: $DECK1 ($LEGEND1)"
echo "  Deck 2: $DECK2 ($LEGEND2)"
echo "  Threads: $THREADS"
echo "  Output: $OUTPUT_DIR/"

cd "$BUILD_DIR"
time RIFTBOUND_ROOT="$PROJECT_ROOT" ./riftbound \
    "$PROJECT_ROOT/$DECK1" \
    "$PROJECT_ROOT/$DECK2" \
    -r "$REGISTRY" \
    --games "$NUM_GAMES" \
    --threads "$THREADS" \
    -o "$OUTPUT_DIR/game.bin"

# Count output files
NUM_FILES=$(ls "$OUTPUT_DIR"/game.bin.game* 2>/dev/null | wc -l)
echo ""
echo "=== Done ==="
echo "  Generated: $NUM_FILES game files"
echo "  Location: $OUTPUT_DIR/"
echo "  Total size: $(du -sh "$OUTPUT_DIR" | cut -f1)"
