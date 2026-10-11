# Fix run status

Maintained by the orchestrator. The authoritative copy is the one on the
`fix/integration` branch (`.worktrees/integration/plans/STATUS.md`); the copy
in the main checkout lags until integration is merged to master.

States: `pending` → `running` → `returned` → `merged` (or `blocked`).

## Resume notes (read this first after a context reset)

Written 2026-10-11 so a fresh orchestrator context can continue with nothing
but this file, `README.md` and `AGENT_BRIEF.md`. "Run decisions" below
override the README where they differ.

**Agents and sessions (2026-10-11 00:52):** the orchestrator is Claude session
`riftbound-engine-d6` (pid 1238504) and all stage 3 agents are its subagents.
The earlier host process (pid 2329) hung at about 00:42 with every agent
silent, and was killed; the six streams were resumed from their transcripts
by `SendMessage` to their ids (in `.worktrees/agents.txt`). If the session
dies again, do the same from the new one: worktrees keep uncommitted work,
in-flight build results are void. A stream is finished when
`.worktrees/reports/<stream>.md` exists and its worktree is clean.

**Where we are:** stages 0, 1, 2 done and on `master` (`6a3e52b`). Stage 3
running. 378 of 514 findings ticked. All state is committed on
`fix/integration` (`.worktrees/integration`).

**Stage 3 streams in flight** (spawned from `0ca1b19`; each has a worktree
`.worktrees/<stream>` and branch `fix/<stream>`; each was told to ALSO write
its final report to `.worktrees/reports/<stream>.md`, so a report survives a
reset — read that file if no notification text is available). Their agent
ids are in `.worktrees/agents.txt` (untracked), not here:

| Stream | Owns | Work |
|---|---|---|
| sweep-costs | cost and payment in `game_engine` (canAfford, payCardCost, pay*Cost, payActivationResources, payment half of executePlayCard / executePlaySpell / payAndQueueAbility, parseRepeatCost, executeHideCard, trash-replay payment), cost virtuals on `Card`, RunePool | 1 equip-cost descriptor (7.7, Weaponmaster cost, Piltovan Forge 948, re-equip); 2 alternative cost for spells (146, 207) and pre-payment additional costs (2, 150, 740, 470, 462, 540, 231, 732, 847, 208, 367, 481); 3 restricted resource pools (294, 506, 786, 312); 4 card-declared Repeat cost (8.9: 579, 743) and Power on granted Repeat (401, 772); 5 play from trash (587, 809), trash-replay gates and discounts, Death Mark 931 Flow; 6 hide-cost hook (259); 7 2.11 root cause; 8 payAnyPower dup, payEnergy base runes, "minimum of [N]", Risen Altar 950, Aphelios 372, Undying Loyalty 730 |
| sweep-verify-cards | card files (not the shared card headers), `tests/cards/test_review_fix_verify_cards.cpp`, ticks in 03 / 07 / 08 / batch files | spawned from `6ad4b6c`; card-only verification, see the Streams row |
| sweep-flow | rest of `game_engine` (generators, turn flow, showdown, combat, scoring, moves, setupBattlefields, resolvePermanent / resolveSpell, runFinalizePass, recalculateAuras, killUnit), `Card::hasLegalTargets`, `intent.h` | 1 5.4 remainder (text matches); 2 target legality with CardDB (45, 939); 3 stable target roles in resolveSpell; 4 hidden plays and showdown reactions (645, CR 811, 813.1.b); 5 mainPhaseLoop stale priority and stranded triggers; 6 finalize / resolve caps; 7 moves (712, 725, 679); 8 CR 468.3.d.1 and effective Victory Score (79, 47, 678, 878, 556); 9 delete requiresLegion, Svellsongur 382, Prodigal Explorer 514, Wuju Master 784, Fiora 232 aura order, Crowd Favorite 664, Defy 45, Yone 544, engine call to recompute legal actions after a god-mode edit |
| sweep-triggers | `trigger_manager`, `chain_manager`, `effect_executor`, `effect_types.h`, `events.h`, DelayedAbility / ChainItem | 1 recall primitive (290, 272, 337, 550, 264); 2 extra score triggers (649, 591); 3 tokens announce a play, token-created trigger, Zilean, Weaponmaster for tokens (8.4, 512, 620, 648, 908); 4 spend-a-buff primitive (423); 5 batch deaths / last-known info (517, 718, 777, 72, 23); 6 trigger plumbing (onUnitStunned, WhenOpponentPlaysAUnit, is_rules_text_inactive, Vision, Might threshold 879); 7 delayed abilities (dup flag, expiry, iterator); 8 chain (ignoresDeflect, PlayReaction branch, replay-while-resolving 582 / 747, resolve cap, Astral Heron 831); 9 executor (recycleCards on board, playIgnoringCost ready, recycled_main, attach guard 8.14, draws by hand 443 / 626, gainXp callers, temp Deflect field, Scuttle Crab 615); 10 LogEvent visibility tag |
| cleanup | `src/io`, `main.cpp`, `src/openspiel`, agents, ml, rules, core except game_state / events / intent, card_registry, runners, step_driver, scripts, docs, README, CMake, CLAUDE.md, tests | 5.13, 5.14; generate_replays.py; unsafe ccache launcher in CLAUDE.md; stale CLAUDE.md; warnings in its files; play_server into riftbound_core; fixture fixes (addToHand, addUnit, accepting agent); spectator Ctrl-C; server side of god-mode recompute |
| perf-search | `src/openspiel`, agents, ml, step_driver, step API in `game_engine` | 06: 6.4, 6.7, 6.8, 6.9, each measured before and after |

**What comes next, in order:**
1. As streams return: merge procedure below. sweep-costs and sweep-flow share
   `game_engine.cpp`; expect conflicts between them and send the later one
   back to rebase if a conflict is more than mechanical.
2. When sweep-costs, sweep-flow, sweep-triggers and cleanup are merged: spawn
   **sweep-verify** (alone among sweep parts; perf-search may still run). Its
   job: for every open box in 03, 07, 08, and the batch files, verify each
   named card in the code and tick; close every Carry-over row still open
   (including the two `orchestrator` rows: RETROFIT of not-a-defect closures,
   and the REOPENED residuals); unify duplicates (`selectAnyNumber` /
   `pickSeveral`, local location pickers in 226 / 349); 8.8 keyword data
   (`Keyword::Reaction` on ~25 permanents and on Ambush units, `Temporary` /
   `Assault` on spell defs); `card.h` and card-file warnings; review every
   `shared:` engine change listed in the table; audit stage 2 tests that pass
   no agent to a "may"; update CLAUDE.md for what the sweep changed; tick 8.16.
   It will likely need splitting or continuing — it is large.
3. Then **perf-engine** (06: 6.1, 6.2, 6.3, 6.5, 6.6, 6.10, 6.11), alone.
4. Stage 3 gate → fast-forward master → email.
5. Stage 4 strong-ids (5.5), alone. Stage 5 checkpoint (1.9, 1.12, 7.33, plus
   anything marked "needs stage 5": Defender of Tomorrow 936; new state fields
   added in stages 2 and 3 that should enter the feature layout). Final gate.

**Merge procedure as practised** (README section 5 plus what this run learned):
- `git merge --no-ff fix/<stream>` in `.worktrees/integration`. Mechanical
  both-sides conflicts are resolved here: plan files with
  `python3 .worktrees/resolve_plans.py <files>`; appended helper blocks and
  enum values by keeping both — then READ the joined region and compile before
  committing (a keep-both join dropped a closing brace once). Anything needing
  judgement about a stream's code goes back to the stream.
- Verify: `flock .worktrees/.build.lock env GATE_JOBS=3 bash .worktrees/gate.sh <label>`
  (lane A; see Run decisions for stage gates)
  (builds four targets, runs the suite, openspiel tests, clone-equiv, 200-game
  smoke, MCTS batch, fault injection; aborts on a failed build; summary in
  `.worktrees/logs/gate-<label>/summary.txt`). For a stage gate use
  `.worktrees/fixcache.sh <label>` instead: reconfigures, cleans, then gates.
- Check: ticked-box count against the report, files changed against ownership
  (`git diff --name-only HEAD~1 HEAD`), no card names or `ability_text` in the
  engine diff, then record the report's "Needs another stream" and "New
  defects" as Carry-over rows and mark the rows it closed `done`.
- Then `git worktree remove ../<stream> --force` and `git branch -d`.
- `.worktrees/progress.sh` is the user's `watch` view. Agents sometimes leave
  background wait loops that keep them showing as active after they finish:
  `pgrep -af 'until |sleep'` and kill the stray shell.

**Open questions for the user (not blocking):** Karma, Channeler 235 tag changed
Vi → Karma against the gallery data, printed card unchecked; smoke win split
drifted 128 / 72 → 138 / 62 and has not been examined.

## Run

| Field | Value |
|---|---|
| Current stage | 3 |
| Integration branch | `fix/integration` (`.worktrees/integration`) |
| Baseline commit (master at stage 0) | `c7143f1` |
| Baseline tests | 1511 run: 1508 passed, 2 failed, 1 skipped (1 disabled). Failing at baseline: `CardTestFixture.BaronNashor_IntegrationFullPlayPathCreatesPit`, `ChainTestFixture.FullGameRunsWithSpellsInDecks` (both: deleted deck file, item 7.40) |
| Last master sync into integration | `c7143f1` (branch point) |
| Last integration → master merge | `6a3e52b` (stage 2 gate, fast-forward) |
| Open checkboxes at start | 514 |

## Run decisions

Changes to the README procedure made by the user during the run. A fresh
iteration must apply these.

- **Batched merges (user, 2026-10-10):** merge every stream that has returned,
  then build and run the full suite once for the batch, instead of once per
  merge. If the batch is red, `git reset --hard` to the last verified commit
  and re-merge the streams one at a time to find the one at fault. "Never
  merge red" still holds for the batch as a whole; per-stream review (ticked
  boxes, ownership, spot-checks) is unchanged.
- **Machine load (user, 2026-10-10):** the trainer repo's `v6_baselines.sh`
  runs were stopped to free cores for this run; they are to be re-run only
  after the final merge to `master`.

- **Email alerts (user, 2026-10-10):** the user is away for stretches. Email
  them (Gmail connector, to the address in the session context, thread
  `1a12858947d608c8`, subject prefix `[riftbound fix run]`) only when something
  needs attention: a section 8 blocker, an agent that looks stalled (for
  example on a permission prompt), or a gate result (passed and merged to
  master, or failed). No routine progress mail.
- **Agent shell rule (2026-10-10):** every stream brief must say: no `rm`, no
  `sh -c` / `bash -c` / `eval` wrappers, no commands assembled from variables;
  multi-step locked work goes in a script file run as `flock <lock> bash <file>`.
  Those shapes raise a permission prompt that stalls the agent.

- **Build jobs (user, 2026-10-10):** builds use `-j6` (briefly `-j3`, then
  restored by the user), and CMake must enforce the cap itself: the integrity
  stream adds a `RIFTBOUND_BUILD_JOBS` cache variable (default 6) implemented
  with Ninja job pools, so a larger `-j` cannot exceed it. Batch and smoke runs
  use `--threads 6`.
- **WSL cores: decided, leave as is (user, 2026-10-11).** The 8-thread limit
  stays: the user has hit out-of-memory and CPU thrashing on this machine and
  wants load kept limited. Do not raise `-j`, the lane count or the thread
  count, and do not propose a WSL restart. Earlier note, for the record:
- **WSL cores (closed):** the host is a 12-core / 24-thread CPU
  but WSL sees 8 threads (likely `processors=8` in `.wslconfig`). Raising it
  needs `wsl --shutdown`, which ends the session; the stage 1 gate is the clean
  point. If the user does it, raise `RIFTBOUND_BUILD_JOBS` and `-j` to match.

- **ccache (2026-10-10):** the cross-worktree launcher from integrity
  (`CCACHE_BASEDIR` + `CCACHE_NOHASHDIR`) is NOT safe while worktrees edit
  headers in parallel: cards-c0 had a `.gch` from cards-c2 restored into its
  build. Every worktree, integration included, now uses a private cache
  (`CCACHE_NAMESPACE=<name>`, plain sloppiness) and does a clean build before
  its final suite run. `CLAUDE.md` still documents the shared launcher — fix or
  caveat it in the sweep.

- **Two build lanes (user, 2026-10-11):** five streams queueing on one lock
  spent most of their time waiting, and the lock is held through single-core
  test runs and links. Builds now run in two lanes at `-j3` each. Lane A locks
  `.worktrees/.build.lock` (sweep-costs, cleanup); lane B locks
  `.worktrees/.build-b.lock` (sweep-flow, sweep-triggers, perf-search). Every
  later stream is assigned a lane in its spawn prompt. Merge gates while
  streams are running take lane A only at `-j3`:
  `flock .worktrees/.build.lock env GATE_JOBS=3 bash .worktrees/gate.sh <label>`
  (or `fixcache.sh` for a clean rebuild). Holding A while queueing for B left
  lane A idle behind a long lane B queue. A STAGE gate, with no stream
  running, takes both locks and `-j6`. This overrides README "one build at a
  time".

- **After the final merge: build the v6 ML tooling (user, 2026-10-11).** Once
  the final gate has passed, `master` holds the final commit and the section 7
  report is written, the orchestrator goes on to build out the v6 tooling
  described in the trainer repo's `plans/README.md`, without waiting: the rest
  of the knowledge tracker, the action encoding and its collision test, the
  observation extractor and leak test, the `prompt` field on decisions, the v6
  model, the data harness, the `policy:` agent, and the trainer housekeeping.
  Engine edits for it are made in worktrees with tests, as in this run.
  Limits set by the user: **no training of real checkpoints and no baseline
  runs** (no self-play, no data generation at scale, no rented compute). Tests
  that run a few optimiser steps on synthetic data are at the orchestrator's
  discretion. The user's decisions for the design: the observation **must**
  encode the action history (what each player did, in order, masked per
  observer, with labels) and both players' public zones including trash and
  banishment — the model cannot play without them. The eventual acceptance
  test, done later with the user, is basic checkpoints that beat random or
  MCTS. libtorch: PyTorch 2.5.1 (CUDA 12.1) is already installed in the conda
  env `riftbound` (`~/miniconda3/envs/riftbound/lib/python3.12/site-packages/torch`,
  with `share/cmake/Torch/TorchConfig.cmake`). The engine's `CMakeLists.txt`
  already has the opt-in wiring: `-DRIFTBOUND_BUILD_LIBTORCH=ON`, which uses
  `LIBTORCH_DIR` if set and otherwise downloads libtorch 2.5.1
  (`RIFTBOUND_LIBTORCH_VARIANT` = `cpu` ~200 MB, or `cu121` ~2.5 GB). No
  downloaded copy is on disk now. Use that switch; point `LIBTORCH_DIR` at the
  conda copy first, download only if that does not link. Every design call made on the user's behalf is listed for review.
- **v6 tooling is C++ only (user, 2026-10-11):** no Python; the engine stays
  free of libtorch and is compiled into the trainer (`add_subdirectory`), one
  process, plain function calls; keep the code base flat. First training
  target, when the user asks for it: one model, Yi vs Fiora, then the same
  model continues on Fiora, then a third deck — continued training of one
  checkpoint, no weight merging, no distillation (trainer `plans/README.md`).
- **Trainer scripts and vast.ai (user, 2026-10-11):** when tooling work starts,
  review the trainer's `scripts/` and drop what v6 does not use (expected: all
  of the old ones, old vast.ai scripts included). Build v6 vast.ai tooling in
  their place — one-command setup and start, resume on interruptible
  instances, rsync both ways — proven end to end locally first, because
  vast.ai time is expensive and the user wants to go straight into training
  with as little friction as possible. Details in trainer `plans/README.md`,
  "Task list: vast.ai workflow" (nine tasks: script review, `run`, local
  proof, `find`, `up`, `sync`, `down`, self-shutdown, first remote shakedown
  with the user watching). The user's vast.ai account needs funding first.
- **Stage 5 and v6 (user, 2026-10-11):** the trainer plan does not run until
  the engine fixes are merged, and every finding is required first. If stage 5
  is reached unattended, follow README stage 5: check whether v6 has replaced
  the old encoding and close 1.9, 1.12 and 7.33 against v6 if so; new state
  fields go into the v6 token layout.

- **Findings found during the run are plan items (user, 2026-10-11):** every
  new defect a stream reports is written as a checkbox in
  `plans/09-found-during-fix-run.md` when its report is read, not only as a
  carry-over row, so the final gate's open-box count includes it. Stream
  reports are copied to `plans/reports/` and committed. The stream that fixes a
  9.x item ticks it there.
- **Sweep split (orchestrator, 2026-10-11):** stage 2 left ~90 carry-over rows,
  67 open boxes in 03/07/08 and 43 BLOCKED card items for the sweep — too much
  for one stream. It runs as sweep-costs, sweep-flow and sweep-triggers in
  parallel (split by file/function ownership), then sweep-verify alone. Each
  engine stream also unblocks the BLOCKED card items that depend on its piece.
  perf-engine still waits for all sweep parts and cleanup.

- **Nothing optional (user, 2026-10-11):** the goal is a clean engine before
  the v6 training tooling. No carry-over row and no plan item may be closed as
  "optional", "low", "note" or "future work". Every row ends as fixed with a
  test, or NOT A DEFECT with evidence a reviewer can check, or — only where the
  fix changes the feature layout or action vocabulary — moved to stage 5 by
  name. Residuals recorded as "partial by design", "documented approximation"
  or "future work" in stage 1 and 2 (Defy 45, Defender of Tomorrow 936, Solari
  Shrine 72, Unlicensed Armory 23, Bard 540, Scuttle Crab 615, Astral Heron
  831, the fair-play log panel, Yone 544, Fiora 232 aura lag) are reopened and
  belong to the sweep.

- **"Not a defect" still changes the code (user, 2026-10-11):** a finding that
  turns out not to be a defect was still raised because the code misled a
  careful reader. Closing one requires, as well as the evidence: (a) a test
  that pins the behaviour the finding questioned, named so it can be found
  from the plan line, and (b) a fix for whatever misled the reviewer — a
  comment at the code site citing the card text or CR number, a rename, or
  removal of the dead or contradictory construct. The plan line reads
  `NOT A DEFECT: <evidence>; pinned by <Test.Name>; clarified in <file>`.
  The only exception is a finding about code that no longer exists.

## Streams

| Stage | Stream | State | Branch head | Tests at merge | Notes |
|---|---|---|---|---|---|
| 1 | search | merged | `44a7541` | batch: 1707 run, 1707 passed, 0 failed | all 16 items closed; both baseline failures fixed; clone-equiv 10/10; new binary `riftbound_openspiel_tests` |
| 1 | helpers | merged | `2e0f181` | batch: 1707 run, 1707 passed, 0 failed | 4 items closed; Thrill of the Hunt 745 and Zenith Blade 258 fixed with red-then-green tests; 3.1, 3.24, 7.30 open for card streams |
| 1 | executor | merged | `ac49a38` (merge `e1eb9b8`) | 1560 run, 1557 passed, 2 baseline failures | all 8 items closed; 2 existing tests corrected (CR 427.1.b.1; temp Deflect) |
| 1 | chain-triggers | merged | `6d4dd76` (merge `c9ad6b4`) | 1583 run, 1580 passed, 2 baseline failures | 12 items closed; 7.1 decision recorded (cards use `pickTarget` + `kFinalizeTrigger`); 8.9 has one bullet left, in carry-over |
| 1 | engine-auras-costs | merged | `3ba5256` (merge `a8af2ef`) | 1631 run, 1628 passed, 2 baseline failures | 10 items closed; 5.4 and 8.4 partly done, remainder in carry-over; edited 25 card files (listed in carry-over) |
| 1 | engine-play-combat | merged | `3df9c9b` (merge `ec13e47`) | 1537 run, 1534 passed, 2 baseline failures | 16 of 17 items closed; 8.7 left open, moved to carry-over (card work) |
| 1 | io-cli | merged | `f3e2208` | gate: 1726 run, 1726 passed | all 21 items of 04 ticked; follow-ups done: decision-id + seat, fair-play log withholding (4.5), batch exits 1 on engine exception, handshake race fixed |
| 1 | integrity | merged | `fc1f7cb` (merge `b0d329b`) | gate: 1726 run, 1726 passed | 8 items closed; build tasks done: shared-ccache launcher + `-fmacro-prefix-map`, unity tests default ON (204.8 s → 110.1 s full rebuild), `RIFTBOUND_BUILD_JOBS` cap verified at `-j16` |
| 2 | cards-c0 | merged | `0c82974` (merge `7ad4269`) | clean build: 1783/1783, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 31 boxes ticked; 6 batch-00 items BLOCKED on engine work (in carry-over); 3 `shared:` commits incl. engine changes |
| 2 | cards-c1 | merged | `7e89826` | batch gate at `e36ffa6`: 2040/2040, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 35 of 40 batch-01 boxes + 5 batch-08; 6 BLOCKED on engine work; corrected tests in 12 existing files; did not build `riftbound` or run the smoke |
| 2 | cards-c2 | merged | `2fe1205` | batch gate at `e36ffa6`: 2040/2040, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 41 of 42 batch-02 boxes + 3 batch-08; Wallop 146 / Call to Glory 207 BLOCKED (spell alternative-cost path); 3 `shared:` commits with engine state and trigger additions |
| 2 | cards-c3 | merged | `c2def86` | batch gate at `e36ffa6`: 2040/2040, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 35 of 39 batch-03 boxes + 9 in batch-08/09; 4 left open (645 facedown, 579, 743 Repeat cost, 931 Flow); 6 `shared:` commits; corrected 7 existing tests |
| 2 | cards-c4 | merged | `fd00d2c` | batch gate at `e36ffa6`: 2040/2040, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 21 of 25 batch-04 boxes + 2 batch-09; 4 BLOCKED on engine work; 42 of 50 new tests shown red on the unfixed cards; merge conflict with c2 in two engine lines resolved by OR-ing both conditions |
| 2 | cards-c5 | merged | `870307e` (merge `b95b4e4`) | gate-c5: 2100/2100, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 33 of 39 batch-05 boxes + 6 batch-09 + 4 batch-10; 7 BLOCKED on engine work; 5 `shared:` commits |
| 2 | cards-c6 | merged | `d06b58d` (merge `a07ce94`) | gate-c6: 2161/2161, openspiel 17/17, clone-equiv 10/10, smoke rc=0 | 39 of 46 batch-06 boxes + 1 batch-10; 7 BLOCKED on engine work; one `shared:` commit |
| 2 | cards-c7 | merged | `8ff64db` (merge `76ab21f`) | stage 2 gate: 2203/2203 | 10 of 18 batch-07 boxes + 6 batch-10; 8 left BLOCKED on engine work; one `shared:` commit; `kKnownOffenders` now empty (all 26 target-less-trigger cards converted) |
| 3 | sweep-costs | running | | | sweep part 1 of 4: costs, payment and play paths in `game_engine` |
| 3 | sweep-flow | merged | `1dad1be` (merge `1559f29`) | gate-sweep-flow: 2248/2248, openspiel 17/17, clone-equiv 10/10, smoke 137 / 63 / 0 rc=0, MCTS batch rc=0, fault injection rc=1 | 9 boxes ticked (2.10 and 8 card items); 5.4 two of three text matches removed, 8.7 virtuals deleted; Crowd Favorite 664 closed NOT A DEFECT with pinning test; 42 new tests. Svellsongur 382 not done → sweep-triggers. CLAUDE.md conflict resolved by keeping both sides |
| 3 | sweep-triggers | merged; follow-up running | `1e9eb2a` (merge `202b779`) | gate-sweep-triggers at `8a8989e`: 2300/2300, openspiel 17/17, clone-equiv 10/10, smoke 141 / 59 / 0 rc=0, MCTS batch rc=0, fault injection rc=1 | 18 boxes ticked (8.4, Svellsongur 382 and 14 more card items, 9.A17, 9.C1); recall primitive, token plays, keyword abilities, death batches, `LogAudience`; 51 new tests, 38 not shown red. Scope was reconstructed after the resume; gaps recorded in plans/09 section D and sent back as a follow-up in the same worktree |
| 3 | sweep-verify-cards | running | | | early half of sweep-verify (card files and a new test file only; lane A): verifies and ticks the card-only boxes in 03 / 07 / 08, retrofits card-site not-a-defect closures, adds missing tests, audits "may" tests. Must rebase after the engine sweeps merge. Leaves engine-dependent boxes to sweep-verify |
| 3 | sweep-verify | pending | | | sweep part 4 of 4: starts after the three above merge; verifies and ticks every open box in 03, 07, 08, the batch files and the rest of carry-over |
| 3 | perf-engine | pending | | | |
| 3 | perf-search | merged | `b549cc4` (merge `bdb8664`) | gate-perf-search at `831dd56`: 2309/2309, openspiel 20/20, clone-equiv 10/10, smoke 141 / 59 / 0 rc=0, MCTS batch rc=0, fault injection rc=1 | 4 boxes ticked (6.4, 6.7, 6.8, 6.9) with before / after numbers; MCTS-50 batch 36.7 s → 11.3 s; 6.7 carry-over row fixed; 3 new items in plans/09 section E |
| 3 | cleanup | merged; follow-up running | `e9f8b79` (merge `a8723a4`) | gate-cleanup2 at `f3670f3`, clean build: 2206/2206, openspiel 17/17, clone-equiv 10/10, smoke 138 / 62 / 0 rc=0, MCTS batch rc=0, fault injection rc=1 | 0 boxes ticked: 5.13 / 5.14 remainder is in sweep-owned files (carry-over). Fixed: trainer-compat CMake paths, ctest registration, PCH-safe launcher, spectator Ctrl-C, god-mode recompute, fixture stats and accepting agent, trigger helpers. Follow-up in the same worktree: warnings, `testHook_executeIntent` cleanup, play-server confirmation |
| 4 | strong-ids | pending | | | |
| 5 | checkpoint | pending | | | |

