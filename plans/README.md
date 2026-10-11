# Engine review fix run — orchestrator plan

**Mission:** fix every finding from the 2026-10 engine code review as one
orchestrated patch, using parallel subagents in git worktrees, merged back
stage by stage, with nothing left behind. **You are the orchestrator.** This
file is your standing instruction on every loop iteration. Subagents get
[AGENT_BRIEF.md](AGENT_BRIEF.md), not this file.

**Where the plan items are (since 2026-10-11).** Each plan file is split in two
directories that mirror each other:

- `plans/remaining/` — only the items still open. When this directory holds no
  `- [ ]`, the plan work is done. `plans/remaining/README.md` lists them.
- `plans/completed/` — every closed item, with the outcome written under it, in
  the same files and sections as before (`completed/02-engine-rules.md`,
  `completed/cards/batch-03.md`, `completed/cards/INDEX.md`, ...).

To close an item: tick it, write the outcome under it, and move the whole item
from the file in `remaining/` to the same section of the file in `completed/`
(delete the `remaining/` file when it has no item left). A new finding goes in
`remaining/09-found-during-fix-run.md`. Where this file or `STATUS.md` names a
plan file without a directory (`02-engine-rules.md`, `cards/batch-00.md`), read
it as "under `completed/` or `remaining/`".

The run is not finished until the completion criteria in section 9 all hold.
Do not end the loop before then. If you hit a deliberate hard blocker
(section 8), stop and tell the user plainly instead of marking things done.

## 1. Every loop iteration

1. Read `STATUS.md` from the integration worktree
   (`.worktrees/integration/plans/STATUS.md`); if it does not exist yet, you
   are at stage 0.
2. Sync: if `master` has commits integration lacks, merge `master` into
   `fix/integration` (section 5), rebuild and re-test before anything else.
3. Collect: for every stream agent that has returned, run the merge procedure
   (section 5) and update `STATUS.md`.
4. Advance: if the current stage's streams are all `merged`, run its gate
   (section 6). If the gate passes, start the next stage.
5. Spawn: start every stream of the current stage that is `pending` and not
   gated, up to **6 running at once**. Send independent spawns in one message.
6. Record what changed in the `STATUS.md` log and commit it on integration.
7. If agents are still running, schedule a long fallback wakeup (about 30
   minutes); you are re-invoked automatically when an agent finishes, so do
   not poll. Never fabricate an agent's result: act only on returned reports.

State lives on disk (`STATUS.md`, plan checkboxes, branches, worktrees), so a
fresh iteration with no memory of the last one can always resume from step 1.

## 2. Hard rules

- **The main checkout belongs to someone else.** Another session works in
  `/home/pirateporkchop/code/riftbound-engine` on `master` and may have
  uncommitted changes there. In the main checkout you may only: commit
  `plans/` at stage 0, and fast-forward `master` at a gate (section 6). Never
  stash, reset, checkout, clean or build there. Never touch `build/`.
- All work happens in worktrees under `.worktrees/`. One stream, one worktree,
  one branch `fix/<stream>`.
- One build at a time, always through
  `flock .worktrees/.build.lock` (commands in `AGENT_BRIEF.md`).
- Never merge red. Integration must pass the full test suite after every
  merge, or the merge is undone.
- Never tick a checkbox on a stream's say-so alone: spot-check the diff for
  every `FIXED`, and the evidence for every `NOT A DEFECT`.
- Do not push to any remote. Do not open pull requests.
- Do not change `kStateFeatureDim`, `kCardVocabSize`, the feature layout or
  the action vocabulary before stage 5.
- Do not delete or weaken a test to get green.

## 3. Stage 0 — baseline (you do this yourself, no subagents)

1. `git worktree list` and `git branch --list 'fix/*'`: if a previous run left
   state, resume it from `STATUS.md` rather than starting over.
2. In the main checkout, commit the plans so worktrees can see them — only
   that path: `git add plans && git commit -m "Engine review: findings and fix plan" -- plans`
   (end the message with the Co-Authored-By line from `AGENT_BRIEF.md`).
   Do not add anything else. `decks/fiora.txt` is untracked and not yours:
   leave it exactly as it is. (The 38 out-of-date decks were deleted on
   `master` in `581ef91`; only `decks/yi_rebuild.txt` is tracked.)
