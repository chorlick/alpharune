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

## Streams

| Stage | Stream | State | Branch head | Tests at merge | Notes |
|---|---|---|---|---|---|
| 1 | search | returned | `44a7541` (merged in batch, pending verification) | 1659 passed, 0 failed (on its branch) | all 16 items closed; both baseline failures fixed; clone-equiv 10/10; new binary `riftbound_openspiel_tests` |
| 1 | helpers | returned | `2e0f181` (merged in batch, pending verification) | 1649 passed, 2 baseline failures (on its branch) | 4 items closed; Thrill of the Hunt 745 and Zenith Blade 258 fixed with red-then-green tests; 3.1, 3.24, 7.30 open for card streams |
| 1 | executor | merged | `ac49a38` (merge `e1eb9b8`) | 1560 run, 1557 passed, 2 baseline failures | all 8 items closed; 2 existing tests corrected (CR 427.1.b.1; temp Deflect) |
| 1 | chain-triggers | merged | `6d4dd76` (merge `c9ad6b4`) | 1583 run, 1580 passed, 2 baseline failures | 12 items closed; 7.1 decision recorded (cards use `pickTarget` + `kFinalizeTrigger`); 8.9 has one bullet left, in carry-over |
| 1 | engine-auras-costs | merged | `3ba5256` (merge `a8af2ef`) | 1631 run, 1628 passed, 2 baseline failures | 10 items closed; 5.4 and 8.4 partly done, remainder in carry-over; edited 25 card files (listed in carry-over) |
| 1 | engine-play-combat | merged | `3df9c9b` (merge `ec13e47`) | 1537 run, 1534 passed, 2 baseline failures | 16 of 17 items closed; 8.7 left open, moved to carry-over (card work) |
| 1 | io-cli | returned | `ae7961b` (merged in batch, pending verification) | 1655 passed, 2 baseline failures (on its branch) | all 21 items of 04 ticked; sent back for: decision-id `handleChoose` + seat (4.3/1.10), close the trace-log hidden-card leak (4.5), confirm batch exit on engine exception, CLAUDE.md / `scripts/generate_replays.py` references to the deleted OpenSpiel CLI |
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
| io-cli | NEW: `LogEvent` (`src/core/events.h`) has no player / visibility tag — finer fix for the 4.5 trace leak than withholding the stream | sweep | open |
| io-cli | NEW: god-mode edits do not recompute the pending decision's legal actions (stale intent can be applied); documented, not fixed | sweep | open |
| io-cli | NEW: spectator mode first Ctrl-C runs the AI game to completion before exit | sweep (low) | open |
| io-cli | optional: move `play_server.cpp` / `play_index_html.cpp` into `riftbound_core` so tests need not `#include` the .cpp files | sweep (optional) | open |
| io-cli | deleted `src/openspiel/openspiel_match.cpp` (in no target) and `src/io/progress_bar.h`; 4.16, 4.17, 4.20 closed as obsolete — check nothing else referenced them | orchestrator (stage 1 gate) | open |
| io-cli | not verified in a browser: page JS for `decision_id`, token query on the WebSocket, per-unit owner/controller on moves, hidden-zone counts; no TSAN run | final report | open |
| search | GATE: also build and run `riftbound_openspiel_tests` (`build/src/openspiel/riftbound_openspiel_tests`, `RIFTBOUND_ROOT=.`) — not part of `riftbound_tests`. 1.7 check: `RIFTBOUND_STEP_FAULT_AT=40 ./build/riftbound --deck1 decks/yi_rebuild.txt --deck2 decks/fiora.txt --agent1 mcts:sims=8 --agent2 random --games 4 --seed 7` must abort with the injected-fault message (exit 134), and exit 0 without the variable | orchestrator (every gate) | open |
| search | `game_runner` / `batch_runner` do not catch exceptions: any engine or agent exception aborts the whole batch; should become a per-game error count | io-cli (owns the runners) | open |
| search | HumanAgent interface for io-cli (4.3): `bool setChosenAction(int legal_index, uint64_t decision_id)` refuses a stale decision id; `explicit HumanAgent(PlayerId seat)`; server should send the rendered decision id, `main.cpp` should pass the seat | io-cli | open |
| search | 7.40 remainder: `openspiel_match.cpp` deck defaults, root `README.md` | io-cli | open |
| search | `CLAUDE.md` still describes the old replay model; add history tokens and `riftbound_openspiel_tests` | sweep | open |
| search | 6.7: `~StepDriver` still drains by playing the game out; a clone at a non-resumable decision with no resumable ancestor replays from game start (~1.5 ms vs ~0.11 ms) | perf-search | open |
| search | behaviour changes for other streams: `GameEngine::applyChoice` throws `std::out_of_range` on a bad index; `RiftboundState::ApplyAction` throws `std::invalid_argument` on an illegal action id (both used to apply action 0); `actionHistory()` returns by value; `makeFromSnapshot` gained `rng_seed`; `InfoSetId` / `ObservationString` values changed (1.13), feature layout unchanged | trainer note in the final report | open |
| search | NEW: `mainPhaseLoop` can reach its decision with an item on the chain and state Closed; `generateLegalActions` then builds the stale `priority_holder`'s options (seen as a lone "P1: PassPriority") while the engine asks the turn player. Repro: yi_rebuild vs fiora, seed 1, move 217 | sweep | open |
| search | NEW: `Intent::operator==` ignores `use_alt_play_cost` and `granted_ability_def` | sweep | open |
| search | NEW (by design, note only): MCTS agents fall back to a uniformly random legal action at non-main-phase decisions when the live game cannot be reproduced from history (wrong seed, god-mode edit) | final report | open |
| search | 1.3: `ResumedEngineKeepsTheMainPhaseActionCount` not confirmed to fail without the fix | sweep (confirm) | open |
| engine-auras-costs | 5.4 remainder: three text matches left in the engine, each needs a `Card` virtual plus card edits — `setupBattlefields` ("can't move from here to base" / "can't be played here"), `generateMainPhaseActions` ("play me to an open battlefield" and variants), `parseRepeatCost` | sweep (ticks 5.4) | open |
| engine-auras-costs | 8.4 remainder: Weaponmaster for Sand Soldier tokens — keyword exists only as a card base class and tokens have no card class; needs a keyword-level "when you play me" handler with a real choice and payment | sweep (ticks 8.4) | open |
| engine-auras-costs | card files it already edited — card streams must re-read, not overwrite: battlefields 762, 775; legends 551, 784; units 15, 19, 28, 79, 84, 140, 232, 236, 243, 299, 349, 388, 411, 452, 467, 547, 609, 639, 675, 680, 709. Batch findings resolved by it: Farron (b04), Warmonger (b05), Rumble Hotheaded (b09), Eager Apprentice and Herald of Scales (b04), Vex (b05), Vaults of Helia (b00), Ornn's Forge 527 (engine). Fiora Victorious 232 def change NOT checked against her batch-05 entry | card streams (each in its range) | open |
| engine-auras-costs | 2.2 contract: `applyPassiveAura` is called once per (card, controller); 08 S3 unit and gear entries and the Heimerdinger duplicate-refs entry are fixed by the engine — verify and close | card streams / sweep | open |
| engine-auras-costs | battlefield hooks still run once per slot: same battlefield brought by both players doubles — War Camp, Void Gate, Kinkou Temple, Forbidding Waste, Black Flame Altar, Gardens of Becoming, Risen Altar need card-side guards (Brush has one; Marai Spire relies on per-slot) | cards-c0 | open |
| engine-auras-costs | `EffectExecutor::payAnyPower` duplicates the rule in `payAdditionalCost` | sweep | open |
| engine-auras-costs | NEW: `killUnit` `death_replacement_recall_pending` block (Tactical Retreat) calls `detachAllGear` on a recall; CR 719.3.a says a recalled unit keeps its gear | sweep | open |
| engine-auras-costs | NEW: Shurelya's Requiem 509 hook grants Ganking at its battlefield but the def text has no such sentence | cards-c1 | open |
| engine-auras-costs | NEW: `TriggerManager::onCardPlayed` — Vision fires once however many instances (CR 817.2: each triggers separately); also runs a full aura pass per permanent played (perf, plan 06) | sweep / perf-engine | open |
| engine-auras-costs | NEW: Fiora Victorious 232 Mighty check reads `current_might` incl. previous pass's auras (pass-order dependent) | cards-c5 | open |
| engine-auras-costs | NEW: `CLAUDE.md` stale on three points (killUnit replacements described as text matching; power recycled from exhausted runes only; no mention of the once-per-(card, controller) aura contract); `game_engine.h` comment "cleanup has no agent-query sites" is wrong | sweep | open |
| engine-auras-costs | unplanned behaviour change to review: "to a minimum of [N]" no longer raises a card already cheaper than N | sweep (confirm against CR) | open |
| helpers | 3.1 card work: Blast Cone 695 (one `confirmOptional` per callback → own trigger or two-mode `pickMode`), Forgotten Signpost 607 (two `pickTarget` → `pickTargetPair`), Zenith Blade 258 stun, Thrill of the Hunt 745; general rule: no state change between two helper calls unless guarded to run once | cards-c1 (607, 695), cards-c2 (258), cards-c3 (745); sweep ticks 3.1 | open |
| helpers | 3.24 card work: 751, 41 → `splitDamageAmong`; 444 → shared `discardThenAct`; 405, 412, 498, 748, 263 → `payOnePower` / `payAnyPower` / `recycleRuneForPower` | card streams (each in its range); sweep ticks 3.24 | open |
| helpers | 7.30 card work: 927, 894, 937, 892, 935 → `offerableChoices` + `commitChosenObject`, or `pickTarget` | card streams (each in its range); sweep ticks 7.30 | open |
| helpers | 7.1 probe: 26 cards never ask the controller on the real path — units 16, 51, 76, 82, 92, 97, 130, 132, 188, 191, 199, 234, 238, 330, 361, 384, 453, 479, 539, 589, 618, 738; battlefields 278, 535, 763; gear 63. Enable each row in `Plan07Item71/DISABLED_TargetedTriggerTest` (`tests/cards/test_choice_helpers.cpp`) when fixed. Blast Cone 695 defect is 7.2; Forge of the Future 212 is not in the probe | card streams (each in its range) | open |
| helpers | Weaponmaster 7.7: Equip cost still not charged — needs an equip-cost descriptor on `Card` (a gear's cost lives inside its `onEquip`); 7.23 / 7.26 base fixed, Yone 544 and Sentinel Adept 331 carry their own copies | sweep (design), cards-c5 (331), cards-c6 (544) | open |
| helpers | NEW: file-local test helpers call `onTrigger` directly (`WeaponmasterTest::fireWeaponmasterTrigger`, `invokeOnTrigger` in `test_jhin_deck.cpp`), bypassing finalization | sweep | open |
| helpers | NEW: `payEnergy` / `payableEnergy` in `card_helpers.h` count and exhaust any rune with a location, not only runes in the player's base, in map order | sweep | open |
| helpers | NEW: Keeper's Verdict 760 asks the spell's controller instead of the unit's owner | cards-c3 | open |
| helpers | NEW: Altar of Memories 490 `__altar_drew` guard (3.7) still breaks every death after the first | cards-c1 | open |
| chain-triggers | 7.1: 26 cards to convert per the rule under the item; remove each id from `kKnownOffenders` in `tests/test_review_fix_chain_triggers.cpp` and enable its row in `Plan07Item71/DISABLED_TargetedTriggerTest` | card streams (each in its range); sweep ticks 7.1 | open |
| chain-triggers | 8.9 last bullet: Square Up ("Discard 1") and Curtain Call (three costs) need a card-declared Repeat cost in `card.h` read by `executePlaySpell`, plus the two card files | sweep | open |
| chain-triggers | Irelia 380 and Jae Medarda 463 need `triggerCondition(subject == source)` | cards-c5 | open |
| chain-triggers | Treasure Trove 186 triggers only on a kill, not on a bounce | cards-c0 | open |
| chain-triggers | 8.16 bullet "spell that banishes itself is also filed in trash" is fixed in `chain_manager.cpp`; note it when ticking 8.16 | sweep | open |
| chain-triggers | 2.11 root cause: the play gate should check card cost plus Deflect together | sweep | open |
| chain-triggers | NEW: `ChainManager::addSpell` / `addAbility` charge Deflect without consulting `Card::ignoresDeflect()` | sweep | open |
| chain-triggers | NEW: `TriggerManager::onUnitDied` queues a unit's own death ability directly, bypassing `triggerCondition` and the LeBlanc Temporary suppression | sweep | open |
| chain-triggers | NEW: `GameEngine::runFinalizePass` caps at 16 choices and stops silently | sweep | open |
| chain-triggers | NEW: Battle Mistress 517 triggers once per dead enemy unit instead of once per "one or more" | cards-c6 | open |
| chain-triggers | NEW: Void Burrower 553 declares `getTargetRequirements()` that nothing reads (allow-listed in the audit test) | cards-c1 | open |
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
- 2026-10-10 merged chain-triggers (`c9ad6b4`), suite 1580/1583 with only the 2 baseline failures; 12 boxes ticked and verified; 11 rows to carry-over. Worktree and branch removed.
- 2026-10-10 helpers returned; sent back for two regressions before merge.
- 2026-10-10 recorded carry-over from the engine-auras-costs and helpers reports ahead of their merge verification (12 and 9 rows).
- 2026-10-10 merged engine-auras-costs (`a8af2ef`), suite 1628/1631 with only the 2 baseline failures; 10 boxes ticked and verified. Worktree and branch removed.
- 2026-10-10 helpers (`2e0f181`) and search (`44a7541`) merged as one batch; verification build queued. 11 search rows to carry-over.
- 2026-10-10 io-cli (`ae7961b`) merged into the same batch; io-cli sent back for four follow-ups. 6 rows to carry-over.
