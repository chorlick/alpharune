#!/bin/bash
# Rengar-mirror self-play loop with gated promotion. Same machinery as
# scripts/self_play_loop.sh, but Rengar vs Rengar only — no Miss Fortune.
#
# Why mirror instead of cross-archetype: training matchup = eval matchup =
# deployment matchup, so the gradient signal directly reflects what the model
# is being tested on. AlphaZero/AlphaGo all do pure mirror self-play.
#
# NOTE: this script writes to models/rengar/candidate.{pt,onnx} — the SAME
# paths the cross-archetype self_play_loop.sh uses. Don't run both at the
# same time, or they'll clobber each other. They DO share the same v00N
# checkpoint pool, so progress made by one loop is picked up by the other.
#
# Run from repo root after activating the riftbound conda env:
#   conda activate riftbound
#   chmod +x scripts/self_play_loop_rengar.sh
#   ./scripts/self_play_loop_rengar.sh
#
# Each iteration:
#   1. Generates Rengar vs Rengar self-play data (T=1.0)
#   2. Trains Rengar from the current best (REINFORCE) → candidate.{pt,onnx}
#   3. Benchmarks the candidate vs current best across 2 mirror seats
#   4. Promotes if decisive win rate ≥ PROMOTE_PCT (55%), else rejects
#   5. Cleans up the iter's .bin data
#
# Outputs: models/rengar/v<N>.{pt,onnx} per promoted iteration.
# All stdout/stderr → logs/rengar_self_play_<timestamp>_partNNN.log + terminal.
# Log files rotate at 50 MB and gzip in the background.
# Loops forever — Ctrl-C to stop.

set -e

# Resolve python: prefer the riftbound conda env's interpreter directly so the
# loop works even when launched outside an activated shell (e.g. via nohup).
PYTHON_BIN="${HOME}/miniconda3/envs/riftbound/bin/python"
if [ ! -x "$PYTHON_BIN" ]; then PYTHON_BIN=python; fi

# Kill any background children if the script exits, is Ctrl-C'd, or is
# kill'd externally. Without this, the python training job we launch with
# `&` gets reparented to init when bash dies and keeps running, competing
# with the next run for GPU/disk.
cleanup_children() {
    pkill -TERM -P $$ 2>/dev/null || true
    sleep 1
    pkill -KILL -P $$ 2>/dev/null || true
}
trap cleanup_children EXIT
trap 'cleanup_children; exit 130' INT TERM

GAMES=500            # self-play games per iter (training data)
EVAL_GAMES=100       # base per-seat eval games (×2 seats = 200 total)
THREADS=8
EPOCHS=1
LR=1e-4
ENTROPY=0.03         # base entropy coefficient (knob-turned when plateauing)
BATCH=256
PROMOTE_PCT=55       # min decisive win rate (integer %) to promote
GPU=0                # which GPU to train on

# Plateau-breaking schedule. After N consecutive rejected iterations we
# crank TWO knobs:
#   - entropy coefficient (forces exploration during training)
#   - eval game count   (tightens the statistical gate near threshold)
# Both reset to 1× immediately after any successful promotion.
ENTROPY_MULT_2=2     # ×2 entropy after 2 consecutive rejects
ENTROPY_MULT_3=4     # ×4 after 4
ENTROPY_MULT_4=8     # ×8 after 6 (cap)
EVAL_MULT_2=2        # ×2 eval games after 2 consecutive rejects
EVAL_MULT_3=4        # ×4 after 4
EVAL_MULT_4=8        # ×8 after 6 (cap)

# ─── Master log setup with size-based rotation ──────────────────────────────
# Each part rolls to a new .log file at LOG_MAX_BYTES (50 MB). Closed parts
# are gzipped in the background. At most MAX_GZ_FILES (100) .gz files are
# kept; older ones get pruned. Terminal still gets live output via tee.
# Read with `zcat logs/rengar_self_play_*.log.gz | less` (sorted = chronological).
mkdir -p logs
RUN_ID=$(date +%Y%m%d_%H%M%S)
LOG_DIR=logs
LOG_MAX_BYTES=$((50 * 1024 * 1024))   # 50 MB per part
MAX_GZ_FILES=100                       # cap on total compressed log files