3. Add `.worktrees/` to `.git/info/exclude`.
4. Create the integration branch and worktree from `master`:
   `git worktree add -b fix/integration .worktrees/integration master`.
5. Configure, build and run the full suite there (under the lock). Record the
   baseline commit and the pass/fail counts in `STATUS.md`. Tests that fail at
   baseline are recorded by name; they are not a reason to stop, but no stream
   may add to that list.
6. Build the `riftbound` binary and `riftbound_clone_equiv_test` once and
   record the baseline smoke result (section 6, "smoke").
7. Commit `STATUS.md` on integration. Stage 0 gate passed.

## 4. Stages and streams

Each stream owns files and plan items. Ownership is what keeps parallel
branches mergeable, so hold streams to it. Item ids are checkbox ids in the
plan files (`2.4` is in `02-engine-rules.md`, `8.6` in
`08-cards-full-review.md`, and so on).

### Stage 1 — engine foundation (parallel)

Card fixes depend on these, so stage 2 does not start until stage 1 is merged.

| Stream | Owns (files) | Items |
|---|---|---|
| `search` | `src/engine/step_driver.*`, `src/openspiel/*` except `openspiel_match.cpp`, `src/agents/*`, `src/ml/*` (no layout change), the step API in `game_engine.{h,cpp}` (`beginGame`, `applyChoice`, `currentStep`, `refreshStepFromDriver`, `resumeFromSnapshot`, `recordAppliedIntent`) | 01: 1.1–1.8, 1.10, 1.11, 1.13, 1.14 · 07: 7.37–7.40 |
| `helpers` | `src/cards/card.{h,cpp}`, `card_helpers.h`, `gear/equip_base.h`, `units/weaponmaster_base.h`, `tests/cards/card_test_fixture.h` | 03: 3.1, 3.23, 3.24 · 07: 7.3, 7.29, 7.30, 7.32 |
| `executor` | `src/engine/effect_executor.{h,cpp}` | 02: 2.14, 2.15, 2.21, 2.22, 2.28, 2.31 · 08: 8.13, 8.14 |
| `chain-triggers` | `src/engine/chain_manager.*`, `trigger_manager.*` | 02: 2.4, 2.5, 2.10, 2.11, 2.13, 2.16, 2.17, 2.32 · 07: 7.4 · 08: 8.5, 8.9, 8.11, 8.15 · the engine half of 07 item 7.1 (see note) |
| `engine-auras-costs` | in `game_engine.cpp`: `recalculateAuras`, `refreshFlowGrants`, `canAfford`, `beginCostPayment`, `payCardCost`, `payRepeatCost`, `payAdditionalCost`, `executeHideCard`, the Altar of Blood block in `killUnit`, `anyDamageIsLethalFor`, `processLethalDamage` | 02: 2.1, 2.2, 2.3, 2.25, 2.26 · 05: 5.4, 5.9, 5.10 · 08: 8.1, 8.2, 8.3, 8.4 |
| `engine-play-combat` | the rest of `game_engine.{h,cpp}`: `executeIntent`, play and resolve paths, all `generate*Actions`, showdown, combat, scoring, `moveUnit`, `killUnit` | 02: 2.6, 2.7, 2.8, 2.9, 2.12, 2.18, 2.19, 2.20, 2.23, 2.24, 2.27, 2.29, 2.30 · 08: 8.6, 8.7, 8.10, 8.12 |
| `io-cli` | `src/io/*`, `src/main.cpp`, `src/openspiel/openspiel_match.cpp`, `src/engine/batch_runner.*`, `game_runner.*`, `docs/play-api.md`, root `README.md` | 04: all |
| `integrity` | `src/rules/*`, `CMakeLists.txt`, `src/core/*` (not the id typedefs), `src/cards/card_registry.*` | 05: 5.1, 5.2, 5.3, 5.6, 5.7, 5.8, 5.11, 5.12 |

Notes for stage 1:

