#!/bin/bash
# Rudimentary self-play loop with gated promotion. No CLI options — edit
# the constants below. Run from repo root after activating the riftbound
# conda env:
#   conda activate riftbound
#   chmod +x scripts/self_play_loop.sh
#   ./scripts/self_play_loop.sh
#
# Each iteration:
#   1. Generates self-play data with the current best models (T=1.0)
#   2. Trains both archetypes in parallel on separate GPUs (REINFORCE)
#      → writes to candidate.pt / candidate.onnx (NOT v(N+1) yet)
#   3. Benchmarks each candidate vs its current best (mirror match, T=0).
#      EVAL_GAMES per seat × 2 seats = 2×EVAL_GAMES total per archetype,
#      split candidate-as-P1 + candidate-as-P2 to control for seat bias.
#   4. Promotes candidates whose decisive win rate ≥ PROMOTE_PCT to
#      v(N+1). Rejected candidates are discarded; that archetype's gen
#      counter stays put and the next iter retries with fresh data.
#   5. Cleans up the iteration's .bin data.
#
# MF and Rengar gens advance independently — either can plateau without
# blocking the other.
#
# Outputs: models/<arch>/v<N>.{pt,onnx} per promoted iteration.
# All stdout/stderr → logs/self_play_<timestamp>.log + terminal.
# Loops forever — Ctrl-C to stop.

set -e

# Kill any background children if the script exits, is Ctrl-C'd, or is
# kill'd externally. Without this, the parallel python training jobs we
# launch with `&` get reparented to init when bash dies and keep running,
# competing with the next run for GPU/disk.
cleanup_children() {
    pkill -TERM -P $$ 2>/dev/null || true
    # Give them a moment to flush. Most processes exit on SIGTERM, but
    # python with CUDA can take a few seconds — fall through to SIGKILL.
    sleep 1
    pkill -KILL -P $$ 2>/dev/null || true
}
trap cleanup_children EXIT
trap 'cleanup_children; exit 130' INT TERM

GAMES=500            # self-play games per iter (training data)
EVAL_GAMES=100       # base per-seat eval games (×2 seats = 200 total / arch)
THREADS=8
EPOCHS=1
LR=1e-4
ENTROPY=0.01         # base entropy coefficient (knob-turned when plateauing)
BATCH=256
PROMOTE_PCT=55       # min decisive win rate (integer %) to promote

# Plateau-breaking schedule. After N consecutive rejected iterations the
# archetype is judged "stuck" and we crank TWO knobs:
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
# Read with `zcat logs/self_play_*.log.gz | less` (sorted by name = chronological).
mkdir -p logs
RUN_ID=$(date +%Y%m%d_%H%M%S)
LOG_DIR=logs
LOG_MAX_BYTES=$((50 * 1024 * 1024))   # 50 MB per part
MAX_GZ_FILES=100                       # cap on total compressed log files

log_part=1
LOG="$LOG_DIR/self_play_${RUN_ID}_part$(printf %03d "$log_part").log"
exec > >(tee -a "$LOG") 2>&1