## Gates

| Gate | Passed | Evidence |
|---|---|---|
| Stage 0 baseline | yes | `c7143f1`; tests 1508/1511 (2 baseline failures above); smoke: 200 games fiora vs yi_rebuild, `--seed 20261010 --threads 6`, exit 0, P1 128 / P2 72 / 0 draws, no exception; `riftbound_clone_equiv_test` FAILS at baseline (hard-coded `decks/leblanc_test.txt`, item 7.40) and must pass at the stage 1 gate |
| Stage 1 | yes | `b0d329b` (all 8 streams): suite 1726/1726, `riftbound_openspiel_tests` 17/17, clone-equiv 10/10, smoke 200 games fiora vs yi_rebuild seed 20261010 rc=0 P1 130 / P2 70 / 0 draws, MCTS batch rc=0, fault injection rc=1 with "game 1 [seed=7]: injected engine fault" (1.7 confirmed: failure, not a draw). Logs `.worktrees/logs/gate-stage1/`. Checkbox criterion: 87+8 ticked; stage 1 items left `[ ]` because the rest of each is card or cross-stream work, all in carry-over — 3.1, 3.24, 7.30, 7.1 (card streams, sweep ticks), 8.7 (cards), 5.4, 8.4, 8.9 (sweep) |
| Stage 2 | yes | `76ab21f` (all 8 card streams), clean rebuild on a private cache: suite 2203/2203, `riftbound_openspiel_tests` 17/17, clone-equiv 10/10, smoke 200 games rc=0 P1 138 / P2 62 / 0 draws, MCTS batch rc=0, fault injection rc=1. Logs `.worktrees/logs/gate-stage2/`. Checkbox criterion: card streams ticked every item they could finish; ~40 card items stay `[ ]` with BLOCKED sub-bullets needing engine work, all in carry-over for the stage 3 sweep |
| Stage 3 | | |
| Stage 4 | | |
| Stage 5 | | |
| Final | | |

## Carry-over

Items a stream reported under "Needs another stream" or "New defects found",
and who picked them up. Every row must end `done` before the final gate.