- **`engine-auras-costs` and `engine-play-combat` share one file.** Run them
  in parallel but merge one, then have the other rebase before merging.
- **`io-cli` is gated.** Start it only when the main checkout has no
  uncommitted changes under `src/io`, `src/main.cpp` or `docs/play-api.md`
  (`git -C <main> status --porcelain -- src/io src/main.cpp docs/play-api.md`
  is empty). Until then leave it `pending` and carry on; it may run alongside
  stage 2. It must be merged before the stage 3 gate.
- **7.1 (target-less triggers):** `chain-triggers` decides and implements the
  mechanism (the engine choosing targets for a trigger at finalization, or a
  documented "cards must use `pickTarget` + `kFinalizeTrigger`" rule plus the
  fixture change in 7.3). It writes the decision as a sub-bullet under 7.1.
  The 26 cards themselves are fixed in stage 2 against that decision.
- **8.1:** removing the text-parsed aura pass requires a card-side
  `applyPassiveAura` for every card that relied on it. `engine-auras-costs`
  may edit those card files (list them in the report so stage 2 skips them).
- **5.12:** add warnings without `-Werror`; fix what `-Wall -Wextra` shows in
  files the stream owns and report the rest as carry-over.
- **1.10 / 4.3:** `search` owns `human_agent.h`; `io-cli` does the server side.

### Stage 2 — cards (8 streams in parallel)

Every card file belongs to exactly one stream, by path range:

| Stream | Card files | Own batch file |
|---|---|---|
| `cards-c0` | `battlefields/*`, `gear/0001`–`gear/0387` | `cards/batch-00.md` |
| `cards-c1` | `gear/0388`–end, `legends/`–`0928` | `cards/batch-01.md` |
| `cards-c2` | `legends/0929`–end, `runes/*`, `spells/`–`0363` | `cards/batch-02.md` |
| `cards-c3` | `spells/0364`–`0933` | `cards/batch-03.md` |
| `cards-c4` | `spells/0934`–end, `units/`–`0218` | `cards/batch-04.md` |
| `cards-c5` | `units/0219`–`0494` | `cards/batch-05.md` |
| `cards-c6` | `units/0495`–`0709` | `cards/batch-06.md` |
| `cards-c7` | `units/0710`–end | `cards/batch-07.md` |

[cards/INDEX.md](completed/cards/INDEX.md) lists, per stream, every card file any plan
mentions and the `plan-file:line` of each mention. A card stream's work list
is: every card in its INDEX section, for every pointer.

Who ticks what:

- A card stream ticks checkboxes in **its own batch file**, and checkboxes in
  `cards/batch-08.md`, `batch-09.md`, `batch-10.md` whose card is in its range.
- For any other plan line INDEX points to (03, 07, 08 sections A–C, 02), the
  card stream fixes its card and appends a sub-bullet under that item:
  `  - cN: <id> <name> — FIXED: ... ; test: ...` (or NOT A DEFECT with
  evidence). It does **not** tick the box; the stage 3 `sweep` does.
- Card streams add tests under `tests/cards/`, one new file per stream
  (`test_review_fix_cN.cpp`), so streams never edit the same test file.

Card streams may not edit the engine or shared headers beyond a small
`shared:` commit (see the brief). Larger needs go to carry-over and are
handled by `sweep`.

### Stage 3 — sweep, cleanup, performance

| Stream | Owns | Items |
|---|---|---|
| `sweep` | anything, serial with respect to cards | Every still-open checkbox in 03, 07 and 08 sections A–C. For each: verify every card and sub-bullet named is resolved in the code (not just reported), fix what is missing, then tick. Also everything in the `STATUS.md` carry-over table. 08 item 8.16 is ticked here. |
| `cleanup` | dead code and comments only | 05: 5.13, 5.14 |
| `perf-search` | `src/openspiel/*`, `src/agents/*`, `src/ml/*`, `step_driver.*` | 06: 6.4, 6.7, 6.8, 6.9 |
| `perf-engine` | engine, core, trigger manager | 06: 6.1, 6.2, 6.3, 6.5, 6.6, 6.10, 6.11 |