log_part=1
LOG="$LOG_DIR/rengar_self_play_${RUN_ID}_part$(printf %03d "$log_part").log"
exec > >(tee -a "$LOG") 2>&1

rotate_log_if_needed() {
    local sz
    sz=$(stat -c%s "$LOG" 2>/dev/null || echo 0)
    if [ "$sz" -le "$LOG_MAX_BYTES" ]; then return; fi

    local old_log="$LOG"
    log_part=$((log_part + 1))
    LOG="$LOG_DIR/rengar_self_play_${RUN_ID}_part$(printf %03d "$log_part").log"
    exec > >(tee -a "$LOG") 2>&1
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [logrotate] closed ${old_log} (size=${sz}B), continuing in ${LOG}"
    (gzip -9 "$old_log" \
        && ls -t "$LOG_DIR"/rengar_self_play_*.log.gz 2>/dev/null \
           | tail -n +$((MAX_GZ_FILES + 1)) \
           | xargs -r rm -f) &
}

# ANSI color codes for log lines we want to scan for quickly. Embedded
# unconditionally — read .log files with `less -R` (or strip with
# `sed 's/\x1b\[[0-9;]*m//g'`) if you don't want the codes.
C_GREEN=$'\e[1;32m'   # bold green — promotes
C_RED=$'\e[1;31m'     # bold red   — rejects
C_YELLOW=$'\e[1;33m'  # bold yellow — eval summaries
C_RESET=$'\e[0m'

