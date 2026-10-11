# sweep-flow — final report

Branch `fix/sweep-flow`, head `1dad1be` (2 commits on `0ca1b19`), worktree clean.

## Result

All items of the stream are fixed with tests except one: **Svellsongur 382 is NOT done** (see "Open").

Verification, clean build under the PCH-safe launcher (`ccache_pch_safe.sh`, switched as instructed; `clean` + full rebuild at `-j3` on lane B):

- `riftbound_tests`: 2245 run, 2245 passed
- `riftbound_openspiel_tests`: 17/17
- clone-equivalence: 10/10
- smoke (200 games, fiora vs yi_rebuild, seed 20261010): rc=0, no exceptions

New test file: `tests/test_review_fix_sweep_flow.cpp` (42 tests, `SweepFlowTest.*`).

## What was done

1. **05 item 5.4 — no `ability_text` matching in flow functions.** Play locations come from `Card::extraPlayLocations()`; battlefield restrictions from `BattlefieldCard::blocksUnitPlay()` / `blocksMoveToBase()`. The old text match had also given Blitzcrank 67, Mischievous Marai 562 and Shadow 752 a play-anywhere permission they do not print; that is gone. Only `parseRepeatCost` (sweep-costs) still reads text.
2. **Target legality on printed values.** Generators call `Card::hasLegalTargets(state, controller, card_db)`. Defy 45 and Rebuttal 939 are no longer offered against a spell they cannot counter. A fixed-count multi-target spell needs that many legal targets.
3. **Target roles.** `Card::chosenTarget(ctx, targets, role)`: a second target is not promoted when the first is gone (Piercing Light 346, Deathgrip 484, Void Assault 758, Smoke and Mirrors 645).
4. **Hidden plays and showdowns.** Showdowns offer Reaction abilities, aura-granted abilities and hidden cards; a hidden champion can be hidden from the Champion Zone; hidden-spell targets are restricted slot by slot (`Card::hiddenTargetFreeOfBattlefield`, CR 811.1.d.4) — Smoke and Mirrors is now playable from facedown.
5. **Settling before the main-phase decision.** `advanceMainPhase` never asks the turn player with a chain pending; a trigger queued while equipping resolves.
6. **Caps.** Finalize-pass and resume bounds no longer end a resolution quietly: they throw `std::logic_error` naming the card (resume bound raised to 1024).
7. **Moves.** Multi-unit Standard Move (CR 144.3) via follow-up choices in `executeStandardMove`; `BattlefieldState::multi_move_tax` (Mageseeker Investigator 725); `GameObject::cant_move_this_turn` (Vex 712); `GameEngine::unitPlayableAt` with `kPlayToLoneEnemyBattlefield` and `PlayerState::grant_units_lone_enemy_bf` (Arachnoid Horror 679, both sentences).
8. **Combat and scoring.** `combatResolutionStep` resolves the chain where CR 468.2 / 468.4 say and stages another combat on No Result with both players present (468.3.d.1). `BattlefieldState::conquered_from` + `ScoreEvent` previous controller (Yone 544). All cards read `GameState::effectiveVictoryScore()` (Leona 79, Find Your Center 47, Chem-Baroness 556, Poppy 678, Corrupted Dragon 878).
9. **Smaller rows.** Wuju Master 784 Level 11; Prodigal Explorer 514 gate is a count (`enemy_choices_this_turn >= 2`, all choice sites go through `GameState::noteChosen`); Might-dependent auras get a second hook after the fold (`hasMightDependentAura` / `applyMightDependentAura`: Fiora Victorious 232, Alpha Wildclaw 619, Vilemaw 622); `Card::requiresLegion` / `requiresLevel` / `levelThreshold` deleted (08 item 8.7); `GameEngine::recomputePendingLegalActions` for god-mode edits.
   - **NOT A DEFECT: Crowd Favorite 664** activating while already buffed — legal under CR 427.1.c; pinned by `SweepFlowTest.CrowdFavorite_MayBeActivatedWhileAlreadyBuffed`, clarified at the code site.

Plans updated: `05-integrity.md` (5.4), `02-engine-rules.md` (2.10), `08-cards-full-review.md` (8.7), `cards/batch-00/01/03/04/06/07.md`. `CLAUDE.md` documents the new mechanisms.

## Open

- **Svellsongur 382 — still BLOCKED, not closed.** "The bearer has each of its abilities twice" needs (a) `TriggerManager` to queue the bearer's own triggers once more per text-copying Equipment (sweep-triggers' file) and (b) a per-instance aura hook, since `applyPassiveAura` runs once per (card, controller). Proposed shape is written in `plans/cards/batch-00.md` under the item (`Card::copiesBearerText()`, `GameObject::text_copies`, `forEachSelfOnce` and `fireTrigger` repeat `1 + text_copies` times). Needs sweep-triggers or sweep-verify.

## For the merge

- This stream edited two of sweep-triggers' files: `src/engine/chain_manager.cpp` (both target-recording loops now call `noteChosen`; resume bound throws) and `src/engine/effect_executor.cpp` (`cant_move_this_turn` check in `moveToBase` / `moveToBattlefield`). Expect conflicts there, and in `CLAUDE.md` (the `killUnit` lesson line, which integration also rewrote).
- Existing tests corrected: `ChainManagerFixTest.SafetyCap_ResumeOverflowDoesNotLeakAChoice` renamed `SafetyCap_ACardThatNeverStopsAskingIsAnError` (now expects the throw); `ReviewFixC1Test.ProdigalExplorer_IsAReactionWithAGate` (gate is two choices); `C6Test.Yone_ConqueringAnEnemyBattlefieldDoesNothing` (passes the previous controller to `testHook_scoreConquer`); `ChainTestFixture.LegionParsesAsConditionGate` (no longer reads the deleted virtual).
- Trainer note: a Standard Move with other eligible units now asks extra `MakeChoice` prompts (first option "Move no more units"); move intents are still one per (unit, destination). No feature-vector or vocab change. New state fields (`cant_move_this_turn`, `enemy_choices_this_turn`, `grant_units_lone_enemy_bf`, `multi_move_tax`, `conquered_from`) are not in `feature_extractor`.
- The process hosting this stream was killed once mid-run; nothing was lost, and every result above is from the rebuild after the resume.