- Run `sweep`, `cleanup` and `perf-search` in parallel. Start `perf-engine`
  only after `sweep` and `cleanup` are merged: 6.1 touches trace call sites
  across the whole tree and would conflict with everything.
- Performance items were never measured. Each perf stream records a before
  and after number (the clone microbenchmarks in `src/openspiel/`, and wall
  time for a fixed-seed 200-game batch) in its report and under the item. A
  change that does not help is reverted and the item closed NOT A DEFECT with
  the numbers.
- 6.7 (unwinding the fiber instead of playing the game out) depends on 1.7.

### Stage 4 — strong id types (one stream, alone)

| Stream | Items |
|---|---|
| `strong-ids` | 05: 5.5 |

Touches most files, so it runs with nothing else in flight and merges before
anything else starts.

### Stage 5 — checkpoint-breaking changes (one stream, alone)

| Stream | Items |
|---|---|
| `checkpoint` | 01: 1.9, 1.12 · 07: 7.33 |

These change the feature layout and action vocabulary and **invalidate
existing trainer checkpoints**. Before spawning, check `master` and the
v6 work (`src/core/legal_action.h`, `knowledge_view.h`, recent `v6` commits):
if v6 has replaced the encoding or the observation layout, the stream closes
the items against v6 instead of changing the old one. The stream must bump
`kStateFeatureDim` / vocabulary constants consistently, keep the
`static_assert`s true, and write a short `docs/` note listing exactly what
changed for the trainer. Keep these commits separate from everything else.

## 5. Spawning and merging

**Before spawning a stream:**

```bash
git worktree add -b fix/<stream> .worktrees/<stream> fix/integration
```

**Spawn** a general-purpose subagent (no `isolation` option: the worktree
above is its isolation, and its path must be deterministic so a later
iteration can find it). Prompt template — fill every angle bracket:

```
You are stream <stream> in an orchestrated fix of an engine code review.
Repository: /home/pirateporkchop/code/riftbound-engine
Your worktree: /home/pirateporkchop/code/riftbound-engine/.worktrees/<stream>
Your branch: fix/<stream>   (based on fix/integration at <commit>)

Read first, in your worktree: plans/AGENT_BRIEF.md (rules you must follow),
CLAUDE.md, then <plan files for this stream>.

You own these files: <files from the stage table>.
You own these plan items: <ids>. <For card streams: "Your work list is your
section of plans/cards/INDEX.md; ticking rules: ...">

What earlier stages changed that you need to know: <one paragraph: decisions
recorded under plan items, e.g. the 7.1 mechanism, the on-board guard, the
helper changes; point at the commits>.

<Any stream-specific note from the stage table.>

Finish every item you own. Report in the format at the end of AGENT_BRIEF.md.
```

Give context the agent cannot get from the repo: what was decided, what
changed underneath it, what to skip. Do not paste plan text it can read.

**When a stream returns:**

1. Read its report. If it left owned items open without a real blocker, send
   it back (continue the same agent) with what is missing.
2. In the integration worktree: `git merge --no-ff fix/<stream>`.
   - Conflict: do not resolve another stream's code by guessing. Abort the
     merge, have the stream rebase onto `fix/integration` and re-test, retry.
3. Build and run the full suite on integration (under the lock).
   - Red: `git reset --hard ORIG_HEAD` (integration worktree only), send the
     failure output back to the stream, set it `returned`.
4. Review: spot-check the diff against the items it claims. Verify the count
   of ticked boxes matches the report. Check no file outside its ownership
   changed except in `shared:` commits.
5. Green and reviewed: mark `merged` in `STATUS.md`, copy "Needs another
   stream" and "New defects found" into the carry-over table, then remove the
   worktree and branch: `git worktree remove .worktrees/<stream>` and
   `git branch -d fix/<stream>`.

**Keeping up with master:** when `master` advances, `git merge master` in the
integration worktree, rebuild, re-test. If the other session's commits fixed
or changed something a plan item covers, note it under that item. Streams
still running rebase when they finish, as usual.

## 6. Gates

A stage's gate passes when all of these hold on `fix/integration`:

- every stream of the stage is `merged` (`io-cli` may slip to before the
  stage 3 gate);