log() {
    # Route to stderr so `$(eval_candidate ...)` captures only the function's
    # actual return on stdout (echo 1 / echo 0), not the log messages. stderr
    # still reaches the master log + terminal via the top-level `2>&1`.
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

log "════════════════════════════════════════════════════════════════"
log "  Rengar self-play loop start (Rengar vs Rengar mirror)"
log "  Log: $LOG (rotates every ${LOG_MAX_BYTES} B, keeps last ${MAX_GZ_FILES} .gz)"
log "  GAMES=$GAMES  EVAL_GAMES=$EVAL_GAMES (×2 seats)  THREADS=$THREADS  GPU=$GPU"
log "  EPOCHS=$EPOCHS  LR=$LR  ENTROPY=$ENTROPY  BATCH=$BATCH"
log "  PROMOTE_PCT=$PROMOTE_PCT"
log "  Plateau bumps (entropy + eval games): 2 rejects → ×${ENTROPY_MULT_2}, 4 → ×${ENTROPY_MULT_3}, 6+ → ×${ENTROPY_MULT_4}"
log "════════════════════════════════════════════════════════════════"

# Adaptive entropy schedule. Args: consecutive_reject_count.
adaptive_entropy() {
    local n=$1
    local mult=1
    if   [ "$n" -ge 6 ]; then mult=$ENTROPY_MULT_4
    elif [ "$n" -ge 4 ]; then mult=$ENTROPY_MULT_3
    elif [ "$n" -ge 2 ]; then mult=$ENTROPY_MULT_2
    fi
    awk -v e="$ENTROPY" -v m="$mult" 'BEGIN { printf "%.4f", e * m }'
}

# Adaptive eval-games schedule. Args: consecutive_reject_count.
adaptive_eval_games() {
    local n=$1
    local mult=1
    if   [ "$n" -ge 6 ]; then mult=$EVAL_MULT_4
    elif [ "$n" -ge 4 ]; then mult=$EVAL_MULT_3
    elif [ "$n" -ge 2 ]; then mult=$EVAL_MULT_2
    fi
    echo $(( EVAL_GAMES * mult ))
}

# ─── Helpers ────────────────────────────────────────────────────────────────

# Run one benchmark. Echoes "P1_WINS P2_WINS DRAWS" on stdout (captured by
# the caller via $()), and streams the live engine output to stderr so it
# lands in the master log + terminal via the top-level `2>&1` tee.
# Args: deck1 deck2 agent1_spec agent2_spec games threads
bench() {
    local deck1=$1 deck2=$2 a1=$3 a2=$4 games=$5 threads=$6
    local tmpf
    tmpf=$(mktemp -t riftbound_bench.XXXXXX)
    ./build/riftbound "$deck1" "$deck2" -r cards/registry.json \
                --agent1 "$a1" --agent2 "$a2" \
                --games "$games" --threads "$threads" 2>&1 \
        | tee "$tmpf" >&2
    local p1 p2 dr
    p1=$(grep "P1 wins:" "$tmpf" | awk '{print $3}')
    p2=$(grep "P2 wins:" "$tmpf" | awk '{print $3}')
    dr=$(grep "Draws:"   "$tmpf" | awk '{print $2}')
    rm -f "$tmpf"
    echo "${p1:-0} ${p2:-0} ${dr:-0}"
}

# Mirror-eval candidate vs prev across 2 seats. Echoes 1 (promote) or 0
# (reject) and logs the breakdown.
# Args: deck candidate_onnx prev_onnx label per_seat_games
eval_candidate() {
    local deck=$1 cand=$2 prev=$3 label=$4 ev=$5

    # Seat A: candidate as P1, prev as P2
    local sA p1a p2a dra
    sA=$(bench "$deck" "$deck" "model:$cand" "model:$prev" "$ev" "$THREADS")
    read -r p1a p2a dra <<<"$sA"

    # Seat B: prev as P1, candidate as P2
    local sB p1b p2b drb
    sB=$(bench "$deck" "$deck" "model:$prev" "model:$cand" "$ev" "$THREADS")
    read -r p1b p2b drb <<<"$sB"

    local cand_wins=$(( p1a + p2b ))
    local prev_wins=$(( p2a + p1b ))
    local draws=$(( dra + drb ))
    local decisive=$(( cand_wins + prev_wins ))
    local total=$(( decisive + draws ))

    local wr=0
    if [ "$decisive" -gt 0 ]; then
        wr=$(( cand_wins * 100 / decisive ))
    fi

    log "  [eval $label] cand $cand_wins / prev $prev_wins / draws $draws"
    log "${C_YELLOW}    decisive win rate: ${wr}% (of $decisive decisive, $total total)${C_RESET}"
    log "    seat A (cand=P1): P1=$p1a P2=$p2a draws=$dra"
    log "    seat B (cand=P2): P1=$p1b P2=$p2b draws=$drb"

    if [ "$wr" -ge "$PROMOTE_PCT" ]; then
        echo 1
    else
        echo 0
    fi
}

# ─── Loop ───────────────────────────────────────────────────────────────────
# Gen counter for Rengar. Auto-detect the highest existing v00N on disk so
# crash-and-restart picks up where we left off instead of clobbering work.
latest_gen() {
    local dir=$1
    local n
    n=$(ls "$dir"/v[0-9][0-9][0-9].pt 2>/dev/null \
        | sed -n 's|.*/v0*\([0-9]\+\)\.pt|\1|p' \
        | sort -n | tail -1)
    echo "${n:-1}"
}
gen_rg=$(latest_gen models/rengar)
iter=1
rg_rejects=0
log "  Resuming with gen_rg=$gen_rg (highest existing v00N on disk)"

while true; do
    prev_rg=$(printf "v%03d" "$gen_rg")
    next_rg=$(printf "v%03d" "$((gen_rg + 1))")
    data="training_data/rengar_self_play_iter${iter}"
    iter_start=$(date +%s)

    cand_rg_pt="models/rengar/candidate.pt"
    cand_rg_onnx="models/rengar/candidate.onnx"

    log ""
    log "═══════════════════════════════════════════════════════════"
    log "  Iter $iter — Rengar best: $prev_rg"
    log "═══════════════════════════════════════════════════════════"
    log "  Disk free: $(df -h . | awk 'NR==2 {print $4}')"
    log "  Input: $(ls -la models/rengar/${prev_rg}.onnx 2>/dev/null | awk '{print $5, $6, $7, $8}')"

    # Verify input exists before we burn time on a doomed iteration
    for f in models/rengar/${prev_rg}.pt models/rengar/${prev_rg}.onnx; do
        if [ ! -f "$f" ]; then
            log "ERROR: missing input model $f — aborting."
            exit 1
        fi
    done

    mkdir -p "$data"

    # 1. Self-play data generation — Rengar vs Rengar mirror
    log "  [step 1] generating $GAMES Rengar mirror self-play games (T=1.0)..."
    step_start=$(date +%s)
    ./build/riftbound decks/rengar_test.json decks/rengar_test.json \
        -r cards/registry.json \
        --agent1 "model:models/rengar/${prev_rg}.onnx" \
        --agent2 "model:models/rengar/${prev_rg}.onnx" \
        --temp1 1.0 --temp2 1.0 \
        --games "$GAMES" --threads "$THREADS" \
        -o "$data/game.bin"
    log "  [step 1] done in $(( $(date +%s) - step_start ))s, size: $(du -sh "$data" 2>/dev/null | awk '{print $1}')"

    # 2. Train Rengar candidate from current best.
    rg_ent=$(adaptive_entropy "$rg_rejects")
    log "  [step 2] training Rengar (GPU $GPU, entropy=$rg_ent, rejects=$rg_rejects)..."
    step_start=$(date +%s)
    set +e
    "$PYTHON_BIN" scripts/train_agent.py train-rl "$data/" \
        --resume "models/rengar/${prev_rg}.pt" \
        --output "$cand_rg_pt" \
        --epochs "$EPOCHS" --lr "$LR" --entropy-coef "$rg_ent" \
        --batch-size "$BATCH" --gpu "$GPU" --dataloader-workers 3
    rg_rc=$?
    set -e
    log "  [step 2] done in $(( $(date +%s) - step_start ))s — exit=$rg_rc"

    if [ $rg_rc -ne 0 ]; then
        log "ERROR: training failed — leaving $data on disk for inspection."
        exit 1
    fi

    # 3. Evaluate candidate vs current best (mirror, T=0 argmax, 2 seats).
    rg_ev=$(adaptive_eval_games "$rg_rejects")
    log "  [step 3] evaluating — Rengar: ${rg_ev}/seat (rejects=$rg_rejects)..."
    step_start=$(date +%s)
    rg_promote=$(eval_candidate decks/rengar_test.json \
        "$cand_rg_onnx" "models/rengar/${prev_rg}.onnx" "Rengar" "$rg_ev")
    log "  [step 3] done in $(( $(date +%s) - step_start ))s"

    # 4. Promote or reject. Reset reject counter on promote; increment on reject.
    if [ "$rg_promote" -eq 1 ]; then
        mv "$cand_rg_pt"   "models/rengar/${next_rg}.pt"
        mv "$cand_rg_onnx" "models/rengar/${next_rg}.onnx"
        gen_rg=$((gen_rg + 1))
        rg_rejects=0
        log "${C_GREEN}  ✓ [promote RG] ${prev_rg} → ${next_rg}${C_RESET}"
    else
        rm -f "$cand_rg_pt" "$cand_rg_onnx"
        rg_rejects=$((rg_rejects + 1))
        log "${C_RED}  ✗ [reject  RG] keeping ${prev_rg}; reject streak=${rg_rejects}; retry next iter${C_RESET}"
    fi

    # 5. Cleanup
    rm -rf "$data"

    log "  Iter $iter complete in $(( $(date +%s) - iter_start ))s  (rg=${gen_rg})"
    iter=$((iter + 1))

    # Roll the master log if the current part has exceeded the size cap.
    rotate_log_if_needed
done
