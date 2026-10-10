# Fix run status

Maintained by the orchestrator. The authoritative copy is the one on the
`fix/integration` branch (`.worktrees/integration/plans/STATUS.md`); the copy
in the main checkout lags until integration is merged to master.

States: `pending` → `running` → `returned` → `merged` (or `blocked`).

## Run

| Field | Value |
|---|---|
| Current stage | 1 |
| Integration branch | `fix/integration` (`.worktrees/integration`) |
| Baseline commit (master at stage 0) | `c7143f1` |
| Baseline tests | 1511 run: 1508 passed, 2 failed, 1 skipped (1 disabled). Failing at baseline: `CardTestFixture.BaronNashor_IntegrationFullPlayPathCreatesPit`, `ChainTestFixture.FullGameRunsWithSpellsInDecks` (both: deleted deck file, item 7.40) |
| Last master sync into integration | `c7143f1` (branch point) |
| Last integration → master merge | — |
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

## Streams

| Stage | Stream | State | Branch head | Tests at merge | Notes |
|---|---|---|---|---|---|
| 1 | search | running | | | |
| 1 | helpers | running | | | |
| 1 | executor | merged | `ac49a38` (merge `e1eb9b8`) | 1560 run, 1557 passed, 2 baseline failures | all 8 items closed; 2 existing tests corrected (CR 427.1.b.1; temp Deflect) |
| 1 | chain-triggers | running | | | stopped by mistake and resumed by the user, same agent and worktree |
| 1 | engine-auras-costs | running | | | |
| 1 | engine-play-combat | merged | `3df9c9b` (merge `ec13e47`) | 1537 run, 1534 passed, 2 baseline failures | 16 of 17 items closed; 8.7 left open, moved to carry-over (card work) |
| 1 | io-cli | running | | | gate open at spawn (main checkout clean); based on `bbc995d` |
| 1 | integrity | running | | | based on `ec13e47`; also carries two build tasks: ccache across worktrees, unity-built tests |
| 2 | cards-c0 | pending | | | |
| 2 | cards-c1 | pending | | | |
| 2 | cards-c2 | pending | | | |
| 2 | cards-c3 | pending | | | |
| 2 | cards-c4 | pending | | | |
| 2 | cards-c5 | pending | | | |
| 2 | cards-c6 | pending | | | |
| 2 | cards-c7 | pending | | | |
| 3 | sweep | pending | | | |
| 3 | perf-engine | pending | | | |
| 3 | perf-search | pending | | | |
| 3 | cleanup | pending | | | |
| 4 | strong-ids | pending | | | |
| 5 | checkpoint | pending | | | |

## Gates

| Gate | Passed | Evidence |
|---|---|---|
| Stage 0 baseline | yes | `c7143f1`; tests 1508/1511 (2 baseline failures above); smoke: 200 games fiora vs yi_rebuild, `--seed 20261010 --threads 6`, exit 0, P1 128 / P2 72 / 0 draws, no exception; `riftbound_clone_equiv_test` FAILS at baseline (hard-coded `decks/leblanc_test.txt`, item 7.40) and must pass at the stage 1 gate |
| Stage 1 | | |
| Stage 2 | | |
| Stage 3 | | |
| Stage 4 | | |
| Stage 5 | | |
| Final | | |

## Carry-over

Items a stream reported under "Needs another stream" or "New defects found",
and who picked them up. Every row must end `done` before the final gate.

| From | Item | Assigned to | State |
|---|---|---|---|
| executor | 2.15 card recipes: Abandon 693, Scryer's Bloom 698, Stacked Deck 183, Baited Hook 242, Last Rites 471 recycle via `deck::putOnBottom` without announcing `"recycled_main"` | cards-c3 (693, 698), cards-c2 (183), cards-c0 (242), cards-c1 (471) | open |
| executor | 2.14: cards printing "if it was buffed this way" must branch on `buffUnit`'s returned bool | card streams (each in its range) | open |
| executor | 8.13: manual erase before `playIgnoringCost` / `playPayingCost` is redundant, drop it | card streams (each in its range) | open |
| executor | 8.14: gear attached to an off-board unit (`attachFree`, hand-rolled attaches) not covered by the executor guard | sweep | open |
| executor | NEW: Aspiring Engineer 384 "return a gear from your trash" coded as `bounceToHand` on a board gear | cards-c5 | open |
| executor | NEW: Blind Fury 25 and Void Rush 505 edit `main_deck` directly, bypassing `deck_ops` and the knowledge tracker | cards-c2 (25), cards-c3 (505) | open |
| executor | NEW: `recycle_self` cost and the mulligan recycle in `game_engine.cpp` do not announce `"recycled_main"` | sweep | open |
| executor | NEW: `EffectExecutor::moveToBase` is used for recalls too and now counts a move; CR 458 says a recall is not a move — needs a recall primitive and a caller audit | sweep | open |
| executor | temp Deflect lives in turn-stamped `card_counters`; a `temp_deflect_value` field expired in `expirationStep` would be cleaner | sweep (optional) | open |
| engine-play-combat | 8.7: engine half NOT A DEFECT; cards still to fix per the recipe under the item — Hand of Noxus 296, Vanguard Captain 218, Sun Disc 21, Scorchclaw 578, Undying Legion 587 (needs play-unit-from-trash) | cards-c0 (21), cards-c4 (218), cards-c5 (296), cards-c6 (578, 587); sweep ticks 8.7 | open |
| engine-play-combat | 8.6 card recipes: Bellows Breath 402, Piercing Light 346, Deathgrip 484, Smoke and Mirrors 645 need `enumerateLegalTargetSets` | cards-c2 (346), cards-c3 (402, 484, 645) | open |
| engine-play-combat | 2.27 card recipes: Baited Hook 242 and Poro Snax 369 misuse `recycle_self` | cards-c0 (242, 369) | open |
| engine-play-combat | `generateTrashReplayActions` duplicates the two legality gates of `canAfford`; discounts (CR 356.4) not applied to a grant cost | sweep | open |
| engine-play-combat | delete `requiresLegion` / `requiresLevel` / `levelThreshold` once the 8.7 cards self-gate | sweep | open |
| engine-play-combat | NEW: `chain_manager.cpp` `stepExecuteAndPass` PlayReaction branch skips `Card::onPlay`, optional additional cost, per-turn gear/equipment counters and the `"played_not_from_hand"` event | sweep | open |
| engine-play-combat | NEW: `generateShowdownActions` offers only `is_action` abilities — `[Reaction]` abilities and aura-granted abilities are not offered in showdowns / Closed States (CR 813.1.b) | sweep | open |
| engine-play-combat | NEW: `generateMainPhaseActions` reads play-to-location permissions by `ability_text.find("play me to ...")` | sweep | open |
| engine-play-combat | NEW: a `[Hidden]` champion cannot be hidden from the Champion Zone (CR 811.1.b); hidden cards not offered in a showdown Open State | sweep | open |
| engine-play-combat | NEW: `Keyword::Reaction` set on ~25 permanents that only have a `[Reaction]` ability (extends 8.8); `extraAnyPowerCost` reads it, so Mystic Vortex taxes playing them | sweep (with 8.8) | open |
| engine-play-combat | NEW: `combatResolutionStep` does not implement CR 468.3.d.1 (re-stage showdown and combat on No Result with both players present) | sweep | open |
| engine-play-combat | NEW: `Card::hasLegalTargets` default passes a fixed-count spell with fewer legal targets than its count; generator then emits zero intents | sweep | open |

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