- the full test suite passes, with no failure that was not in the stage 0
  baseline list;
- **smoke:** `riftbound_clone_equiv_test` passes, and a fixed-seed
  200-game random-vs-random batch (`./build/riftbound --deck1 … --deck2 …
  --games 200 --threads 6 --seed <fixed>`, using a deck that exists) finishes
  with no engine exception and no game ending without a winner or a stated
  reason. After 1.7 lands, an engine exception must surface as a failure,
  not a draw: confirm that once;
- every checkbox the stage's streams own is `[x]`;
- `STATUS.md` records the evidence (commit, test counts, smoke result).

**Then merge to master**, so finished work lands as it completes: in the main
checkout run `git merge --ff-only fix/integration`. This is safe to attempt:
git refuses without changing anything if the other session's uncommitted
files are in the way or `master` has moved. If it refuses, sync master into
integration if needed, log it, and carry on with the next stage; try again at
the next gate. Only the final gate requires it to succeed.

## 7. Final gate and cleanup

1. `grep -rnE '^\s*- \[ \]' plans/` in the integration worktree prints
   nothing. No `BLOCKED:` sub-bullet remains. The carry-over table is all
   `done`.
2. Full suite and smoke pass on the final integration commit.
3. `git diff master...fix/integration --stat` reviewed once end to end for
   anything outside the plans' scope.
4. Fast-forward `master` to `fix/integration` in the main checkout. If git
   refuses because the other session has uncommitted work in the way, stop
   and tell the user; do not stash or move their work.
5. Clean up, then verify:
   - `git worktree remove` every directory under `.worktrees/`, then
     `git worktree prune`, then delete the `.worktrees/` directory and the
     `.worktrees/` line in `.git/info/exclude`;
   - `git branch -d` every `fix/*` branch, including `fix/integration`;
   - `git worktree list` shows only the main checkout and
     `git branch --list 'fix/*'` prints nothing.
6. Report to the user: commits merged, items fixed vs closed as not-a-defect
   (with counts), baseline vs final test counts, the checkpoint-breaking
   changes from stage 5, and anything the main checkout's `build/` now needs
   (it was never rebuilt).

## 8. When to stop instead of pushing on

Stop the loop and tell the user, without marking anything done, when:

- the final fast-forward to `master` is refused (section 7 step 4);
- an item cannot be fixed without a rules or product decision the plans do
  not settle and that no reading of `rules/core-rules.md` resolves;
- a stream has failed the same item three times;
- the main checkout or `master` shows signs that this run and the other
  session are fighting over the same files.

Ordinary obstacles (a failed build, a merge conflict, a flaky test, an agent
that returned early) are not blockers: diagnose and continue.

## 9. Completion criteria

All of:

- [ ] No `- [ ]` remains in any file under `plans/` except this list (`plans/remaining/` holds no item).
- [ ] Every stream in `STATUS.md` is `merged`; every gate row has evidence.
- [ ] `master` contains the final integration commit.
- [ ] Full test suite and smoke pass on that commit.
- [ ] No `fix/*` branch and no worktree remains; `.worktrees/` is gone.
- [ ] The user has the final report (section 7 step 6).

Tick these last, on `master`, as the final commit of the run.

---

# Reference: the review

Whole-engine review at commit `b05e51d` (master): integrity, consistency, bugs,
coding defects, performance. 514 open checkboxes across the plan files at the
start of the fix run.

## Background: how the review was done

Seven read-only reviewers, one per area, each reading its files in full:

| Area | Files |
|---|---|
| Engine, first half | `src/engine/game_engine.h`, `game_engine.cpp` 1-3300 |
| Engine, second half | `game_engine.cpp` 3300-6657 |
| Executor / chain / triggers | `effect_executor`, `chain_manager`, `trigger_manager`, `step_driver`, `game_runner`, `batch_runner`, `effects/effect_types.h` |
| Core and card base | `src/core/`, `rules/deck_validator`, `cards/card.{h,cpp}`, `card_helpers.h`, `card_registry`, top-level `CMakeLists.txt` |
| OpenSpiel / ML / agents | `src/openspiel/`, `src/ml/`, `src/agents/` |
| IO and CLIs | `src/io/`, `src/main.cpp`, `openspiel/openspiel_match.cpp`, `docs/play-api.md` |
| Card files | pattern audit over the 955 files in `src/cards/<type>/` |

