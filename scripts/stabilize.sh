#!/usr/bin/env bash
# Phase 6k-5 — unsupervised driver to land "valid model" checkpoints.
#
# Pipeline:
#   1. Diagnostic sweep on baseline_* checkpoints (already done if logs/diag/ exists).
#   2. Train stable_gloomist + stable_virtuoso in parallel
#      (configs/stable_*.json).
#   3. Validate each final checkpoint via scripts/validate_checkpoint.sh.
#   4. Generate logs/baseline_report.md with the matchup tables.
#
# Usage: bash scripts/stabilize.sh
# All output goes to logs/. Idempotent — re-running skips steps whose
# artifacts already exist (unless FORCE=1).

set -euo pipefail

REPO="${REPO:-/home/pirateporkchop/code/automated-riftbound}"
FORCE="${FORCE:-0}"
cd "$REPO"
mkdir -p logs checkpoints

# ── Step 1: diagnostic sweep ────────────────────────────────────────────
if [[ "$FORCE" == "1" ]] || [[ ! -s logs/diagnostic_sweep_summary.log ]]; then
    echo "=== Step 1: diagnostic sweep ==="
    bash scripts/diagnostic_sweep.sh > logs/diagnostic_sweep_summary.log 2>&1
fi
echo "Diagnostic summary:"
tail -20 logs/diagnostic_sweep_summary.log
echo

# ── Step 2: stable training (two legends in parallel) ───────────────────
need_train=0
[[ -f checkpoints/stable_gloomist/iter_29.pt ]] || need_train=1
[[ -f checkpoints/stable_virtuoso/iter_29.pt ]] || need_train=1
if [[ "$FORCE" == "1" || "$need_train" == "1" ]]; then
    echo "=== Step 2: stable training (parallel, ~2 hr expected) ==="
    rm -rf checkpoints/stable_gloomist checkpoints/stable_virtuoso
    ./build-torch/riftbound_runner --config configs/stable_gloomist.json \
        > logs/train_stable_gloomist.log 2>&1 &
    GLOOMIST_PID=$!
    ./build-torch/riftbound_runner --config configs/stable_virtuoso.json \
        > logs/train_stable_virtuoso.log 2>&1 &
    VIRTUOSO_PID=$!
    echo "  Gloomist PID=$GLOOMIST_PID  Virtuoso PID=$VIRTUOSO_PID"
    wait $GLOOMIST_PID || echo "  Gloomist exited non-zero (continuing)"
    wait $VIRTUOSO_PID || echo "  Virtuoso exited non-zero (continuing)"
    echo "  Training done."
fi
echo

# ── Step 3: validation ──────────────────────────────────────────────────
echo "=== Step 3: validation ==="
declare -A DECK_FOR
DECK_FOR["stable_gloomist"]="decks/vex_pre_con.json"
DECK_FOR["stable_virtuoso"]="decks/jhin_test.json"

declare -A PASS_FAIL
for legend in stable_gloomist stable_virtuoso; do
    deck="${DECK_FOR[$legend]}"
    # Pick the latest available iter checkpoint.
    ckpt=$(ls -t checkpoints/$legend/iter_*.pt 2>/dev/null | head -1 || true)
    if [[ -z "$ckpt" ]]; then
        echo "$legend: no checkpoint found — SKIP"
        PASS_FAIL[$legend]="MISSING"
        continue
    fi
    echo "--- $legend: $ckpt ---"
    if bash scripts/validate_checkpoint.sh "$ckpt" "$deck" \
        > "logs/validate_${legend}.log" 2>&1; then
        PASS_FAIL[$legend]="PASS"
        echo "  PASS"
    else
        PASS_FAIL[$legend]="FAIL"
        echo "  FAIL"
    fi
    tail -7 "logs/validate_${legend}.log" | sed 's/^/    /'
done

# ── Step 4: baseline report ─────────────────────────────────────────────
echo
echo "=== Step 4: baseline report ==="
REPORT="logs/baseline_report.md"
{
    echo "# V2 Baseline Report — $(date -Iseconds)"
    echo
    echo "## Validation summary"
    echo
    echo "| Legend | Result |"
    echo "|---|---|"
    for legend in stable_gloomist stable_virtuoso; do
        echo "| $legend | ${PASS_FAIL[$legend]:-?} |"
    done
    echo
    echo "## Per-checkpoint matchup details"
    for legend in stable_gloomist stable_virtuoso; do
        echo
        echo "### $legend"
        if [[ -f "logs/validate_${legend}.log" ]]; then
            echo '```'
            cat "logs/validate_${legend}.log"
            echo '```'
        fi
    done
    echo
    echo "## Diagnostic sweep (per-iter trend, baseline_* checkpoints)"
    echo '```'
    tail -20 logs/diagnostic_sweep_summary.log 2>/dev/null || echo "(missing)"
    echo '```'
} > "$REPORT"
echo "Wrote $REPORT"
cat "$REPORT"

# Overall exit code: 0 if both legends PASS, 1 otherwise.
overall=0
for legend in stable_gloomist stable_virtuoso; do
    [[ "${PASS_FAIL[$legend]:-}" == "PASS" ]] || overall=1
done
exit $overall