# Rotate the master log if the active part has grown past LOG_MAX_BYTES.
# Re-binds stdout/stderr to a new tee on a fresh part file; the old tee
# process gets EOF when we re-exec and exits cleanly. Closed part is
# gzipped asynchronously; oldest .gz files beyond MAX_GZ_FILES are pruned.
rotate_log_if_needed() {
    local sz
    sz=$(stat -c%s "$LOG" 2>/dev/null || echo 0)
    if [ "$sz" -le "$LOG_MAX_BYTES" ]; then return; fi

    local old_log="$LOG"
    log_part=$((log_part + 1))
    LOG="$LOG_DIR/self_play_${RUN_ID}_part$(printf %03d "$log_part").log"
    exec > >(tee -a "$LOG") 2>&1
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [logrotate] closed ${old_log} (size=${sz}B), continuing in ${LOG}"
    (gzip -9 "$old_log" \
        && ls -t "$LOG_DIR"/self_play_*.log.gz 2>/dev/null \
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
log "  Self-play loop start"
log "  Log: $LOG (rotates every ${LOG_MAX_BYTES} B, keeps last ${MAX_GZ_FILES} .gz)"
log "  GAMES=$GAMES  EVAL_GAMES=$EVAL_GAMES (×2 seats)  THREADS=$THREADS"
log "  EPOCHS=$EPOCHS  LR=$LR  ENTROPY=$ENTROPY  BATCH=$BATCH"
log "  PROMOTE_PCT=$PROMOTE_PCT"
log "  Plateau bumps (entropy + eval games): 2 rejects → ×${ENTROPY_MULT_2}, 4 → ×${ENTROPY_MULT_3}, 6+ → ×${ENTROPY_MULT_4}"
log "════════════════════════════════════════════════════════════════"

# Adaptive entropy schedule. Args: consecutive_reject_count.
# Echoes the entropy coefficient to use for this iteration.
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
# Echoes the per-seat eval game count to use for this iteration.
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
    # tee splits the engine stream: → tmpf (for parsing) AND → stderr (live).
    # The trailing redirect runs tee's stdout into the script's stderr so it
    # doesn't get picked up by the caller's $().
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
# Per-archetype gen counters. Either can advance independently.
# On startup, detect the highest existing v00N for each archetype so a
# crash-and-restart picks up where we left off instead of clobbering work.
latest_gen() {
    local dir=$1
    local n
    n=$(ls "$dir"/v[0-9][0-9][0-9].pt 2>/dev/null \
        | sed -n 's|.*/v0*\([0-9]\+\)\.pt|\1|p' \
        | sort -n | tail -1)
    echo "${n:-1}"
}
gen_mf=$(latest_gen models/miss_fortune)
gen_rg=$(latest_gen models/rengar)
iter=1
# Per-archetype consecutive-rejection counters drive the entropy bump.
mf_rejects=0
rg_rejects=0
log "  Resuming with gen_mf=$gen_mf, gen_rg=$gen_rg (highest existing v00N on disk)"

while true; do
    prev_mf=$(printf "v%03d" "$gen_mf")
    prev_rg=$(printf "v%03d" "$gen_rg")
    next_mf=$(printf "v%03d" "$((gen_mf + 1))")
    next_rg=$(printf "v%03d" "$((gen_rg + 1))")
    data="training_data/self_play_iter${iter}"
    iter_start=$(date +%s)

    cand_mf_pt="models/miss_fortune/candidate.pt"
    cand_mf_onnx="models/miss_fortune/candidate.onnx"
    cand_rg_pt="models/rengar/candidate.pt"
    cand_rg_onnx="models/rengar/candidate.onnx"

    log ""
    log "═══════════════════════════════════════════════════════════"
    log "  Iter $iter — MF best: $prev_mf  Rengar best: $prev_rg"
    log "═══════════════════════════════════════════════════════════"
    log "  Disk free: $(df -h . | awk 'NR==2 {print $4}')"
    log "  Inputs:"
    log "    mf .onnx: $(ls -la models/miss_fortune/${prev_mf}.onnx 2>/dev/null | awk '{print $5, $6, $7, $8}')"
    log "    rg .onnx: $(ls -la models/rengar/${prev_rg}.onnx       2>/dev/null | awk '{print $5, $6, $7, $8}')"

    # Verify inputs exist before we burn time on a doomed iteration
    for f in models/miss_fortune/${prev_mf}.pt \
             models/miss_fortune/${prev_mf}.onnx \
             models/rengar/${prev_rg}.pt \
             models/rengar/${prev_rg}.onnx; do
        if [ ! -f "$f" ]; then
            log "ERROR: missing input model $f — aborting."
            exit 1
        fi
    done

    mkdir -p "$data"

    # 1. Self-play data generation
    log "  [step 1] generating $GAMES self-play games (T=1.0)..."
    step_start=$(date +%s)
    ./build/riftbound decks/miss_fortune_test.json decks/rengar_test.json \
        -r cards/registry.json \
        --agent1 "model:models/miss_fortune/${prev_mf}.onnx" \
        --agent2 "model:models/rengar/${prev_rg}.onnx" \
        --temp1 1.0 --temp2 1.0 \
        --games "$GAMES" --threads "$THREADS" \
        -o "$data/game.bin"
    log "  [step 1] done in $(( $(date +%s) - step_start ))s, size: $(du -sh "$data" 2>/dev/null | awk '{print $1}')"

    # 2. Train both archetypes in parallel to CANDIDATE paths (NOT v(N+1) yet)
    #    Entropy coefficient adapts per-archetype based on consecutive
    #    rejection count — stuck models get pushed toward more exploration.
    mf_ent=$(adaptive_entropy "$mf_rejects")
    rg_ent=$(adaptive_entropy "$rg_rejects")
    log "  [step 2] training MF (GPU 0, entropy=$mf_ent, rejects=$mf_rejects) and Rengar (GPU 1, entropy=$rg_ent, rejects=$rg_rejects)..."
    step_start=$(date +%s)
    python scripts/train_agent.py train-rl "$data/" \
        --resume "models/miss_fortune/${prev_mf}.pt" \
        --output "$cand_mf_pt" \
        --epochs "$EPOCHS" --lr "$LR" --entropy-coef "$mf_ent" \
        --batch-size "$BATCH" --gpu 0 --dataloader-workers 3 &
    mf_pid=$!

    python scripts/train_agent.py train-rl "$data/" \
        --resume "models/rengar/${prev_rg}.pt" \
        --output "$cand_rg_pt" \
        --epochs "$EPOCHS" --lr "$LR" --entropy-coef "$rg_ent" \
        --batch-size "$BATCH" --gpu 1 --dataloader-workers 3 &
    rg_pid=$!

    set +e
    wait $mf_pid; mf_rc=$?
    wait $rg_pid; rg_rc=$?
    set -e
    log "  [step 2] done in $(( $(date +%s) - step_start ))s — mf exit=$mf_rc, rg exit=$rg_rc"

    if [ $mf_rc -ne 0 ] || [ $rg_rc -ne 0 ]; then
        log "ERROR: training failed — leaving $data on disk for inspection."
        exit 1
    fi

    # 3. Evaluate candidates vs current best (mirror, T=0 argmax, 2 seats).
    #    Per-seat game count scales with the reject streak so stuck archetypes
    #    get a tighter statistical gate near the 55% threshold.
    mf_ev=$(adaptive_eval_games "$mf_rejects")
    rg_ev=$(adaptive_eval_games "$rg_rejects")
    log "  [step 3] evaluating — MF: ${mf_ev}/seat (rejects=$mf_rejects), Rengar: ${rg_ev}/seat (rejects=$rg_rejects)..."
    step_start=$(date +%s)
    mf_promote=$(eval_candidate decks/miss_fortune_test.json \
        "$cand_mf_onnx" "models/miss_fortune/${prev_mf}.onnx" "MF" "$mf_ev")
    rg_promote=$(eval_candidate decks/rengar_test.json \
        "$cand_rg_onnx" "models/rengar/${prev_rg}.onnx" "Rengar" "$rg_ev")
    log "  [step 3] done in $(( $(date +%s) - step_start ))s"

    # 4. Promote winners, discard losers. Reset reject counter on promote;
    #    increment on reject so the next iter cranks entropy.
    #    Promote = bold green, reject = bold red, easy to grep / eyeball.
    if [ "$mf_promote" -eq 1 ]; then
        mv "$cand_mf_pt"   "models/miss_fortune/${next_mf}.pt"
        mv "$cand_mf_onnx" "models/miss_fortune/${next_mf}.onnx"
        gen_mf=$((gen_mf + 1))
        mf_rejects=0
        log "${C_GREEN}  ✓ [promote MF] ${prev_mf} → ${next_mf}${C_RESET}"
    else
        rm -f "$cand_mf_pt" "$cand_mf_onnx"
        mf_rejects=$((mf_rejects + 1))
        log "${C_RED}  ✗ [reject  MF] keeping ${prev_mf}; reject streak=${mf_rejects}; retry next iter${C_RESET}"
    fi

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

    log "  Iter $iter complete in $(( $(date +%s) - iter_start ))s  (mf=${gen_mf}, rg=${gen_rg})"
    iter=$((iter + 1))

    # Roll the master log if the current part has exceeded the size cap.
    # Done at iter boundary so logs split on natural sections, not mid-line.
    rotate_log_if_needed
done