**Nothing was built or run.** Every finding comes from reading code. Line
numbers are as of `b05e51d` and will drift.

## Confidence markers

- **[verified]** — confirmed by a reviewer and re-checked against the code a second time.
- **[confirmed]** — confirmed by the reviewer reading the code path end to end.
- **[plausible]** — mechanism read, but a concrete trigger was not found or not traced.

## Plan files

1. [01-openspiel-search.md](completed/01-openspiel-search.md) — search/training correctness. **Fix first**: these corrupt MCTS and training output regardless of rules correctness.
2. [02-engine-rules.md](completed/02-engine-rules.md) — rules bugs in the engine, executor, chain and trigger managers.
3. [03-cards.md](completed/03-cards.md) — card-file bugs and the choice-helper ordering bug.
4. [04-play-server-cli.md](completed/04-play-server-cli.md) — play server, HTML output, CLIs.
5. [05-integrity.md](completed/05-integrity.md) — consistency, type safety, build, dead code, deck validation.
6. [06-performance.md](completed/06-performance.md) — hot-path costs (none measured).
7. [07-cards-second-pass.md](completed/07-cards-second-pass.md) — second pass over the card files that closed the first pass's coverage gaps: verified list of target-less triggers, complete late-cost list, hand-rolled resume cards, benchmarks, deleted decks.
8. [08-cards-full-review.md](completed/08-cards-full-review.md) — full review of all 955 card files against their text: 335 findings (114 high), root causes first, per-card lists in [cards/batch-00.md](completed/cards/batch-00.md) … [cards/batch-10.md](completed/cards/batch-10.md).

9. [09-found-during-fix-run.md](completed/09-found-during-fix-run.md) — defects that were in no plan at the start: found by streams during the fix run, recorded as checkboxes and required like the rest. Appended to by the orchestrator at each merge.

Run files:

- [AGENT_BRIEF.md](AGENT_BRIEF.md) — rules and context every stream agent reads first.
- [STATUS.md](STATUS.md) — stream states, gates, carry-over and log; the orchestrator's resume point.
- [cards/INDEX.md](completed/cards/INDEX.md) — per card stream, every card file with a finding and where each finding is.
- [reports/](reports/) — each stream's final report, copied in at merge time so it survives the cleanup of `.worktrees/`.

**The plans are older than the code.** They were written against `b05e51d`.
Since then `master` has gained `12c5064` (spectator stepping in the play
server, which touches the code 04 describes) and `581ef91` (the 38 out-of-date
decks removed). By the time the fix run was planned `master` was already at `895abfe` (two v6
knowledge-tracker commits, which among other things add
`src/engine/knowledge_tracker.*` that 05 item 5.14 says does not exist), and
another session had uncommitted changes in `src/io`, `src/main.cpp` and
`docs/play-api.md` (now committed as `12c5064`). Line numbers are approximate everywhere; every item is
re-verified against current code before it is changed.

## What the review did not cover

Every source file in `src/` and every one of the 955 card files has now been
read in full. What remains open:

- Nothing was built or run. Items marked [plausible] have not been reproduced,
  and [confirmed] means traced by reading, not demonstrated by a test.
- `cards_init.cpp` was checked by script only (clean).
- Card def data that cannot be checked from the repo (art, artist, collector
  numbers, rarity) was not reviewed; costs, might, domains, tags and keywords
  were checked only against each card's own text, not against an external
  card database.
- Tests under `tests/` were not reviewed as code, only consulted. Known gap:
  the card test fixture feeds targets straight to `onTrigger`, a path the
  engine never produces (07 item 7.3), so passing card tests do not show a
  card works in a real game.
- Whether any synchronous subscriber to `awakenPhase` / `doExpirationBody`
  events can insert into `state_.objects` mid-iteration.
- Whether any `applyPassiveAura` override creates objects during the Step 1b
  iteration in `recalculateAuras`.