| From | Item | Assigned to | State |
|---|---|---|---|
| trainer survey | NEW (stage 1 regression): `src/openspiel/CMakeLists.txt` lines 110, 125, 127 use `CMAKE_SOURCE_DIR` for `riftbound_openspiel_tests`; the trainer builds the engine with `add_subdirectory`, so it cannot configure against engine master. Fix every engine CMake path that breaks as a subdirectory and verify with a throwaway parent project | cleanup (relayed 2026-10-11 via riftbound-engine-5e) | done (cleanup `5e6bbbd`: `PROJECT_SOURCE_DIR`; verified by configuring a throwaway parent project; also found `riftbound_openspiel_tests` was never registered with ctest and fixed it) |
| trainer survey | notes for the final report: trainer `terminationKind` has no case for `"engine_error"`; trainer compiles `scripted_agent.cpp` itself although it is now in `riftbound_core`; trainer `test_knowledge_gate` allow-lists the engine files that may edit `main_deck` (a new direct edit in a card file fails it); full survey in `.worktrees/reports/trainer-survey.md` | final report | open |
| cleanup | 5.13 / 5.14 remainder, in files the sweep streams own: `game_engine.{h,cpp}` — `swapBackBattlefield`, `generateCombatDamageActions`, `advanceCleanup`, `activePlayer` have no callers, `executeEndTurn` is empty, `had_temp_might` unused, `contested_by` / `combat_staged` left set after combat resolution, header still says "stubbed" and "worker thread" and "cleanup has no agent-query sites"; `events.h` — `CleanupNeededEvent` has no emitter; `game_state.h` — `CostPaymentCursor` labelled POD with a `std::vector`, wrong `current_play_source` comment; `cards_init.cpp` line 1 says "Auto-generated" | sweep-verify (ticks 5.13, 5.14) | open |
| cleanup | warnings: 253 `warning:` lines in the clean gate build at `f3670f3` (many repeats of the `card.h` unused parameters). cleanup follow-up takes io / openspiel / agents / ml / core / tests; sweep-verify-cards takes card files; `card.h` and engine files go to sweep-verify | cleanup, sweep-verify-cards, sweep-verify | open |
| cleanup | cleanup's resumed agent had lost its original brief (transcript resume): every resumed stream's report must be checked against the STATUS in-flight table, not against what the report says it was asked | orchestrator (every stage 3 merge) | open |
| sweep-flow | partly closed by sweep-flow, remainder with other streams: cards-c7 BLOCKED row (712 move flag, 725 done; 879, 732, 718 / 777, 740, 908 / 712 token plays, 847 open); cards-c6 BLOCKED row (679 done; 649, 591, 587, 620, 648, 550 open); cards-c1 BLOCKED row (514 done; 512, 809, 294 / 506 / 786, 259 open); cards-c0 BLOCKED row (382 → sweep-triggers; 290, 272, equip descriptor, 950 open); Yone 544 previous controller done | sweep-costs, sweep-triggers; sweep-verify reconciles each BLOCKED row card by card | open |
| sweep-flow | Svellsongur 382 not done: needs the trigger manager to repeat the bearer's triggers and a per-instance aura hook (design under the item in batch-00) | sweep-triggers (messaged 2026-10-11) | open |
| sweep-flow | `shared:`-style changes to review: `GameState::noteChosen` at every choice site; `cant_move_this_turn` also stops the controller's effect moves; multi-unit Standard Move asks follow-up `MakeChoice` prompts; resume / finalize bounds now THROW (`std::logic_error`) — a card that loops is an engine exception and fails a batch; 4 existing tests corrected (`SafetyCap_...`, `ProdigalExplorer_IsAReactionWithAGate`, `Yone_ConqueringAnEnemyBattlefieldDoesNothing`, `LegionParsesAsConditionGate`); Blitzcrank 67, Mischievous Marai 562, Shadow 752 lost a play-anywhere permission the text match had given them — confirm against card text | sweep-verify (review) | open |
| sweep-flow | new state fields not in the feature layout: `cant_move_this_turn`, `enemy_choices_this_turn`, `grant_units_lone_enemy_bf`, `multi_move_tax`, `conquered_from` | checkpoint / v6 token layout | open |
| sweep-flow | trainer note: a Standard Move with other eligible units asks extra `MakeChoice` prompts (first option "Move no more units") | final report | open |
| sweep-verify-cards | first report (`.worktrees/reports/sweep-verify-cards.md`, not yet merged): 48 boxes ticked in 03 / 07 / 08 + 2 batch boxes; 19 left open, each with a `verify-cards:` sub-bullet naming what remains (3.8, 3.13, 3.15, 3.21, 7.6, 7.7, 7.23, 7.26, 7.35, S2, S3, S4, S5 wait on in-flight cards; 8.4, 8.7, 8.8, 8.9, 8.16 on the engine streams; 7.33 is stage 5) | sweep-verify (after the engine sweeps) | open |
| sweep-verify-cards | NEW card defects it found (about 30, listed in section 5 of its report): sent back to the same stream as a second tranche, each to end FIXED with a red-first test or NOT A DEFECT with pin and clarification | sweep-verify-cards (second report: `sweep-verify-cards-2.md`) | open |
| sweep-verify-cards | NEW, not card files: `Card::costDueNow` is true on every re-entry when no finalize pass ran (an item played by an effect pays a non-idempotent cost again); `pickSeveral` "any number" stops at 12 picks (Albus Ferros); `weaponmaster_base.h` detaches Equipment by editing vectors instead of `unattachGear` and asks its "may" at resolution; Ornn 381 / Ivern 613 "draw it" pushes to hand with no draw event (same class as 443 / 626); The Zero Drive 412 "Banish this" cost paid at resolution | sweep-costs (costDueNow, weaponmaster), sweep-triggers (381, 613 with the draw primitive), sweep-verify (pickSeveral, 412) | open |
| sweep-verify-cards | existing tests that assert something false or cannot fail (14 listed in its report, e.g. `C3Test.SquareUp_DoesNotForceADiscard` pins the missing Repeat cost as correct; `TargetlessTriggerAudit` is a proxy that an `onEquippedTrigger` passes) — each must be made able to fail or replaced | sweep-verify | open |
| perf-search | trainer notes: `currentStep()` / `applyChoice()` / `beginGame()` / `resumeFromSnapshot()` and `RiftboundState::legalIntents()` now return references that the next applied choice overwrites (a `const auto&` held across `applyChoice` is stale); `RiftboundGame` no longer owns its card table; new `ml::extractStateFeatures(state, perspective, db, out)` and `decisionsReplayedToBuild()`; observation values and layout unchanged | final report | open |
| perf-search | measured: MCTS-50 batch 36.7 s → 11.3 s, ISMCTS-20 batch ~20 s → 3.3 s (almost all from 6.7); two smoke runs were started without the lane lock by mistake | note | done |
| orchestrator | RETROFIT "not a defect still changes the code": every NOT A DEFECT closure made in stages 1 and 2 (about 20: ticked lines in batch-03, batch-06, batch-10 and sub-bullets under 02 2.20, 03 3.17, 3.18, 3.21, 07, 08 8.7, batch-01 Dark Child 315 and Shurelya's Requiem 509, batch-02, batch-08 Matriarch of War 940, Seal of Strength 163, reprint region tags, Stalking Wolf 728 Ambush half, Veteran Poro 779, Crowd Favorite 664 if closed that way) needs its pinning test and its code-site clarification; the three 04 items closed as obsolete (file deleted) are exempt | sweep-verify | open |
| orchestrator | REOPENED under "nothing optional": residuals that earlier streams closed as partial or documented — Solari Shrine 72 (killer not tracked), Unlicensed Armory 23 (rescue needs an Armory still in play), Scuttle Crab 615 (shows only cards facedown now), Astral Heron 831 (a countered response hides the first spell), Yone 544 (previous-controller field on `ScoreEvent`), Renekton 879 exact threshold edge, Defender of Tomorrow 936 (two abilities as one activation → stage 5 if it needs a vocab slot) | sweep-verify (936 → checkpoint) | open |
| cards-c7 | BLOCKED batch-07: Renekton, Brute 879 (engine event for Might crossing a threshold; only 5 is announced); Atakhan 732 (additional-cost hook that picks a unit to kill before payment and reads its printed cost); Loyal Poro 718 / Lonely Poro 777 (last-known "other friendly units here" snapshot for a batch of deaths — same work as Battle Mistress 517); Vex 712 (per-unit can't-move-this-turn flag read by the move generator); Poppy, Defender of the Meek 740 (XP optional additional cost with pre-payment reduction — Buccaneer path); Mageseeker Investigator 725 (multi-unit moves and a move cost); Reluctant Leader 908 / Vex 712 token plays (`createToken` does not announce a play, CR 350.2); Sky Crusier 847 (typed discard cost takes the first gear in hand; engine must ask) | sweep | open |
| cards-c7 | `shared:` commit `e52e52a` to review: `selectObjectAs(ctx, chooser, ...)` helper; `TriggerManager::onScore` fires delayed abilities with trigger `WhenIHold` on their controller's hold (Ashe registers hers with the opponent as controller and a far-future `expires_on_turn`); 4 existing test files corrected | sweep (review) | open |
| cards-c7 | Corrupted Dragon 878 uses `mode.victory_score + turn.victory_score_bonus` because `entersReadyOnPlay` has no executor — fold into the effective-victory-score row; Astral Heron 831 documented limit (a countered response hides the first spell); Black Rose Dignitary 714 still has a warning | sweep / cleanup | victory score done (sweep-flow: `effectiveVictoryScore()`); Astral Heron 831 limit → sweep-triggers; Black Rose Dignitary 714 warning → sweep-verify-cards — open |
| cards-c7 | trainer note: units asking more or different `MakeChoice` prompts — 722, 726, 732, 738, 741, 754, 810, 869, 888, 896, 901 | final report | open |
| cards-c7 | NEW: `createToken` Zilean doubling plays the copy at the same location with no choice | sweep (with Zilean 648) | open |
| cards-c7 | NEW: Fate Weaver 626 moves the chosen spell to hand by hand (no draw event, unused `ps`) — same class as Called Shot 443 | sweep | open |
| cards-c7 | NEW: `WhenOpponentPlaysAUnit` carries no `triggering_subject`; the played unit is passed through a counter on the watcher, so two plays before resolution overwrite it | sweep | open |
| cards-c6 | BLOCKED: Blue Sentinel 649 and Red Brambleback 591 ("hold / conquer effects for ... here trigger an additional time" — `TriggerManager::onScore` must queue those triggers an extra time); Undying Legion 587 (play-a-unit-from-trash path); Arachnoid Horror 679 (grant to other friendly units needs a `PlayerState` flag read by the unit-play generator); Lillia, Protector of Dreams 620 (trigger from `TokenCreatedEvent`); Zilean 648 (choice point inside `createToken`); Soraka 550 (recall primitive) | sweep | open |
| cards-c6 | Wily Newtfish 670: XP sources outside its range still write `xp +=` and must call `gainXp` — gear 658, 671, 698, 720; legends 759, 787; spells 657, 669, 751; battlefield 769; units 719, 724 | cards-c7 (719, 724), sweep (the rest) | open — c7 did 719 and 724; the rest is sweep-triggers item 9 |
| cards-c6 | `shared:` commit `65518a8` to review: `Card::canBeReadied()` (checked in `awakenPhase` and `readyObject`); `Card::optionalAdditionalCost(state, player)` overload; `recalculateAuras` resets `untargetable_by_enemy` in its own pass before the hooks; helpers `gainXp`, `selectAnyNumber`; `testHook_awakenPhase` | sweep (review) | open |
| cards-c6 | DUPLICATE: `selectAnyNumber` (c6, state in slots, composes with other helpers) overlaps `pickSeveral` (c5) — unify | sweep | open |
| cards-c6 | residuals on ticked items: Bard 540 pays "exhaust your legend" at trigger finalization, not during the play (`OptionalAdditionalCost` cannot express it); Scuttle Crab 615 shows only cards facedown now; Yone 544 tracks "battlefield that was uncontrolled" by a note in `applyPassiveAura` (a previous-controller field on `ScoreEvent` would be cleaner) | sweep | open |
| cards-c6 | NEW: Ambush units carry `Keyword::Reaction` from reminder text (560, 574, 583, 622, 633, 676, 682, 705) — decide whether intended (extends the `Keyword::Reaction` row and 8.8) | sweep (with 8.8) | open |
| cards-c6 | on-board guards added without their own test: Vayne 537, Vex 617, Hwei 642 | sweep (add tests) | open |
| cards-c6 | NEW: Crowd Favorite 664 can be activated while already buffed (legal, but costs 2 XP for nothing; random rollouts will do it) | sweep (required: fix, or close with evidence) | done (sweep-flow: NOT A DEFECT, CR 427.1.c; pinned by `SweepFlowTest.CrowdFavorite_MayBeActivatedWhileAlreadyBuffed`; clarified at the code site) |
| cards-c6 | reprint region tags (539, 543, 546, 547, 537, 548) closed NOT A DEFECT: each def matches its own printing in `cards/raw/gallery_raw.json`; gallery lists OGN-235 Karma as `Vi, Ionia` — see the c5 Karma row | user to confirm (final report) | open |
| cards-c5 | BLOCKED batch-05: Weaponmaster 324 / 414 / 421 / 448 / 407 / 435 and Jax, Unrelenting 440 (Equip cost needs an equip-cost descriptor on `Card`, 7.7); Minotaur Reckoner 337 (effect moves to base not stopped — needs the recall primitive); Commander Ledros 231 (interactive "kill N units" additional cost in the play path); Fae Dragon 423 second sentence (spend-a-buff primitive + trigger type, nine spending cards across streams); Lux, Crownguard 312 (spell-only Energy pool); Irelia, Graceful 462 ([1]-or-[A] choice and deferred-target spells in `executePlaySpell`); Ezreal, Prodigy 470 (optional-additional-cost discount) | sweep | open |
| cards-c5 | DEPENDENCY: Karthus now repeats only objects with `Keyword::Deathknell` — Baccai Witherclaw 865 and Noxian Emissary 915 must get the keyword on their defs or Karthus stops repeating them | cards-c7 (told in brief via 8.8); verify at stage 2 gate | done (c7: keyword added, Karthus engine-path test) |
| cards-c5 | `shared:` changes to review: `GameObject::aura_cant_move_to_base` (read by the standard-move generator); `EffectExecutor::lookBeforeReveal(player)` (other reveal-from-deck cards should call it); helpers `pickSeveral`, `createGoldExhausted` honouring `tokens_enter_ready`; `CostModifier::max_printed_energy_cost` (Jayce); Karthus repeat filter in `trigger_manager.cpp`; 3 existing tests corrected | sweep (review) | open |
| cards-c5 | reprints outside its range not edited: Karma 548, Darius 547, Plundering Poro 778 (3.18 defect is here, not 392), Veteran Poro 779 | cards-c6 (547, 548), cards-c7 (778, 779) | done (c6: 547, 548; c7: 778, 779) |
| cards-c5 | Karma, Channeler 235 tags set to `{Karma, Ionia}`; the gallery data itself says "Vi" for OGN-235 and no physical card was checked | user to confirm (final report) | open |
| cards-c5 | NEW: `CardTestFixture::addUnit` does not copy `shield_value` / `assault_value` / `deflect_value` from the def | cleanup (fixture) | done (cleanup: `copyPrintedStats`) |
| cards-c5 | NEW: Spectral Matron 226 and Rumble 349 carry a local "base or controlled battlefield" `pickMode`; switch to `pickEffectPlayLocation` | sweep | open |
| cards-c5 | NEW: Legion Quartermaster 367 / Zaun Punk 481 additional cost is paid at trigger finalization, after the unit has landed — true pre-play cost needs an engine hook (same family as Cruel Patron 208) | sweep | open |
| cards-c5 | NEW: Aphelios 372 "Ready 2 runes" readies the first two exhausted runes in map order; the player does not choose | sweep | open |
| cards-c5 | NEW: stale CLAUDE.md pitfall — says `combat_designation` is cleared between trigger fire and resolution; `processContestedBattlefields` keeps designations in step with `combat_in_progress` | sweep (CLAUDE.md) | open |
| cards-c5 | Fiora, Victorious 232: her own Shield no longer keeps her Mighty, but other auras still lag one pass (no layer system) | sweep (required: fix, or close with evidence) | done (sweep-flow: `hasMightDependentAura` second hook after the fold; also 619, 622) |
| cards-c3 | open batch-03: Smoke and Mirrors 645 never offered from facedown — hidden-play generator requires every target at the hidden battlefield, CR 811.1.d.4 frees the second target | sweep | done (sweep-flow `c081449`) |
| cards-c3 | open batch-03: Death Mark 931 `[A][A]` Flow from two domains — engine gap in the any-domain grant payment path (card's `flowCost` is correct) | sweep | open |
| cards-c3 | open batch-03: Square Up 579, Curtain Call 743 Repeat cost (card-declared Repeat cost, see the 8.9 row); card-side parts fixed | sweep | open |
| cards-c3 | Wild Claw 876 fixed without a regression test (could not construct a failing-payment path) | sweep (add test) | open |
| cards-c3 | DUPLICATE: `DelayedAbility::repeats` (c3) and `DelayedAbility::every_time` (c2) are the same flag added twice; `checkDelayedAbilities` honours both — unify | sweep | open |
| cards-c3 | `shared:` changes to review: `GameObject::banish_if_would_die_this_turn` (both kill paths, Smite); `__facedown_play_bf` / `__facedown_play_turn` counters stamped in both hidden-play paths; `playPayingCost` / `canPlayPayingCost` stage `current_play_source` from the card's zone (changes what `grantsAccelerateTo` sees as "played from hand" for effect plays); `checkDelayedAbilities` called on unit plays and conquers; helpers `facedownPlayBattlefield`, `playLocations`, `lockedPlayTarget`, `playTokensChoosingLocations`; 7 existing tests corrected | sweep (review) | open |
| cards-c3 | Dancing Grenade 582 and Death from Below 747 replay through `playIgnoringCost`, so the card sits in its owner's trash while the replay is on the chain — needs a "spell replayed while it resolves" state (ENGINE-GAP comments in both cards) | sweep | open |
| cards-c3 | NEW: `resolveSpell` drops illegal play-time targets and the rest shift position, so a multi-target card reading `targets[0]` / `targets[1]` as roles can swap them (fixed in Deathgrip and Void Assault by reading roles from the chain item; audit the others) | sweep | done (sweep-flow: `Card::chosenTarget(ctx, targets, role)`; 346, 484, 758, 645) — sweep-verify audits remaining multi-target cards |
| cards-c3 | NEW: Called Shot 443 "draw one" moves the card to hand by hand, emitting no draw event (same class as Double Trouble) | cards-c5 (443 is a unit? verify path) / sweep | open — sweep-triggers item 9 |
| cards-c3 | NEW (latent): `checkDelayedAbilities` iterates `state_.delayed_abilities` by reference while `addAbility` can run a finalize pass that pushes a delayed ability | sweep | open |
| cards-c3 | NEW: spell defs carrying a keyword only mentioned in their text — `Keyword::Temporary` on 632, 645, 727; `Keyword::Assault` with `assault_value` on 579 (extends 8.8) | sweep (with 8.8) | open |
| cards-c3 | NEW: Undying Loyalty 730 `selfCostReduction` cannot see printed costs; discount keys on tag and the card settles the difference when the unit is chosen — a cost hook that sees the chosen target would be cleaner | sweep (required) | open |
| cards-c1 | BLOCKED batch-01: Prodigal Explorer 514 (needs a counter instead of bool `chose_enemy_unit_this_turn`, set at five engine sites); Emperor of the Sands 512 (Weaponmaster for tokens, 8.4); Endless Riches 809 (play-from-trash for every card type + trash-to-banish replacement); 294 / 506 / 786 (restricted resource pools in `RunePool`: spell-only, gear-only, showdown-only); Swift Scout 259 (hide cost has no card hook in `executeHideCard`) | sweep | open |
| cards-c1 | Wuju Master 784 Level 11 — one line in the aura hook now that `PlayerState::units_enter_ready_by` (c4) is merged | sweep | done (sweep-flow `c081449`) |
| cards-c1 | Battle Mistress 517 once-per-batch is in c1's range, not c6's, and was NOT done: needs engine batching of simultaneous deaths | sweep | open |
| cards-c1 | batch-08 three-card item (274 / 671 / 783) fully fixed across c0 and c1 — tick it | sweep | open |
| cards-c1 | warnings left in untouched files in its range: 408, 430, 437, 439, 445, 455, 493, 507, 658 | cleanup | open — moved to sweep-verify-cards |
| cards-c1 | `shared:` changes to review: `TriggerType` `WhenAnEnemyUnitAttacksYourBattlefield`, `WhenYouMoveAnEnemyUnit`, `WhenYouRecycleARune` with dispatch in `onCombatStarted` / `onUnitMoved` / `onLeftBoard`; `beginningStep` no longer kills attached [Temporary] gear (CR 718.2); tests corrected in 12 existing files | sweep (review the 12 test edits) | open |
| cards-c1 | NEW: `grant_repeat_base_to_next_spell` carries only Energy, so a spell that also costs Power repeats for Energy alone (Temporal Portal 401 / The Academy 772) | sweep | open |
| cards-c1 | NEW: `is_rules_text_inactive` is set by only one attach path, not `attachEquipped` / `attachFree`, while `fireTrigger` skips triggers on gear with it — behaviour depends on how the gear was attached; Blighted Battleaxe 581 end-of-turn ability would be suppressed | sweep | open |
| cards-c1 | NEW: `TriggerManager::onUnitStunned` queues with `addAbility` directly: no `fired_trigger`, no `triggerCondition`, reads `triggerType()` not `triggerTypes()` | sweep | open |
| cards-c1 | NEW: a trigger queued while an Equip is being paid sits on the chain until something else runs it (`executeIntent` runs the chain only when `payAndQueueAbility` returns true) — same family as the search "item on chain, state Closed" row | sweep | done (sweep-flow `c081449`) |
| cards-c1 | NEW: `CardTestFixture::addToHand` leaves a unit's `base_might` at 0, so a unit played through `playThroughEngine` has 0 Might unless the test sets it | cleanup (fixture) | done (cleanup: `copyPrintedStats`) |
| cards-c1 | NEW: Chem-Baroness 556 reads `state.mode.victory_score` (ignores `victoryScoreModifier()`), same as the Leona 79 row | sweep (with the victory-score row) | done (sweep-flow `c081449`) |
| cards-c4 | BLOCKED batch-04: Brazen Buccaneer 2 (`canAfford` ignores the optional discard discount; `executePlayCard` must force the discard when full cost is unaffordable) and Kraken Hunter 150 (variable additional cost needs a prompt before payment) — same pre-payment code path | sweep | open |
| cards-c4 | BLOCKED batch-04: Leona, Zealot 79 victory score — `entersReadyOnPlay` cannot see the effective Victory Score; Find Your Center 47, Poppy 678, Corrupted Dragon 878 read `mode.victory_score` directly too. Needs a state-visible effective Victory Score (c2 added `TurnState::victory_score_bonus` — use it) | sweep | done (sweep-flow `c081449`) |
| cards-c4 | BLOCKED batch-04: Rebuttal 939 "[4] or less" half — `hasLegalTargets` sees only `GameState`, printed cost is in `CardDB` (same limit as Defy 45) | sweep (with the `hasLegalTargets` CardDB row) | done (sweep-flow `c081449`) |
| cards-c4 | reprints outside its range: Sett 543, Ahri 539, Vayne 537 need what 164, 119, 35 got (per-instance gate with the buff paid at activation; can't-be-chosen filter; on-board `still_legal`) | cards-c6 | done (c6) |
| cards-c4 | 16, 20, 217, 218 still override `requiresLegion()` because `ChainTestFixture.LegionParsesAsConditionGate` reads Duo's — delete with the virtual | sweep | done (sweep-flow `c081449`) |
| cards-c4 | `shared:` changes to review: `PlayerState::units_enter_ready_by` (read in `resolvePermanent`, `createToken`, `playIgnoringCost`); `BattlefieldState::opp_hidden_unrevealable` now per player; `GrantedAbilityRef::exhaust_cost_only`; helpers `pickEffectPlayLocation` / `effectPlayLocation` (other streams' batch-09 "lands in base" cards should use them) | sweep (review), card streams | open |
| cards-c4 | bisect note: commits `b475300` up to the batch-04 card commit do not build on their own | final report | open |
| cards-c4 | 8 unused-`targets` warnings left in untouched files in its range: 66, 73, 75, 87, 96, 137, 190, 216 | cleanup | open — moved to sweep-verify-cards |
| cards-c4 | NEW: `EffectExecutor::playIgnoringCost` never asks `entersReadyOnPlay`, so an effect-played unit is always exhausted unless Magma Wurm is out | sweep | open |
| cards-c4 | NEW: Vision look in `TriggerManager::onCardPlayed` never asks the player; it recycles the top card automatically when it is a spell | sweep | open |
| cards-c4 | NEW: Darius, Trifarian 27 does not see himself as "your second card" (play-a-card triggers skip the played unit); Cruel Patron 208 kill is a trigger after entering, not an additional cost to play (needs an engine hook) | sweep | open |
| cards-c2 | BLOCKED: Wallop 146 / Call to Glory 207 "spend a buff to ignore this spell's cost" need an alternative cost on the spell play path (`Card::alternativePlayCost` is read only for permanents; `generateSpellActions` drops an unaffordable spell; `executePlaySpell` always charges printed cost) | sweep | open |
| cards-c2 | `shared:` engine changes to review: `PlayerState::units_enter_ready_this_turn`, `unit_died_this_turn`, `buff_might_bonus_this_turn`, `hide_ignoring_costs_this_turn`; `TurnState::victory_score_bonus`; `TriggerType::WhenAUnitTakesDamage` + `TriggerManager::onDamageDealt`; `DelayedAbility::every_time`; delayed-ability chain items carry `triggering_subject`; `onScore` fires `WhenIConquer` for spells in the conquerer's trash; helpers `pickMoveDest`, `tokenPlayDests`, `destLocation`, `pickTokenDest` | sweep (review), checkpoint (new state fields → feature layout?) | open |
| cards-c2 | partial by design: Defy 45 cost caps checked at resolution only (`hasLegalTargets` has no CardDB, so it can be offered and do nothing); Defender of Tomorrow 936 keeps two printed abilities as one activation (splitting renumbers the Empower vocab slot) | sweep (45), checkpoint (936) | Defy 45 done (sweep-flow); Defender of Tomorrow 936 → checkpoint — open |
| cards-c2 | batch-08 Falling Star 29 / Angle Shot 334 / Whirlwind 187 fixed; Switcheroo 466 remains for that box | cards-c5 (466) | done (c3: Switcheroo 466) |
| cards-c2 | NEW: delayed-ability expiry compares `expires_on_turn == turn_number` but the header says `-1` means end of current turn — Deadly Flourish 635 and others that leave the default never expire | sweep | open |
| cards-c2 | NEW: `EffectExecutor::recycleCards` on a board permanent emits no `LeftBoardEvent`, puts tokens into the deck and leaves attachments linked (Divine Judgment works around it) | sweep | open |
| cards-c2 | NEW: `Card::hasLegalTargets` has no CardDB / registry, so cost-based target restrictions cannot be checked at offer time | sweep | done (sweep-flow `c081449`) |
| cards-c2 | NEW: hand-built multi-prompt cards cap at 12 prompts (16 re-entry limit in the resolve loop): Fox-Fire with many 0-Might units, Overt Operation with more than 12 buffed units | sweep (with the `runFinalizePass` 16 cap row) | done (sweep-flow: resume bound 1024 and it throws naming the card) — sweep-verify confirms Fox-Fire / Overt Operation with many units |
| cards-c2 | fixture note: `testHook_executeIntent` does not run the cleanup that kills lethally damaged units; damage-only spell tests assert on `damage_marked` | cleanup (fixture) | open — cleanup follow-up |
| cards-c2 | trainer note: several spells now ask more or different `MakeChoice` prompts | final report | open |
| cards-c0 | BUILD: the shared-ccache launcher documented in `CLAUDE.md` (integrity) restores PCHs across worktrees with different header contents — find the cause (direct-mode manifest? `-fmacro-prefix-map`?) or remove the launcher from `CLAUDE.md` | sweep | done (cleanup: cause found and reproduced, `scripts/ccache_pch_safe.sh`; orchestrator `f3670f3`: a PCH is never taken from the cache) |
| cards-c0 | BLOCKED batch-00 items needing engine work: Svellsongur 382 (dispatch bearer's abilities through the gear); Vilemaw's Lair 290 and Back-Alley Bar 272 (need a recall primitive distinct from `moveToBase`); Equip generator offers an unaffordable Equip, and Piltovan Forge 948 Equip branch bypasses discount and per-turn counter (need an equip-cost descriptor); Risen Altar 950 (needs a Power-side activation reduction) | sweep | open |
| cards-c0 | `shared:` engine changes to review in the sweep: `predict` asks an extra ordering prompt per kept card beyond the first (all Predict users: 834, 624, 843); `recalculateAuras` grants Equipment keywords before card hooks; `killObject` asks the dying unit's battlefield for a replacement; Academy's granted Repeat charges Power; new `TriggerType::WhenILeaveTheBoard`; `PlayerState::chosen_champion` | sweep (review), checkpoint (new state field → feature layout?) | open |
| cards-c0 | for cards-c1: Chem-Baroness 556 sets `card_counters["__gold_add_bonus_energy"]` on the legend from `applyPassiveAura`; Gold 564 needs the token flag; Seal of Strength 542 wrong domain | cards-c1 (messaged) | done (c1) |
| cards-c0 | for cards-c6: Nami 614 can use the new `WhenYouPlayAUnit` subject (7.12) | cards-c6 | done (c6) |
| cards-c0 | NEW: `EffectExecutor::killObject` `death_replacement_recall_pending` block detaches gear on a recall (same CR 719.3.a issue as `killUnit`) | sweep | open |
| cards-c0 | NEW: Equip generator and `payAndQueueAbility` skip a gear that is already attached, so attached Equipment can never be re-equipped to another unit — check against the CR | sweep | open |
| cards-c0 | NEW: `Card::confirmOptional` lists "No" first, so the fixture's default `EnginePathAgent` declines every "may"; a test that passes no agent silently tests the decline path | sweep (audit stage 2 tests for this) | open |
| cards-c0 | approximations left documented: Solari Shrine 72 (killer not tracked), Unlicensed Armory 23 (rescue needs an Armory still in play); Baited Hook 242 `[C]` is Order, not Chaos as the earlier carry-over row said | sweep (confirm) | open |
| integrity | 5.12 warnings carry-over (`-Wall -Wextra`): 120 sites — `card.h` 18 unused parameters (fix first, ~400 lines of noise), units 31, gear 18, spells 10, legends 3, battlefields 1, engine 3, io 3, `openspiel_tests.cpp` 1, tests 30, libstdc++ 2 | card streams fix warnings in card files they touch; cleanup takes `card.h`, engine, io, tests | open — card streams cleared the card files they touched; cleanup takes io / openspiel / tests; `card.h` and untouched card files go to sweep-verify |
| integrity | NEW: Karma, Channeler 235 has tags `{Vi, Ionia}`; reprint 548 has `{Karma}` — 235 would be a legal Chosen Champion for a Vi legend | cards-c5 | done (c5); printed card unconfirmed, see the user-to-confirm rows |
| integrity | NEW: token reference cards without `SuperType::Token`: Gold 326 / 564, Reflection 566, Bird 559, Sprite 569, token battlefields Brush 561 and Baron Pit 558 (only Recruit 954 and Shadow Clone 955 carry it); `affects_non_token_only` cost checks read `super_type` | cards-c0 (326, 558, 561), cards-c1 (564), cards-c6 (559, 566, 569) — verify ranges by path | done (c0, c1, c6) |
| integrity | NEW: Allay, Eager Admirer 603 is a champion unit with no champion tag (`{Yordle, Bandle City}`) — check the printed card | cards-c6 | done (c6: she is not a champion) |
| integrity | stale docs for 5.14: `card_registry.h` and `card_db.cpp` mention "legacy id-only constructor / registry.json" | cleanup | done (cleanup `e9f8b79`) |
| integrity | test PCH: each test file costs ~7.5 s even when nearly empty; anything path- or time-dependent in `card_test_fixture.h` breaks cross-worktree caching again | note for all streams | done (noted in briefs) |
| integrity | 25 test files wrapped in a file-named namespace for unity builds (incl. `test_review_fix_executor`, `_auras_costs`, `_chain_triggers`); `tests/test_review_fix_io_cli.cpp` is on `RIFTBOUND_TEST_UNITY_EXCLUDE` | note for all streams | done (noted in briefs) |
| io-cli | `scripts/generate_replays.py` is dead (targets the removed `riftbound_openspiel` binary, `decks/*.json`, a registry file); root `README.md` still lists it — delete or rewrite | cleanup | done (cleanup: rewritten against `build/riftbound`; NOT run — sweep-verify runs it once or removes it) |
| io-cli | fair-play mode now forwards no `log` lines, so the UI log panel is empty for a human player until `LogEvent` carries a visibility tag | sweep (with the LogEvent row) | open |
| io-cli | NEW: `LogEvent` (`src/core/events.h`) has no player / visibility tag — finer fix for the 4.5 trace leak than withholding the stream | sweep | open |
| io-cli | NEW: god-mode edits do not recompute the pending decision's legal actions (stale intent can be applied); documented, not fixed | sweep | done for the main-phase decision (cleanup `681533e`, two `PlayServerGodMode` tests); at a nested decision the list is kept and the reply says so — sweep-verify decides whether that is acceptable or needs the engine call |
| io-cli | NEW: spectator mode first Ctrl-C runs the AI game to completion before exit | cleanup (required; `src/io`) | done (cleanup `681533e`, `PlayServerShutdown.FirstShutdownEndsASpectatorGameAtTheNextDecision`) |
| io-cli | move `play_server.cpp` / `play_index_html.cpp` into `riftbound_core` so tests need not `#include` the .cpp files | sweep (required) | open — in `CMakeLists.txt` already; cleanup follow-up confirms |
| io-cli | deleted `src/openspiel/openspiel_match.cpp` (in no target) and `src/io/progress_bar.h`; 4.16, 4.17, 4.20 closed as obsolete — check nothing else referenced them | orchestrator (stage 1 gate) | done (checked at the stage 1 gate: only a code comment referenced them) |
| io-cli | not verified in a browser: page JS for `decision_id`, token query on the WebSocket, per-unit owner/controller on moves, hidden-zone counts; no TSAN run | final report | open |
| search | GATE: also build and run `riftbound_openspiel_tests` (`build/src/openspiel/riftbound_openspiel_tests`, `RIFTBOUND_ROOT=.`) — not part of `riftbound_tests`. 1.7 check: `RIFTBOUND_STEP_FAULT_AT=40 ./build/riftbound --deck1 decks/yi_rebuild.txt --deck2 decks/fiora.txt --agent1 mcts:sims=8 --agent2 random --games 4 --seed 7` must abort with the injected-fault message (exit 134), and exit 0 without the variable | orchestrator (every gate) | done (built into `.worktrees/gate.sh`, run at every gate) |
| search | `game_runner` / `batch_runner` do not catch exceptions: any engine or agent exception aborts the whole batch; should become a per-game error count | io-cli (owns the runners) | done (io-cli: batch exits 1 with game and seed) |
| search | HumanAgent interface for io-cli (4.3): `bool setChosenAction(int legal_index, uint64_t decision_id)` refuses a stale decision id; `explicit HumanAgent(PlayerId seat)`; server should send the rendered decision id, `main.cpp` should pass the seat | io-cli | done (io-cli follow-up) |
| search | 7.40 remainder: `openspiel_match.cpp` deck defaults, root `README.md` | io-cli | done (io-cli: file deleted, README repointed) |
| search | `CLAUDE.md` still describes the old replay model; add history tokens and `riftbound_openspiel_tests` | sweep | open |
| search | 6.7: `~StepDriver` still drains by playing the game out; a clone at a non-resumable decision with no resumable ancestor replays from game start (~1.5 ms vs ~0.11 ms) | perf-search | done (perf-search `3b41878`, `a434b7d`) |
| search | behaviour changes for other streams: `GameEngine::applyChoice` throws `std::out_of_range` on a bad index; `RiftboundState::ApplyAction` throws `std::invalid_argument` on an illegal action id (both used to apply action 0); `actionHistory()` returns by value; `makeFromSnapshot` gained `rng_seed`; `InfoSetId` / `ObservationString` values changed (1.13), feature layout unchanged | trainer note in the final report | open |
| search | NEW: `mainPhaseLoop` can reach its decision with an item on the chain and state Closed; `generateLegalActions` then builds the stale `priority_holder`'s options (seen as a lone "P1: PassPriority") while the engine asks the turn player. Repro: yi_rebuild vs fiora, seed 1, move 217 | sweep | done (sweep-flow `c081449`) |
| search | NEW: `Intent::operator==` ignores `use_alt_play_cost` and `granted_ability_def` | sweep | done (verified on integration: both fields compared, `intent.h:210-212`) |
| search | NEW (by design, note only): MCTS agents fall back to a uniformly random legal action at non-main-phase decisions when the live game cannot be reproduced from history (wrong seed, god-mode edit) | final report | open |
| search | 1.3: `ResumedEngineKeepsTheMainPhaseActionCount` not confirmed to fail without the fix | sweep (confirm) | open |
| engine-auras-costs | 5.4 remainder: three text matches left in the engine, each needs a `Card` virtual plus card edits — `setupBattlefields` ("can't move from here to base" / "can't be played here"), `generateMainPhaseActions` ("play me to an open battlefield" and variants), `parseRepeatCost` | sweep (ticks 5.4) | two of three done (sweep-flow); `parseRepeatCost` is sweep-costs — open |
| engine-auras-costs | 8.4 remainder: Weaponmaster for Sand Soldier tokens — keyword exists only as a card base class and tokens have no card class; needs a keyword-level "when you play me" handler with a real choice and payment | sweep (ticks 8.4) | open |
| engine-auras-costs | card files it already edited — card streams must re-read, not overwrite: battlefields 762, 775; legends 551, 784; units 15, 19, 28, 79, 84, 140, 232, 236, 243, 299, 349, 388, 411, 452, 467, 547, 609, 639, 675, 680, 709. Batch findings resolved by it: Farron (b04), Warmonger (b05), Rumble Hotheaded (b09), Eager Apprentice and Herald of Scales (b04), Vex (b05), Vaults of Helia (b00), Ornn's Forge 527 (engine). Fiora Victorious 232 def change NOT checked against her batch-05 entry | card streams (each in its range) | done (all card streams re-read them) |
| engine-auras-costs | 2.2 contract: `applyPassiveAura` is called once per (card, controller); 08 S3 unit and gear entries and the Heimerdinger duplicate-refs entry are fixed by the engine — verify and close | card streams / sweep | done (card streams verified their S3 entries); sweep-verify ticks |
| engine-auras-costs | battlefield hooks still run once per slot: same battlefield brought by both players doubles — War Camp, Void Gate, Kinkou Temple, Forbidding Waste, Black Flame Altar, Gardens of Becoming, Risen Altar need card-side guards (Brush has one; Marai Spire relies on per-slot) | cards-c0 | done (c0: all seven plus Ornn's Forge and Windswept Hillock) |
| engine-auras-costs | `EffectExecutor::payAnyPower` duplicates the rule in `payAdditionalCost` | sweep | open |
| engine-auras-costs | NEW: `killUnit` `death_replacement_recall_pending` block (Tactical Retreat) calls `detachAllGear` on a recall; CR 719.3.a says a recalled unit keeps its gear | sweep | open |
| engine-auras-costs | NEW: Shurelya's Requiem 509 hook grants Ganking at its battlefield but the def text has no such sentence | cards-c1 | done (c1: NOT A DEFECT, the sentence is in `effect_text`) |
| engine-auras-costs | NEW: `TriggerManager::onCardPlayed` — Vision fires once however many instances (CR 817.2: each triggers separately); also runs a full aura pass per permanent played (perf, plan 06) | sweep / perf-engine | open |
| engine-auras-costs | NEW: Fiora Victorious 232 Mighty check reads `current_might` incl. previous pass's auras (pass-order dependent) | cards-c5 | done (c5: own-Shield feedback loop fixed); one-pass lag is a separate row |
| engine-auras-costs | NEW: `CLAUDE.md` stale on three points (killUnit replacements described as text matching; power recycled from exhausted runes only; no mention of the once-per-(card, controller) aura contract); `game_engine.h` comment "cleanup has no agent-query sites" is wrong | sweep | open |
| engine-auras-costs | unplanned behaviour change to review: "to a minimum of [N]" no longer raises a card already cheaper than N | sweep (confirm against CR) | open |
| helpers | 3.1 card work: Blast Cone 695 (one `confirmOptional` per callback → own trigger or two-mode `pickMode`), Forgotten Signpost 607 (two `pickTarget` → `pickTargetPair`), Zenith Blade 258 stun, Thrill of the Hunt 745; general rule: no state change between two helper calls unless guarded to run once | cards-c1 (607, 695), cards-c2 (258), cards-c3 (745); sweep ticks 3.1 | card work done (c1, c2, c3, helpers); sweep-verify ticks 3.1 |
| helpers | 3.24 card work: 751, 41 → `splitDamageAmong`; 444 → shared `discardThenAct`; 405, 412, 498, 748, 263 → `payOnePower` / `payAnyPower` / `recycleRuneForPower` | card streams (each in its range); sweep ticks 3.24 | card work done (c1, c2, c3, c4, c5); sweep-verify ticks 3.24 |
| helpers | 7.30 card work: 927, 894, 937, 892, 935 → `offerableChoices` + `commitChosenObject`, or `pickTarget` | card streams (each in its range); sweep ticks 7.30 | card work done (c3, c4); sweep-verify ticks 7.30 |
| helpers | 7.1 probe: 26 cards never ask the controller on the real path — units 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199, 234, 238, 330, 361, 384, 453, 479, 539, 589, 618, 738; battlefields 278, 535, 763; gear 63. Enable each row in `Plan07Item71/DISABLED_TargetedTriggerTest` (`tests/cards/test_choice_helpers.cpp`) when fixed. Blast Cone 695 defect is 7.2; Forge of the Future 212 is not in the probe | card streams (each in its range) | done (all 26; DISABLED probe emptied) |
| helpers | Weaponmaster 7.7: Equip cost still not charged — needs an equip-cost descriptor on `Card` (a gear's cost lives inside its `onEquip`); 7.23 / 7.26 base fixed, Yone 544 and Sentinel Adept 331 carry their own copies | sweep (design), cards-c5 (331), cards-c6 (544) | open |
| helpers | NEW: file-local test helpers call `onTrigger` directly (`WeaponmasterTest::fireWeaponmasterTrigger`, `invokeOnTrigger` in `test_jhin_deck.cpp`), bypassing finalization | sweep | done (cleanup `2c2a2e0`) |
| helpers | NEW: `payEnergy` / `payableEnergy` in `card_helpers.h` count and exhaust any rune with a location, not only runes in the player's base, in map order | sweep | open |
| helpers | NEW: Keeper's Verdict 760 asks the spell's controller instead of the unit's owner | cards-c3 | done (c3) |
| helpers | NEW: Altar of Memories 490 `__altar_drew` guard (3.7) still breaks every death after the first | cards-c1 | done (c1) |
| chain-triggers | 7.1: 26 cards to convert per the rule under the item; remove each id from `kKnownOffenders` in `tests/test_review_fix_chain_triggers.cpp` and enable its row in `Plan07Item71/DISABLED_TargetedTriggerTest` | card streams (each in its range); sweep ticks 7.1 | card work done (`kKnownOffenders` empty); sweep-verify ticks 7.1 |
| chain-triggers | 8.9 last bullet: Square Up ("Discard 1") and Curtain Call (three costs) need a card-declared Repeat cost in `card.h` read by `executePlaySpell`, plus the two card files | sweep | open |
| chain-triggers | Irelia 380 and Jae Medarda 463 need `triggerCondition(subject == source)` | cards-c5 | done (c5) |
| chain-triggers | Treasure Trove 186 triggers only on a kill, not on a bounce | cards-c0 | done (c0: `WhenILeaveTheBoard`) |
| chain-triggers | 8.16 bullet "spell that banishes itself is also filed in trash" is fixed in `chain_manager.cpp`; note it when ticking 8.16 | sweep | open |
| chain-triggers | 2.11 root cause: the play gate should check card cost plus Deflect together | sweep | open |
| chain-triggers | NEW: `ChainManager::addSpell` / `addAbility` charge Deflect without consulting `Card::ignoresDeflect()` | sweep | open |
| chain-triggers | NEW: `TriggerManager::onUnitDied` queues a unit's own death ability directly, bypassing `triggerCondition` and the LeBlanc Temporary suppression | sweep | open |
| chain-triggers | NEW: `GameEngine::runFinalizePass` caps at 16 choices and stops silently | sweep | done (sweep-flow `c081449`) |
| chain-triggers | NEW: Battle Mistress 517 triggers once per dead enemy unit instead of once per "one or more" | cards-c6 | superseded by the cards-c1 Battle Mistress row (sweep-triggers) |
| chain-triggers | NEW: Void Burrower 553 declares `getTargetRequirements()` that nothing reads (allow-listed in the audit test) | cards-c1 | done (c1) |
| executor | 2.15 card recipes: Abandon 693, Scryer's Bloom 698, Stacked Deck 183, Baited Hook 242, Last Rites 471 recycle via `deck::putOnBottom` without announcing `"recycled_main"` | cards-c3 (693, 698), cards-c2 (183), cards-c0 (242), cards-c1 (471) | done (c0, c1, c2, c3) |
| executor | 2.14: cards printing "if it was buffed this way" must branch on `buffUnit`'s returned bool | card streams (each in its range) | done (no card prints it in any range) |
| executor | 8.13: manual erase before `playIgnoringCost` / `playPayingCost` is redundant, drop it | card streams (each in its range) | done (all card streams) |
| executor | 8.14: gear attached to an off-board unit (`attachFree`, hand-rolled attaches) not covered by the executor guard | sweep | open |
| executor | NEW: Aspiring Engineer 384 "return a gear from your trash" coded as `bounceToHand` on a board gear | cards-c5 | done (c5) |
| executor | NEW: Blind Fury 25 and Void Rush 505 edit `main_deck` directly, bypassing `deck_ops` and the knowledge tracker | cards-c2 (25), cards-c3 (505) | done (c2: 25; c3: 505) |
| executor | NEW: `recycle_self` cost and the mulligan recycle in `game_engine.cpp` do not announce `"recycled_main"` | sweep | open |
| executor | NEW: `EffectExecutor::moveToBase` is used for recalls too and now counts a move; CR 458 says a recall is not a move — needs a recall primitive and a caller audit | sweep | open |
| executor | temp Deflect lives in turn-stamped `card_counters`; a `temp_deflect_value` field expired in `expirationStep` would be cleaner | sweep (required) | open |
| engine-play-combat | 8.7: engine half NOT A DEFECT; cards still to fix per the recipe under the item — Hand of Noxus 296, Vanguard Captain 218, Sun Disc 21, Scorchclaw 578, Undying Legion 587 (needs play-unit-from-trash) | cards-c0 (21), cards-c4 (218), cards-c5 (296), cards-c6 (578, 587); sweep ticks 8.7 | card work done (c0: 21, c1: 296, c4: 218, c6: 578); 587 is in the c6 BLOCKED row; sweep-verify ticks 8.7 |
| engine-play-combat | 8.6 card recipes: Bellows Breath 402, Piercing Light 346, Deathgrip 484, Smoke and Mirrors 645 need `enumerateLegalTargetSets` | cards-c2 (346), cards-c3 (402, 484, 645) | done (c2: 346; c3: 402, 484, 645 from hand); 645 facedown is a separate row |
| engine-play-combat | 2.27 card recipes: Baited Hook 242 and Poro Snax 369 misuse `recycle_self` | cards-c0 (242, 369) | done (c0) |
| engine-play-combat | `generateTrashReplayActions` duplicates the two legality gates of `canAfford`; discounts (CR 356.4) not applied to a grant cost | sweep | open |
| engine-play-combat | delete `requiresLegion` / `requiresLevel` / `levelThreshold` once the 8.7 cards self-gate | sweep | done (sweep-flow `c081449`) |
| engine-play-combat | NEW: `chain_manager.cpp` `stepExecuteAndPass` PlayReaction branch skips `Card::onPlay`, optional additional cost, per-turn gear/equipment counters and the `"played_not_from_hand"` event | sweep | open |
| engine-play-combat | NEW: `generateShowdownActions` offers only `is_action` abilities — `[Reaction]` abilities and aura-granted abilities are not offered in showdowns / Closed States (CR 813.1.b) | sweep | done (sweep-flow `c081449`) |
| engine-play-combat | NEW: `generateMainPhaseActions` reads play-to-location permissions by `ability_text.find("play me to ...")` | sweep | done (sweep-flow `c081449`) |
| engine-play-combat | NEW: a `[Hidden]` champion cannot be hidden from the Champion Zone (CR 811.1.b); hidden cards not offered in a showdown Open State | sweep | done (sweep-flow `c081449`) |
| engine-play-combat | NEW: `Keyword::Reaction` set on ~25 permanents that only have a `[Reaction]` ability (extends 8.8); `extraAnyPowerCost` reads it, so Mystic Vortex taxes playing them | sweep (with 8.8) | open |
| engine-play-combat | NEW: `combatResolutionStep` does not implement CR 468.3.d.1 (re-stage showdown and combat on No Result with both players present) | sweep | done (sweep-flow `c081449`) |
| engine-play-combat | NEW: `Card::hasLegalTargets` default passes a fixed-count spell with fewer legal targets than its count; generator then emits zero intents | sweep | done (sweep-flow `c081449`) |

## Log

One line per orchestrator action that changed state (merge, gate, respawn).

- 2026-10-10 stage 0: plans committed on master as `c7143f1`; `fix/integration` created; baseline build, tests and smoke recorded. Gate passed.
- 2026-10-10 stage 1: spawned search, helpers, executor, chain-triggers, engine-auras-costs, engine-play-combat (6 running). integrity and io-cli wait for a slot (io-cli gate is open: main checkout clean).
- 2026-10-10 chain-triggers agent stopped by the user before reporting; worktree left untouched, not respawned.
- 2026-10-10 chain-triggers resumed by the user (the stop was a mistake); back to `running`.
- 2026-10-10 merged engine-play-combat (`ec13e47`), suite 1534/1537 with only the 2 baseline failures; 16 boxes ticked, verified against the report; 8.7 and 11 other rows moved to carry-over. Worktree and branch removed.
- 2026-10-10 spawned integrity from `ec13e47`.
- 2026-10-10 user approved batched merges (see Run decisions).
- 2026-10-10 merged executor, suite 1557/1560 with only the 2 baseline failures; 8 boxes ticked and verified; 9 rows to carry-over. Worktree and branch removed.
- 2026-10-10 spawned io-cli from `bbc995d`.
- 2026-10-10 merged chain-triggers (`c9ad6b4`), suite 1580/1583 with only the 2 baseline failures; 12 boxes ticked and verified; 11 rows to carry-over. Worktree and branch removed.
- 2026-10-10 helpers returned; sent back for two regressions before merge.
- 2026-10-10 recorded carry-over from the engine-auras-costs and helpers reports ahead of their merge verification (12 and 9 rows).
- 2026-10-10 merged engine-auras-costs (`a8af2ef`), suite 1628/1631 with only the 2 baseline failures; 10 boxes ticked and verified. Worktree and branch removed.
- 2026-10-10 helpers (`2e0f181`) and search (`44a7541`) merged as one batch; verification build queued. 11 search rows to carry-over.
- 2026-10-10 io-cli (`ae7961b`) merged into the same batch; io-cli sent back for four follow-ups. 6 rows to carry-over.
- 2026-10-10 batch helpers + search + io-cli verified: 1707 run, 1707 passed, 0 failed, 28 disabled (27 are the 7.1 work-list rows). Baseline failures are gone (7.40). helpers and search worktrees and branches removed; io-cli kept for its follow-up (also fixing a WebSocket-handshake race it found by repeating its server tests).
- 2026-10-10 early gate run at `c0975fc` all green (see Gates). io-cli follow-ups (`f3e2208`) merged; verification batched with integrity.
- 2026-10-10 user limited builds to `-j3` (see Run decisions).
- 2026-10-10 user restored `-j6` and asked for CMake to enforce it (integrity adds `RIFTBOUND_BUILD_JOBS`).
- 2026-10-10 integrity (`fc1f7cb`) merged; integration reconfigured with the shared-ccache launcher; stage 1 gate run started. AGENT_BRIEF.md updated (launcher, job cap, unity-test rule, shell rule). 7 rows to carry-over.
- 2026-10-10 STAGE 1 GATE PASSED at `b0d329b`. integrity and io-cli marked merged; their worktrees and branches removed.
- 2026-10-10 master fast-forwarded to `1640e81`. Stage 2: spawned cards-c0..c5 (6 running); c6 and c7 wait for a slot.
- 2026-10-10 cards-c0 returned and merged; it found the shared-ccache launcher restoring a foreign PCH. All running streams told to use a private cache namespace and clean-build before their final run; integration reconfigured the same way and rebuilt from clean. 9 rows to carry-over.
- 2026-10-10 cards-c0 verified on a clean private-cache build (gate-c0-clean, 4.5 min cold): all checks green. Worktree and branch removed. Spawned cards-c6.
- 2026-10-10 cards-c2 (`2fe1205`) returned and merged, gate running. 10 rows to carry-over.
- 2026-10-10 cards-c4 (`fd00d2c`) merged into the batch with c2; two-line engine conflict resolved by keeping both conditions (see merge commit `12c4935`). 11 rows to carry-over. Decision: mechanical both-sides conflicts (appended helpers, appended plan sub-bullets, independent conditions) are resolved by the orchestrator and verified by the gate; anything else goes back to the stream.
- 2026-10-10 cards-c1 (`7e89826`) merged into the batch with c2 and c4 (merge `faa7516`); conflicts were appended enum values, appended plan sub-bullets and the kKnownOffenders list, resolved mechanically. 12 rows to carry-over.
- 2026-10-10 cards-c3 (`c2def86`) merged into the batch (merge `35bb914`); conflicts mechanical, resolved with `.worktrees/resolve_plans.py` for plan files. Batch awaiting gate: c1, c2, c3, c4. 12 rows to carry-over.
- 2026-10-10 first batch gate (label c2) FAILED TO BUILD: orchestrator's keep-both resolution in `card_helpers.h` dropped a closing brace (fixed in `e36ffa6`). `gate.sh` had gone on to test stale binaries after the failed build; it now aborts on a failed build. Lesson: compile-check a resolved code conflict before committing the merge.
- 2026-10-10 batch c1+c2+c3+c4 verified at `e36ffa6` (gate-batch1): 2040 run, 2040 passed (1783 + 67 + 68 + 72 + 50), openspiel 17/17, clone-equiv 10/10, smoke 135 / 65 / 0 rc=0, fault injection rc=1. Worktrees and branches removed. Spawned cards-c7.
- 2026-10-10 cards-c5 (`870307e`) merged cleanly (merge `b95b4e4`), gate running. 11 rows to carry-over.
- 2026-10-10 cards-c5 verified (gate-c5): 2100 run, 2100 passed; smoke 141 / 59 / 0 rc=0 (baseline was 128 / 72: the fiora-vs-yi split has drifted as cards were fixed — expected, but worth a look in the final report). Worktree and branch removed.
- 2026-10-10 cards-c6 (`d06b58d`) merged cleanly, gate running. 9 rows to carry-over.
- 2026-10-10 cards-c6 verified (gate-c6): 2161 run, 2161 passed; smoke 135 / 65 / 0. Worktree and branch removed. cards-c7 messaged with the c5/c6 hand-overs.
- 2026-10-10 cards-c7 (`8ff64db`) merged cleanly; stage 2 gate started on a clean rebuild. 7 rows to carry-over.
- 2026-10-11 STAGE 2 GATE PASSED at `76ab21f` (clean rebuild). cards-c7 worktree and branch removed.
- 2026-10-11 master fast-forwarded to `6a3e52b`. Stage 3: spawned sweep-costs, sweep-flow, sweep-triggers, cleanup, perf-search (5 running).
- 2026-10-11 carry-over reconciled against the stage 1 and stage 2 stream reports: 41 rows marked done, 3 annotated; the rest are open and assigned to stage 3 streams, the final report, or the user.
- 2026-10-11 user: nothing may be left optional (see Run decisions). Relabelled 10 rows, reopened the documented residuals, messaged the five running stage 3 streams.
- 2026-10-11 user: a not-a-defect closure still needs a pinning test and a clarification at the code site (see Run decisions). Brief updated, running sweep streams messaged, retrofit of earlier closures assigned to sweep-verify.
- 2026-10-11 wrote "Resume notes" at the top of this file ahead of a context reset; stage 3 streams told to write their final reports to `.worktrees/reports/<stream>.md`.
- 2026-10-11 context reset survived: all five stage 3 streams still running, state resumed from this file. User: two build lanes at `-j3` (see Run decisions); the five streams messaged with their lane, brief updated.
- 2026-10-11 user asked for remaining implementation to start early. Spawned sweep-verify-cards (card-only half of sweep-verify) in the sixth slot. perf-engine, strong-ids and checkpoint NOT started early: each rewrites files the running streams are editing (6.1 trace call sites, id types tree-wide, feature layout).
- 2026-10-11 00:40 orchestrator handover to session riftbound-engine-d6 (see Resume notes); the old session keeps hosting the agents. Monitor armed on `.worktrees/reports/`.
- 2026-10-11 old session confirmed it stood down (host and relay only). Trainer survey returned (`.worktrees/reports/trainer-survey.md`); its engine-side finding (CMake paths break the trainer's subdirectory build) relayed to cleanup and added to carry-over.
- 2026-10-11 00:52 old host process (pid 2329) found hung: no agent activity since 00:42, unreachable, no terminal. Killed. sweep-costs, sweep-flow, sweep-triggers, cleanup, perf-search and sweep-verify-cards resumed from their transcripts in this session; about 10 minutes of agent time and any in-flight build lost, no file work lost. CMake trainer-compat item re-sent to cleanup directly.
- 2026-10-11 user: after the final merge, build the v6 ML tooling without waiting (no real training, no baselines); action history and public zones are required in the observation (see Run decisions).
- 2026-10-11 cleanup returned (4 commits, `e9f8b79`) and merged as `a8723a4`; gate running on a clean rebuild with the new PCH-safe launcher. cleanup sent back for three brief items it had lost on resume (warnings in its files, `testHook_executeIntent` cleanup, play-server confirmation). All streams told to switch launcher and rebuild clean. Merge gates now take lane A only.
- 2026-10-11 cleanup gate: first run FAILED TO BUILD (`types.h` redefinitions in every unit): after cleanup's comment-only edit to `types.h`, ccache returned the previous precompiled header on a preprocessed-text hit and `#pragma once` no longer matched. Fixed in `f3670f3` (the launcher always compiles a PCH itself). Re-run gate-cleanup2 green: 2206/2206. cleanup marked merged; worktree kept for its follow-up.
- 2026-10-11 sweep-flow returned (`1dad1be`), merged as `1559f29`, gate green: 2248/2248. Worktree and branch removed. sweep-costs and sweep-triggers told to merge integration into their worktrees and resolve conflicts before reporting; Svellsongur 382 handed to sweep-triggers. sweep-verify-cards moved to lane B.
- 2026-10-11 sweep-verify-cards wrote its first report (48 + 2 boxes ticked, about 30 new card defects found). Not merged yet: sent back to merge integration, re-check boxes sweep-flow unblocked, and fix its own new card defects; second report expected as `sweep-verify-cards-2.md`.
- 2026-10-11 user: make sure findings are recorded. Created `plans/09-found-during-fix-run.md` (44 boxes: 25 new card / helper defects and 13 unsound tests from sweep-verify-cards, 6 from cleanup and sweep-flow) and `plans/reports/` (four reports). Open boxes now 128 + 44 = 172.
- 2026-10-11 sweep-triggers returned (`1e9eb2a`, integration already merged in by the stream), merged as `202b779`, gate running. 18 boxes ticked. Its resumed agent had also lost its brief and rebuilt scope from the carry-over table: checked against the in-flight table — Astral Heron 831 missing, several fixes without tests, 38 tests never shown red. 13 items added to plans/09 section D; stream sent back for a follow-up.
- 2026-10-11 perf-search returned (`b549cc4`): 6.4, 6.7, 6.8, 6.9 closed with before / after numbers, own suite 2257/2257. Waiting for the sweep-triggers gate to finish before merging (the gate builds in the integration tree). 3 items added to plans/09 section E.
- 2026-10-11 sweep-triggers gate green (2300/2300); marked merged, worktree kept for its follow-up. perf-search merged as `bdb8664` with no conflicts; gate running.
- 2026-10-11 perf-search gate green (2309/2309, openspiel 20/20); marked merged, worktree and branch removed. cleanup's first follow-up (`bbe7381`, `2c7d674`, `0227a08`) being merged next; its second follow-up is running.
