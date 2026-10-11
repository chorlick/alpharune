# Carry-over audit (stream sweep-verify-ledger, 2026-10-11)

An audit of every row of the Carry-over table in `plans/STATUS.md` that was not
plainly `done`, against the code and tests on `fix/integration` at `502288c`
(all three engine sweeps merged; suite 2488 tests).

**How to read it.** One entry per row, in table order. `L<n>` is the row's line
number in `plans/STATUS.md` at `502288c`; the quoted words are the start of the
row's Item column. Verdicts:

- **DONE** — fixed; the entry names the file and function and the test. The
  row can be set to `done`.
- **DUPLICATE** — the row restates a plan checkbox; the entry names it and its
  state. The row can be set to `done (tracked by <box>)`.
- **OPEN** — real remaining work that is in no checkbox; a `9.K<n>` line for
  it is in "New checkboxes" at the end.
- **NOTE** — information for the final report or for trainer developers, no
  code work. Trainer notes are collected in `docs/trainer-notes-2026-10.md`.
- **USER** — needs a decision from the user; the question is under "For the
  user".
- **MIXED** — the sub-items differ; each has its own verdict.

**How it was made.** Three read-only helpers took disjoint slices of the table
(L295–335, L336–385, L386–471) and wrote their entries from the code; the lead
checked a sample of each slice against the code, wrote the entries for the rows
it handles itself (L412, L418, L429, L469 and the parts of L301, L349 that are
this stream's own items) and merged duplicate findings. **The helpers built and
ran nothing**: a test named in an entry was found and read, not executed —
except where an entry says "run". Paths are relative to the repository root.
Line numbers in code are as of `502288c` and will drift.

**Counts** are at the end, before "New checkboxes".

---

### L295 — trainer survey — "notes for the final report: trainer `terminationKind` has"
**Verdict: NOTE (final report + trainer developers)**
- `terminationKind` has no `"engine_error"` case: NOTE (trainer) — engine side verified: `GameEngine::refreshStepFromDriver` sets `step_result_.termination_reason = "engine_error"` when the fiber ended by exception (`src/engine/game_engine.cpp:171-176`), documented at `applyChoice` (`src/engine/game_engine.h:170-175`). The trainer side (`src/match/match_runner.cpp:36-43`) is from `plans/reports/trainer-survey.md` item 7 and was not re-read (outside this tree). The survey itself leaves open whether `runGame` can return that reason or always throws.
- trainer compiles `scripted_agent.cpp` itself: NOTE (trainer) — engine side verified: `src/agents/scripted_agent.cpp` is in `riftbound_core` (`CMakeLists.txt:450`). Trainer `CMakeLists.txt:89-94` per survey item 3.
- `test_knowledge_gate` allow-list of files that may edit `main_deck`: NOTE (trainer) — engine side verified today: direct `main_deck` mutations outside `src/engine/deck_ops*` are only `src/engine/game_engine.cpp:436`, `src/io/state_editor.cpp:310`, `src/rules/deck_validator.cpp:322`; **no card file edits `main_deck` directly**, so the gate's allow-list still holds after stage 3 (the survey's line number for `game_engine.cpp` was 426, now 436 — the trainer test matches by file, per the survey).

### L296 — cleanup — "5.13 / 5.14 remainder, in files the sweep streams own"
**Verdict: DUPLICATE** — plans/05-integrity.md **5.13** (open) and **5.14** (open); each item below is a named sub-bullet there. All re-verified as still present:
- `swapBackBattlefield`, `generateCombatDamageActions`, `advanceCleanup`, `activePlayer`: no callers (`git grep` finds definition + declaration only: `game_engine.cpp:7061 / 4284 / 5655 / 7007`) — 5.13 open.
- `executeEndTurn` empty (`game_engine.cpp:2831`, called at `:1830`) — 5.13 open.
- `had_temp_might` set and `(void)`-ed (`game_engine.cpp:1229, 1262`) — 5.13 open.
- `contested_by` / `combat_staged` after combat resolution — 5.13 open (not re-traced; the sub-bullet is unchanged).
- header says "stubbed", "worker thread", "Cleanup has NO agent-query sites" (`game_engine.h:52, 55, 101, 107, 126, 493`; also `game_engine.cpp:149`) — 5.14 open.
- `CleanupNeededEvent` no emitter (`events.h:188`, `events.cpp:30` forwards only) — 5.13 open.
- `CostPaymentCursor` "Memcpy-clone safe (POD)" with `std::vector<Domain>` (`game_state.h:799-809`) — 5.14 open.
- `current_play_source` comment "Default Hand for resetTurnTracking" (`game_state.h:313-317`) — 5.14 open.
- `cards_init.cpp` line 1 "Auto-generated aggregator" — 5.14 open.

### L297 — cleanup — "warnings: 253 `warning:` lines in the clean gate build"
**Verdict: DUPLICATE** — plans/09 **9.C6** (open), **9.F2** (open), **9.F3** (open); 9.F4 ticked.
- tests / io / openspiel / agents / ml / core: done by cleanup follow-up (`plans/reports/cleanup-followup.md` §1: tests 41+1 → 0, the rest 0) — covered by 9.C6, which stays open until the whole tree is clean.
- card files: 0 (`plans/reports/sweep-verify-cards-2.md` line 11; 9.F4 ticked).
- `card.h` 18 unused-parameter warnings: 9.F3 open; default bodies still name their parameters (`src/cards/card.h:76-77, 88-89`).
- engine, 3 warnings: 9.F2 open; all three still in the code: `chain_manager.cpp:197` `finalized_something`, `game_engine.cpp:4734` `greedyCascade` (one occurrence = defined, never called), `trigger_manager.cpp:627` `bf_loc` in `onCombatStarted` (unused; the one at `:962` is used).
- UNDETERMINED: the current total. `sweep-verify-cards-3.md` line 12 reports 113 `warning:` lines in a three-target build "from targets not built before"; settling it needs one clean build log of all targets (9.C6).

### L298 — cleanup — "cleanup's resumed agent had lost its original brief"
**Verdict: NOTE (final report; orchestrator process, no code)**
- Six streams were resumed from transcripts (STATUS log 2026-10-11 00:52: sweep-costs, sweep-flow, sweep-triggers, cleanup, perf-search, sweep-verify-cards). The log records the check against the in-flight table for two: cleanup (log line "sent back for three brief items it had lost on resume") and sweep-triggers (log line "checked against the in-flight table — Astral Heron 831 missing ...", → plans/09 section D). No log line records the same check for sweep-flow, sweep-costs, perf-search or sweep-verify-cards. This ledger audit re-verifies their carry-over rows against code, which is the substance of the rule; the row can close once the orchestrator records that.

### L299 — sweep-flow — "partly closed by sweep-flow, remainder with other streams"
**Verdict: MIXED** — this row is an index of the BLOCKED rows; card-by-card evidence is under L312 (c7), L319 (c6), L328 (c5). Items only named here:
- cards-c1 514 Prodigal Explorer: DONE — `legends/0514_prodigal_explorer.cpp` `canActivateAbility` (`enemy_choices_this_turn >= 2`), counted in `GameState::noteChosen` (`src/core/game_state.h:864`); batch-01 514 ticked; test `SweepFlowTest.ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities` (engine path: friendly unit 0, enemy unit 1 not offered, enemy gear 2 offered).
- cards-c1 512 Emperor of the Sands: DONE — `src/engine/keyword_abilities.{h,cpp}` (Weaponmaster / Vision queued per instance); batch-01 512 ticked; tests `SweepTriggersTest.Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed`, `.Weaponmaster_IsAMay`, `SweepCostsTest.SandSoldier_HasWeaponmasterFromTheEmperor`.
- cards-c1 809 Endless Riches: DONE — `gear/0809_endless_riches.cpp` + `src/engine/trash_replacement.h` (`settleInTrash`); batch-01 809 ticked; tests `SweepCostsTest.EndlessRiches_CardsArePlayedFromTheTrashAtTheirOwnCost`, `.EndlessRiches_WhatWouldGoToTheTrashIsBanishedExceptFromTheDeck`.
- cards-c1 294 / 506 / 786: DONE — `RunePool::spell_power` / `gear_power` / `showdown_energy` (`src/core/game_state.h:45-50`), spent through `src/engine/resource_payment.h`; batch-01 line 164 ticked; tests `SweepCostsTest.RestrictedPower_SpellOnlyPaysForSpellsOnly`, `.RestrictedPower_GearOnlyPaysForGearAndGearAbilities`, `.RestrictedEnergy_ShowdownOnlyIsUnusableOutsideAShowdown`.
- cards-c1 259 Swift Scout: DONE — `Card::adjustHideCost`, `GameEngine::hideCostFor`; batch-08 259 ticked; test `SweepCostsTest.SwiftScout_HidesForOneEnergyInsteadOfThePower`.
- cards-c0 382 Svellsongur: DONE — see L300.
- cards-c0 290 Vilemaw's Lair: DONE — `EffectExecutor::moveToBase` refuses when `moveToBaseBlocked` (`effect_executor.cpp:877-`); batch-00 290 ticked; test `SweepTriggersTest.MoveToBase_AnEffectCannotMoveAUnitOutOfVilemawsLair`.
- cards-c0 272 Back-Alley Bar: DONE for the fix (`EffectExecutor::recallToBase` emits no `UnitMovedEvent`; batch-00 272 ticked; tests `SweepTriggersTest.Recall_DeathReplacementOnAnEffectKillKeepsGearAndIsNotAMove`, `.Recall_DeathReplacementOnLethalDamageKeepsGearAndIsNotAMove`). The missing positive control (a real move out of the Bar giving +1 through the engine) is DUPLICATE of plans/09 **9.I4** (open).
- cards-c0 equip descriptor: MIXED — code DONE (`Card::equipCost` / `canPayEquip` / `payEquipAndAttach`, `card.h`, `card.cpp`; generator gate tested by `SweepCostsTest.Equip_IsOfferedOnlyWhenItsCostCanBePaid`; 948 Piltovan Forge ticked with `SweepCostsTest.Equip_FirstGearAbilityDiscountAppliesToTheEquipCost`). But the batch-00 box **"Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)"** (plans/cards/batch-00.md:101) is still `[ ]` with a stale `BLOCKED` sub-bullet; sweep-costs' report does not list it among the boxes it ticked. DUPLICATE of that open box, **ready to tick**: all ten gears declare their cost (332, 339, 345, 353, 356, 374, 379, 387 through `SimpleEquipGear` / `UniversalEquipGear::equipCost` in `gear/equip_base.h:25, 41`; 365 and 382 with their own `equipCost`), `standardEquip` is gone (`git grep standardEquip -- src` → nothing) and no gear overrides `onEquip`.
- cards-c0 950 Risen Altar: fix in place (`Card::activationPowerReductionFor`, `GameEngine::netActivationCost`; batch-00 950 ticked; `SweepCostsTest.EmpowerDiscount_TakesAPowerWhereThereIsNoEnergy` is card-level). The missing real-activation test is DUPLICATE of plans/09 **9.J4** (open).
- Yone 544 previous controller: DONE — `BattlefieldState::conquered_from` (`game_state.h:481`), passed on the score event; batch-06 544 ticked; tests `C6Test.Yone_ConqueringAnUncontrolledBattlefieldHitsTheChosenBaseUnit`, `C6Test.Yone_ConqueringAnEnemyBattlefieldDoesNothing` (passes `P2` as previous controller).

### L300 — sweep-flow — "Svellsongur 382 not done: needs the trigger manager"
**Verdict: DONE**
- `Card::copiesBearerText()` (`card.h`), `EffectExecutor::textCopiesOn` (`effect_executor.cpp`), repeat in `TriggerManager::fireTrigger`, `recalculateAuras` step 1b' (`game_engine.cpp`); `gear/0382_svellsongur.cpp` has no gap comment. Boxes: batch-00 382 ticked (line 48), plans/09 9.C1 ticked. Tests (all present in `tests/test_review_fix_sweep_triggers.cpp:947-1037`): `SweepTriggersTest.Svellsongur_TheBearersTriggeredAbilityTriggersTwice`, `_TheBearersDeathknellTriggersTwice`, `_TheBearersPassiveAbilityAppliesTwice`, `_ThePrintedAssaultCountsAgain`, `_AnActivatedAbilityIsOfferedOnce`, `_CountsAnAdditionalTimeEffectAgain`.

### L301 — sweep-flow — "`shared:`-style changes to review: `GameState::noteChosen` at every"
**Verdict: MIXED (review)**
- `GameState::noteChosen` at every choice site: reviewed OK — definition `src/core/game_state.h:864-873` (counts an enemy unit or gear on the board, chosen by a spell or by an ability whose source is a unit; sets `chose_enemy_unit_this_turn` for any enemy unit). Callers: both play-time target loops in `chain_manager.cpp:75, 153`; the finalize-time target in `game_engine.cpp:1446`; `commitChoice` in `card.cpp:415` (used by `pickTarget`, `pickTargetPair`, `Card::commitChoice`); `splitDamage` in `card_helpers.h:727`. No other writer of either counter (`git grep chose_enemy_unit_this_turn` → only `game_state.h`). (a) no card names in the engine; (b) matches the printed gates (Prodigal Explorer 514 "chosen enemy units and/or gear twice this turn with spells or unit abilities"; "unit" = on the board, CR 141.1.a as cited at `card.cpp:411`); (c) `SweepFlowTest.ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities`.
- `cant_move_this_turn` also stops the controller's effect moves: OPEN 9.K13 — code is there (`EffectExecutor::moveToBase` `effect_executor.cpp:870-876`, `moveToBattlefield` `:1047-1053`; cleared in the Expiration Step `game_engine.cpp:1212`; Standard Move gate `standardMoveDestinations` `:3191`), no card logic in the engine, but **no test exercises the two executor refusals**: the only test, `SweepFlowTest.Vex_ThePlayedUnitCannotBeMovedThisTurn`, checks the Standard Move generator and the expiry. The batch-07 712 line claims the executor half under that test.
- multi-unit Standard Move asks follow-up `MakeChoice` prompts: reviewed OK with one finding — `GameEngine::executeStandardMove` (`game_engine.cpp:2701-2757`): first option is `MakeChoice{chosen_value = 0, choice_label = "Move no more units"}`, then one option per unit that can make the same Standard Move to the same destination; `multi_move_tax[player]` [A] is paid as each unit is added and a unit that cannot be paid for is not offered. (a) no card logic; (c) `SweepFlowTest.StandardMove_SeveralUnitsMoveAsOneAction`, `.StandardMove_DecliningToAddUnitsMovesOne`, `.MageseekerInvestigator_TaxesEachUnitBeyondTheFirst`. (b) CR 144.3 / 144.3.a / 144.3.b match. **Finding (CR 144.3.c "The Costs of Exhausting the Units are also paid Simultaneously"; 144.3 "one game action"):** the loop at `:2759-2780` exhausts, moves and emits `UnitMovedEvent` one unit at a time, so a `triggerCondition` evaluated on the first unit's event sees the others not yet exhausted and not yet arrived. OPEN 9.K1.
- resume / finalize bounds now THROW: reviewed OK — `ChainManager::runResumable` (`chain_manager.cpp:450-460`, `kMaxResumeIterations = 1024`) and `GameEngine::runFinalizePass` (`game_engine.cpp:1537-1548`, `kMaxChoices = 1024`) throw `std::logic_error` naming the card. No card logic. Tests `ChainManagerFixTest.SafetyCap_ACardThatNeverStopsAskingIsAnError`, `SweepFlowTest.Finalize_ACardThatNeverStopsAskingIsAnError`. "No card reaches the bound in normal play" is DUPLICATE of plans/09 **9.C5** (open); the helper caps sized to the old bound are **9.D3** / **9.A22** (open).
- 4 existing tests corrected (commit `c081449`, diff read):
  - `SafetyCap_ResumeOverflowDoesNotLeakAChoice` → `SafetyCap_ACardThatNeverStopsAskingIsAnError`: OK — follows the deliberate behaviour change, comment says why, and it now also runs in debug builds (the old one was skipped there). Not a weakening.
  - `ReviewFixC1Test.ProdigalExplorer_IsAReactionWithAGate`: OK — cites the printed "twice", adds a "once is not twice" assertion. Strengthened.
  - `C6Test.Yone_ConqueringAnEnemyBattlefieldDoesNothing`: OK — the fixture's `conquer()` gained a `from` argument and the test passes `P2`; assertions unchanged.
  - `ChainTestFixture.LegionParsesAsConditionGate` (`tests/test_chain.cpp:1045`): **weakened** — `EXPECT_TRUE(card->requiresLegion())` was removed (the virtual is gone, 8.7) and nothing replaced it; the test now only asserts `triggerType() == WhenYouPlayMe` under a name that claims a Legion gate. The behaviour is covered elsewhere (`C4Test.DangerousDuo_NeedsLegionAndThenChooses`), so this one should be deleted or made to assert the gate. OPEN 9.K14.
  - (also corrected in the same commit, not listed in the row: `Misc2Test.MageseekerInvestigator_FlagsItsBattlefield` — `surcharge_enemy_multi_move` → `multi_move_tax`; OK, comment cites who owes the tax, three assertions kept.)
- Blitzcrank 67, Mischievous Marai 562, Shadow 752 lost a play-anywhere permission: DUPLICATE — plans/09 **9.C4** (open at `502288c`; **ticked by this stream**, commit `8f1feb1`: `LedgerTest.Pin_9C4_BlitzcrankMaraiAndShadowArePlayedOnlyWhereAnyUnitIs`, `.Pin_9C4_EachIsStillPlayableToABattlefieldItsControllerControls`, comments at the three cards). What was found: none of the three prints a permission (67 "When you play me to a battlefield, you may move an enemy unit to here."; 562 "When you play me to a battlefield, deal 2 to an enemy unit here."; 752 "If you play me to a battlefield, I enter ready.") and none overrides `extraPlayLocations` (overriders are 161, 174, 176, 193, 415, 679, 902). Blitzcrank is pinned (`SweepFlowTest.PlayLocation_ATriggerThatMentionsPlayingToABattlefieldGrantsNothing`); 562 and 752 had no pin until that commit.

### L302 — sweep-flow — "new state fields not in the feature layout"
**Verdict: NOTE (trainer developers / stage 5 checkpoint input — the one deferral the brief allows)**
- Verified: `GameObject::cant_move_this_turn` (`game_object.h:268`), `PlayerState::enemy_choices_this_turn` (`game_state.h:326`), `PlayerState::grant_units_lone_enemy_bf` (`:178`), `BattlefieldState::multi_move_tax[2]` (`:489`), `BattlefieldState::conquered_from` (`:481`) all exist; none is read in `src/ml/feature_extractor.cpp` or `src/ml/token_layout.h`; `kStateFeatureDim` unchanged (6188).
- The row's list is sweep-flow's only. Same class, also absent from the extractor (verified by grep): sweep-triggers' `GameObject::temp_deflect_value`, `pending_death_replacements`, `PlayerState::may_look_at_opp_facedown_turn`; sweep-costs' `RunePool::spell_energy`, `showdown_energy`, `spell_power`, `gear_power` (and the older `unit_energy`). The checkpoint stream needs the union, not this row alone.

### L303 — sweep-flow — "trainer note: a Standard Move with other eligible units"
**Verdict: NOTE (trainer developers)**
- Verified in `GameEngine::executeStandardMove` (`game_engine.cpp:2713-2757`): after a one-unit `StandardMove` intent, while at least one other unit of the player has the same destination among its `standardMoveDestinations` (and the tax, if any, is payable), the engine asks a `MakeChoice` whose first option is `chosen_value = 0`, `choice_label = "Move no more units"` and whose other options carry `chosen_objects = {unit}`; prompt text "move another unit with this move" (with "(pay N [A])" when taxed). It repeats until declined or no unit is left. Move intents are still one per (unit, destination) (`standardMoveDestinations`, CLAUDE.md "Standard Move"). No vocabulary or feature change.

### L304 — sweep-verify-cards — "first report (`.worktrees/reports/sweep-verify-cards.md`, not yet merged)"
**Verdict: DUPLICATE** (every item is a plan checkbox; state confirmed by grep today)
- 3.8: plans/03 3.8 **ticked**.
- 3.13: plans/03 3.13 **open** — only Aphelios 372 was left. Read today: `units/0372_aphelios_exalted.cpp` still writes four `card_counters` before the marker, but idempotently and keyed to the chain item (`__aph_item`), so a re-entered item is not blocked by its own answer; tests `C5Test.ApheliosModeCannotBeChosenTwiceInATurn`, `SweepCostsTest.Aphelios_ReadiesTheTwoRunesThePlayerChooses`, `VerifyCardsTest.JayceBrilliantInventor_IsAskedAgainAfterNewChoicesAndStillResolves` exist. Nothing left but the tick.
- 3.15: plans/03 3.15 **ticked**.
- 3.21: plans/03 3.21 **open** — the one check left is done: `units/0421_veteran_poro.cpp:34` sets only `Keyword::Weaponmaster` (no `Keyword::Equip`), as 779. Nothing left but the tick.
- 7.6, 7.7: plans/07 **ticked**.
- 7.23: plans/07 7.23 **open** — what its last sub-bullet asks for is still missing: Riven 828 has only `VendettaA.Vendetta_Riven_WeaponmasterEquipsOnPlay` / `_AttackDamagePerAttachedEquipment`; no `playThroughEngine` test with two Equipment (decline; accept choosing the second).
- 7.26: plans/07 7.26 **open** — 331 Sentinel Adept and 544 Yone to be re-read now that `weaponmaster_base.h` is merged.
- 7.33: plans/07 7.33 **open** — the row says "stage 5", but the c2 sub-bullet under the box says FIXED without a vocabulary change (`spells/0071_party_favors.cpp:30` `valChoice(opp, 0, "Cards")` / `valChoice(opp, 1, "Runes")`; `ReviewFixC2Test.PartyFavors_TheTwoAnswersAreDistinctValues` exists). The box and the row disagree; whoever ticks it should check the two values land in different `MakeChoice` slots and tick, rather than carry it to stage 5.
- 7.35: plans/07 **ticked**.
- 8.4, 8.7, 8.9: plans/08 **ticked**. 8.8, 8.16: plans/08 **open**.
- S2, S3, S4, S5: plans/08 **open** (S5 still lists 847 Sky Crusier, see L312).

### L305 — sweep-verify-cards — "NEW card defects it found (about 30, listed in"
**Verdict: DUPLICATE** — plans/09 section A, boxes 9.A1–9.A25 (the row's list became those boxes). State by grep today: **ticked** 9.A1–A12, A14 (superseded by 9.G2, ticked), A15–A18, A20, A23–A25; **open** 9.A13 (The Zero Drive 412: `gear/0412_the_zero_drive.cpp:48` still has the ENGINE-GAP comment), 9.A19 (Spirit's Refuge 63), 9.A21 (Jayce 406 "from hand"), 9.A22 (`pickSeveral` cap: `kPickSeveralMaxPicks = 12`, `card_helpers.h:614`).

### L306 — sweep-verify-cards — "NEW, not card files: `Card::costDueNow` is true on"
**Verdict: MIXED**
- `Card::costDueNow` paid again on re-entry: DONE — `card.cpp:27-50` (`finalizingChoices` sets bit 8 once a run that reported a cost due reaches the marker; `costDueNow` returns false on bit 8); plans/09 9.A4 ticked; tests `SweepCostsTest.CostDueNow_IsPaidOnceWhenAPromptFollowsIt`, `.CostDueNow_IsPaidOnceWhenTheCardIsPlayedByAnEffect` (`tests/cards/test_sweep_costs.cpp:959, 975`). The remaining shape (prompt between a paid cost and the marker inside a finalize pass) is plans/09 **9.J2** (open).
- `pickSeveral` "any number" stops at 12: DUPLICATE — plans/09 **9.A22** (open); still `kPickSeveralMaxPicks = 12`.
- `weaponmaster_base.h` detach by vector edit / may at resolution: DONE — `units/weaponmaster_base.h:37` (`kFinalizeTrigger`), `:81` (`payEquipAndAttach`, which goes through `EffectExecutor::attachGear` / `unattachGear`); plans/09 9.A24 ticked; tests `SweepCostsTest.Weaponmaster_TheMayAndTheEquipmentAreChosenAtFinalization`, `.Weaponmaster_TakingAnAttachedEquipmentAnnouncesTheUnattach`.
- Ornn 381 / Ivern 613 "draw it": DONE — `EffectExecutor::drawHeldCard` (`effect_executor.cpp:1017-1033`: hand, knowledge, `draws_this_turn`, `CardsDrawnEvent`), called from `units/0381:68`, `units/0613:72` (and 626, 443); plans/09 9.A17 ticked; test `SweepTriggersTest.RevealAndDrawIt_CountsAsADraw` (381, 613, 626 through `fireTriggerThroughEngine`, asserts the event and the counter), `.CalledShot_DrawingOneCountsAsADraw`.
- The Zero Drive 412 "Banish this" paid at resolution: DUPLICATE — plans/09 **9.A13** (open).

### L307 — sweep-verify-cards — "existing tests that assert something false or cannot fail"
**Verdict: DUPLICATE** — plans/09 section B (9.B1–9.B13). Ticked: B3–B13. Open: **9.B1**, **9.B2**.
- 9.B1 is open but the work is done — **ready to tick**: `C3Test.SquareUp_DoesNotForceADiscard` (`tests/cards/test_review_fix_c3.cpp:443`) now declines the Repeat and asserts exactly one prompt ("only the Repeat offer"), and `C3Test.SquareUp_RepeatIsPaidWithADiscard` (`:458`) pays it (discard gone, Assault 8) and shows no offer with an empty hand. Corrected by sweep-costs with 8.9 (its report §4); the test's name is now misleading ("DoesNotForceADiscard") but its assertions can fail.
- 9.B2 is really open: `C5Test.WeaponmasterUnitsUseTheSharedMay` tail (`tests/cards/test_review_fix_c5.cpp:564-570`) still calls `fireTriggerThroughEngine(dead, TriggerType::WhenYouPlayMe)` with no agent.

### L308 — perf-search — "trainer notes: `currentStep()` / `applyChoice()` / `beginGame()` / `resumeFromSnapshot()`"
**Verdict: NOTE (trainer developers)** — each fact checked against the headers:
- `src/engine/game_engine.h:113` `const StepResult& beginGame(const DeckSubmission& deck1, const DeckSubmission& deck2, uint64_t seed = 0);`
- `:145` `const StepResult& resumeFromSnapshot(GameState snapshot_state, const std::mt19937_64& rng_state, const StepResult& snapshot_step);`
- `:164` `const StepResult& currentStep() const;` — doc comment: "Returns a view of the engine's own record of the pending decision ... The next applyChoice overwrites it — copy the StepResult (`auto step = ...`) to keep it across a choice."
- `:175` `const StepResult& applyChoice(int legal_index);`
- `src/openspiel/riftbound_state.h:207` `const std::vector<::riftbound::Intent>& legalIntents() const` — "A view, not a copy: valid until this state is next advanced (or destroyed)."
- `RiftboundGame` no longer owns its card table: `src/openspiel/riftbound_game.h:69-70` holds `const CardDB& card_db_; const CardRegistry& registry_;`; the table is `sharedCards()` (`:44`, `struct SharedCards { CardRegistry registry; CardDB db; }`), built once per process. (plans/09 **9.E3**, open: `src/main.cpp` still builds a second table.)
- `src/ml/feature_extractor.h:84` `void extractStateFeatures(const GameState& state, PlayerId perspective, const CardDB& card_db, std::vector<float>& out);` (clears `out`, keeps capacity); the returning overload at `:77` is still there.
- `src/openspiel/riftbound_state.h:222` `size_t decisionsReplayedToBuild() const` (0 for a state that never was a clone, is snapshot-only, or resumed straight from its snapshot).
- "observation values and layout unchanged": `kStateFeatureDim` is unchanged (6188); value equality is UNDETERMINED by reading — it rests on perf-search's test run.
- Finding while checking: the doc comment at `src/ml/feature_extractor.h:74` still says "Build a 354-dim state feature vector" (the vector is 6188). OPEN 9.K39.

### L310 — orchestrator — "RETROFIT "not a defect still changes the code": every NOT"
**Verdict: MIXED**
- card-site closures: DONE — every line in plans/0*.md and plans/cards/batch-*.md that says NOT A DEFECT (28 lines) names a pin and a clarified file, except the exempt ones (04 items 4.16, 4.17, 4.20: file deleted) and the two below. The 13 distinct pin tests named after "pinned by" / "pin:" all exist (`VerifyCardsTest.Pin_3_17_…`, `Pin_3_18_…`, `Pin_3_21_…` ×2, `Pin_7_35_…`, `Pin_Batch03_…` ×2, `Pin_Batch08_…`, `Pin_Batch10_…`, `Pin_Shurelya509_…`, `Pin_Udyr_…`, `SweepTriggersTest.StackedDeck_PuttingACardIntoHandIsNotADraw`; the Fae Porter pin was deleted with 9.G2, as its line says); 16 `Pin_` tests in `tests/cards/test_review_fix_verify_cards.cpp`. Code-site comments were re-read for 523 and 779 only; the others rest on the plan lines.
- "confirming the 8.7 virtuals are gone": DONE — `git grep -n "requiresLegion\|requiresLevel\|levelThreshold" -- src` returns nothing (two comments in tests only); plans/08 8.7 ticked; its engine-side NOT A DEFECT sub-bullet is about code that no longer exists (exempt).
- "the 02 item 2.20 plan line": OPEN 9.K2 — the pin exists (`VerifyCardsTest.Pin_2_20_ForgottenMonument523_NoScoreMeansNoScoreEventAndNoConquerTrigger`, `tests/cards/test_review_fix_verify_cards.cpp:742`) and the card site is clarified (`battlefields/0523_forgotten_monument.cpp:17-21`), but the sub-bullet at plans/02-engine-rules.md:108 still reads "The turn-gated return is NOT A DEFECT: ..." with no "pinned by / clarified in", and the engine site the finding was raised against (`GameEngine::isScoreGatedByTurn`, `game_engine.cpp:4978`, and its two early returns in `scoreConquer` / `scoreHold`) has no comment. **Since then:** the plan line was rewritten in the required form by this stream (commit `5e84282`); the engine comment is what is left. **Since then:** the plan line was rewritten in the required form by this stream (commit `5e84282`); the comment at the engine site is what is left (9.K2).

### L311 — orchestrator — "REOPENED under "nothing optional": residuals that earlier streams closed"
**Verdict: MIXED**
- Solari Shrine 72 (killer not tracked): DONE — `LastKnown::killed_by`, read in `gear/0072_solari_shrine.cpp:29` (`lk.valid && lk.stunned && lk.killed_by == controller`); tests `SweepTriggersTest.SolariShrine_TriggersOnlyWhenItsControllerKilledTheUnit`, `.SolariShrine_DoesNotTriggerWhenTheOpponentKilledTheirOwnUnit` (lethal damage through `testHook_processLethalDamage`).
- Unlicensed Armory 23 (rescue needs an Armory in play): DONE — the replacement is stored on the unit (`GameObject::pending_death_replacements`, pushed at `gear/0023_unlicensed_armory.cpp:56`); tests `SweepTriggersTest.UnlicensedArmory_ReplacementAppliesWithTheGearGone`, `.UnlicensedArmory_ReplacementEndsWithItsTurn`.
- Scuttle Crab 615 (only cards facedown now): DONE — `PlayerState::may_look_at_opp_facedown_turn` set at `units/0615_scuttle_crab.cpp:44`; test `SweepTriggersTest.ScuttleCrab_ShowsCardsHiddenLaterThatTurn` (asserts the private reveal on a later `CardHiddenEvent`, and none without the permission).
- Astral Heron 831 (countered response hides the first spell): DONE — `units/0831_astral_heron.cpp:36` (`playOrdinalThisTurn(state, played) == 1` in `triggerCondition`); plans/09 9.D1 ticked; tests `C7Test.AstralHeron_AFirstSpellWhoseResponseWasCounteredStillCounts`, `.AstralHeron_AFirstSpellAnsweredByYourOwnReactionStillCounts`, `.AstralHeron_APlayOrdinalFromAnEarlierTurnDoesNotCount`. The sibling case (the first card is itself countered) is DUPLICATE of plans/09 **9.H2** (open).
- Yone 544 (previous-controller field on `ScoreEvent`): DONE — `ScoreEvent::previous_controller` (`src/core/events.h:140`), `BattlefieldState::conquered_from` set in `GameEngine::scoreConquer` (`game_engine.cpp:4995`), read in Yone's `triggerCondition` (`units/0544_yone_blademaster.cpp:28-39`); tests `C6Test.Yone_ConqueringAnUncontrolledBattlefieldHitsTheChosenBaseUnit`, `.Yone_ConqueringAnEnemyBattlefieldDoesNothing`.
- Renekton 879 exact threshold edge: DONE — `Card::mightThreshold()` (10) + `TriggerManager::onStateSettled` (`trigger_manager.cpp:473-491`, fires on each below → at-or-above crossing of a settled state); batch-07 879 ticked; tests `SweepTriggersTest.Renekton_IsEmpoweredWhenAnotherEffectTakesHimToTen`, `C7Test.RenektonBrute_BecomesEmpoweredWhenHisAbilityTakesHimToTen`, `C7Test.RenektonBrute_IsEmpoweredWhateverTakesHimToTen`.
- Defender of Tomorrow 936 (two printed abilities as one activation): OPEN 9.K38 — unchanged (`legends/0936_defender_of_tomorrow.cpp:17-21`: one activation readies 2 gear while Empowered, else 1); batch-02 936 is ticked FIXED with that residual in its text ("splitting them would renumber the [Empower] ability's index, i.e. its action-vocabulary slot"); STATUS line 62 names it for stage 5, but no checkbox carries it, so the final gate cannot count it.

### L312 — cards-c7 — "BLOCKED batch-07: Renekton, Brute 879 (engine event for"
**Verdict: MIXED** — no ENGINE-GAP / BLOCKED comment is left in any of the nine files.
- Renekton, Brute 879: DONE — see L311; batch-07 879 ticked.
- Atakhan 732: DONE — `units/0732_atakhan.cpp:41-80` (`additionalPlayCost` / `payAdditionalPlayCost`: `confirmNow`, `chooseObjectNow`, the killed unit's printed `energy_cost` / `power_cost` come off); batch-07 732 ticked; tests `SweepCostsTest.Atakhan_KillsAUnitAndCostsItsPrintedCostLess`, `.Atakhan_TheKillCanBeDeclinedWhenTheFullCostIsPayable`.
- Loyal Poro 718 / Lonely Poro 777: DONE — `LastKnown::other_friendly_units_here` read at `units/0718:26`, `units/0777:25`; snapshot for a batch in `EffectExecutor::beginDeathBatch` (`effect_executor.cpp:190-197`); batch-07 718 and 777 ticked; tests `SweepTriggersTest.LoyalPoro_UnitsThatDieTogetherWereNotAlone`, `.LonelyPoro_UnitsThatDieTogetherDidNotDieAlone`, `.LonelyPoro_DrawsWhenItDiesAlone`, `.KillAllEffect_UnitsItKillsDiedTogether` (9.D2). (`DeathBatch` scope is plans/09 9.H1, open.)
- Vex 712 can't-move flag: DONE for the Standard Move (`units/0712_vex_apathetic.cpp:38`, `GameEngine::standardMoveDestinations` `game_engine.cpp:3191`, cleared `:1212`; batch-07 712 ticked; `SweepFlowTest.Vex_ThePlayedUnitCannotBeMovedThisTurn`). The executor half has no test: 9.K13 (under L301).
- Poppy, Defender of the Meek 740: DONE — `units/0740:22-45` (3 XP spent in `payAdditionalPlayCost`, counted in `additionalPlayCost`); batch-07 740 ticked; test `SweepCostsTest.Poppy_SpendsXpForADiscountChosenBeforePayment`.
- Mageseeker Investigator 725: DONE — `units/0725:23-34` sets `BattlefieldState::multi_move_tax[opponent]`; charged in `GameEngine::executeStandardMove` (`game_engine.cpp:2714-2755`); batch-07 725 ticked; tests `SweepFlowTest.MageseekerInvestigator_TaxesEachUnitBeyondTheFirst`, `Misc2Test.MageseekerInvestigator_FlagsItsBattlefield`.
- Reluctant Leader 908 / Vex 712 token plays: DONE — `TriggerManager::onTokenCreated` → `announceUnitPlayed` (`trigger_manager.cpp:422-439, 381-`; CR 350.2 "Tokens are not cards, but can still be Played"); batch-07 line 86 ticked; tests `SweepTriggersTest.TokenUnit_IsAUnitBeingPlayed`, `.TokenUnit_IsAUnitTheOpponentPlayed` (Vex stuns the token), `.TokenGear_DoesNotTriggerPlayAUnit`.
- Sky Crusier 847 (typed discard takes the first gear): **not DONE** — the fix is in the engine (`GameEngine::payAndQueueAbility`, `game_engine.cpp:1717-1745`: a typed discard asks `decideNow(... "choose a card to discard (cost)")` one card at a time, CR 422.1.a; added by sweep-costs' `2053a08` although its report says "no change"), but **no test goes through it**: the only 847 tests are `VendettaB.Vendetta_SkyCrusier_CostAndTargets` (descriptor fields) and `_Deals4` (direct `onActivate`); Mel 897, the other typed-discard card, has none either. Plan side: 847 has no batch box of its own; it sits in the open plans/08 **S5** box, and the sub-bullet under batch-07 "S5" (plans/cards/batch-07.md:104) still says "847 Sky Crusier — BLOCKED". OPEN 9.K26.

### L313 — cards-c7 — "`shared:` commit `e52e52a` to review: `selectObjectAs(ctx, chooser, ...)`"
**Verdict: MIXED (review)**
- `selectObjectAs` (`card_helpers.h:1150-`): reviewed OK — a `selectObject` asked of `chooser`; mandatory; the answer is validated against the options (else the first); options sorted; state in two `resume_data` slots. Not engine code. Tests `C7Test.SafetyInspector_EachPlayerChoosesTheirOwnUnit`, `C7Test.Atakhan_TheDefenderChoosesWhichOfTheirUnitsHereDies`.
- `TriggerManager::onScore` fires delayed `WhenIHold` abilities on their controller's hold (`trigger_manager.cpp:984-987`): OPEN 9.K15 — (a) no card logic in the engine (the comment names Ashe, the code is generic); (c) `C7Test.AsheFocused_TheBanishedCardReturnsWhenTheyHold` (conquer does not return it; hold after Ashe died does). (b) **Finding:** `DelayedAbility` has one player field, `controller`, used both for "whose event fires it" and for who controls the resulting chain item (`checkDelayedAbilities`: `da.controller != relevant_player` → skip; `chain_.addAbility(da.source, da.controller, ...)`). Ashe, Focused 731 therefore registers her return with `da.controller = opp` (`units/0731_ashe_focused.cpp:64-70`), so the ability on the chain is controlled by the opponent: the opponent is the "you" of that item (its `ctx.controller`, priority, anything keyed on the item's controller — e.g. the holder's own Blue Sentinel 649 repeats it, since `onScore` copies every item with `ci.controller == e.player`). A delayed ability has the properties of the ability it is (CR 391.1, 392): Ashe's controller controls it. Needs a separate "fires for this player's event" field. Also `expires_on_turn = std::numeric_limits<int>::max()` predates `DelayedAbility::kUntilFired` (works, `expiresAtEndOf` is false for it) and should use the constant.
- "4 existing test files corrected": reviewed OK — the c7 branch changed six existing test files (`git diff 76ab21f^1...76ab21f^2 -- tests`): `test_choice_helpers.cpp` and `test_review_fix_chain_triggers.cpp` (738 removed from the offender lists; fixed card, `C7Test.ViPeacekeeper_StunsTheEnemyUnitHereItsControllerChooses` covers it, including the untargetable enemy that the next file dropped); `test_rules_compliance_cards.cpp` (Vi removed from the custom-target-list loop with a comment naming that test); `test_ivern_deck.cpp` (Daisy moved from a direct `onTrigger` to `fireTriggerThroughEngine`: stronger); `test_vendetta_A.cpp` (Blade Twirler through the engine, comment cites "The first time I move each turn"; Astral Heron `turn_number++` — later rewritten by sweep-triggers); `test_vendetta_B.cpp` (Jayce 855 through the engine with the played card as subject, comment cites the text; Profiteer: the only possible cost is no longer prompted). Each has a comment; none is a weakening.

### L314 — cards-c7 — "Corrupted Dragon 878 uses `mode.victory_score + turn.victory_score_bonus`"
**Verdict: MIXED**
- victory score: DONE — `GameState::effectiveVictoryScore()` (`game_state.h:857`), used by 878 (`units/0878_corrupted_dragon.cpp:22`), 79, 47, 556, 678; `git grep "mode\.victory_score" -- src/cards` → nothing; tests set `victory_score_bonus = 1` in `tests/cards/test_review_fix_c7.cpp:845` (Corrupted Dragon) and `test_review_fix_c2.cpp:1206`, and `tests/test_review_fix_sweep_flow.cpp:1086`.
- Astral Heron 831 documented limit: DONE (9.D1 ticked, see L311); the remaining rules question is plans/09 **9.H2** (open) — DUPLICATE.
- Black Rose Dignitary 714 warning: DONE by report, not re-measured — commit `0d2f003` ("unused targets parameter in 18 card files ... 714"); `units/0714_black_rose_dignitary.cpp:19` now has `/*targets*/`; `sweep-verify-cards-2.md` reports 0 warnings in card files. (A data-free fix: no test applies.) UNDETERMINED without a build log, covered by 9.C6.

### L315 — cards-c7 — "trainer note: units asking more or different `MakeChoice` prompts"
**Verdict: NOTE (trainer developers)** — all eleven do ask; kind and timing per card, read from the files:
- 722 Ultrasoft Poro: at resolution, one location prompt per Bird token (`playTokensChoosingLocations`, 2 tokens: base or a controlled battlefield).
- 726 Safety Inspector: at finalization, yes/no "spend 3 XP to skip your kill?" (`confirmOptional`); at resolution, the controller picks one of their units (unless paid) and then the OPPONENT picks one of theirs (`selectObjectAs` — a prompt to the non-controller).
- 732 Atakhan: while being played, before payment: yes/no (asked only when the full cost is payable) and an object pick for the unit to kill (`confirmNow` / `chooseObjectNow` — changed again by sweep-costs after the c7 note); at resolution the DEFENDER picks which of their units dies (`selectObjectAs`).
- 738 Vi, Peacekeeper: at finalization, target pick "an enemy unit here" (`pickTarget`, `kFinalizeTrigger`).
- 741 Rift Herald: at resolution, yes/no "reveal a unit from the top 3 and draw it?" then a pick among those units; on [Deathknell], a pick of a unit card from hand.
- 754 Daisy!: at finalization, target pick "an enemy unit here".
- 810 Zed, From the Shadows: the optional discard additional cost (engine prompt at play), then at resolution a location prompt for the Shadow Clone (`pickTokenDest`, a mode pick).
- 869 Profiteer: at finalization, yes/no, then which Empowered object to disempower (`selectObject`; not asked when there is only one), then which object to empower (`pickTarget`).
- 888 Gust Monk: at finalization, target pick for [Assault 2], then which card in any trash to banish as the cost (`selectObject`).
- 896 Illaoi, Prophet: at resolution, a location prompt for the Tentacle on each of play / conquer / hold.
- 901 Kharox: at resolution, yes/no "play a unit from their trash?" then the pick.
- Also true and not in the row: with Zilean 648 on a battlefield, 722 / 810 / 896 (any token unit play) add a yes/no and possibly a location prompt (`EffectExecutor::createToken`).

### L316 — cards-c7 — "NEW: `createToken` Zilean doubling plays the copy at"
**Verdict: MIXED**
- copy placed with no choice / "may" never asked: DONE — `EffectExecutor::createToken` (`effect_executor.cpp:1622-1650`): `confirmNow(... "play an additional copy of ...?")`, then `chooseLocationNow` over the instruction's `location_choices` when there was a choice; batch-06 648 ticked; tests `SweepTriggersTest.Zilean_AdditionalCopyIsAskedAndPlacedByChoice`, `.Zilean_AdditionalCopyCanBeDeclined`, `C6Test.KeeperOfMasks_ZileansExtraReflectionBecomesACopyToo`.
- Finding while reading it: OPEN 9.K16 — (1) the engine carries card-named state: `PlayerState::zilean_present` / `zilean_double_token_turn` (`game_state.h:196, 206`), read and written in `effect_executor.cpp:1634-1637` and reset in `game_engine.cpp:5184`. The mechanism is generic (a card sets a flag from `applyPassiveAura`), the names are not. (2) "Once each turn" is printed on each Zilean, but the stamp is one per player and `applyPassiveAura` only sets a bool, so two Zileans at battlefields give one extra copy a turn, not two.

### L317 — cards-c7 — "NEW: Fate Weaver 626 moves the chosen spell to"
**Verdict: DONE** (DUPLICATE of plans/09 9.A17, ticked) — `units/0626_fate_weaver.cpp:68` `ctx.executor.drawHeldCard(ctx.controller, drawn, /*seen_by_all=*/true)`; test `SweepTriggersTest.RevealAndDrawIt_CountsAsADraw` has a 626 case (asserts `CardsDrawnEvent` and `draws_this_turn`).

### L318 — cards-c7 — "NEW: `WhenOpponentPlaysAUnit` carries no `triggering_subject`"
**Verdict: DONE** — `TriggerManager::announceUnitPlayed` (`trigger_manager.cpp:403-`) fires `WhenOpponentPlaysAUnit` with the played unit as subject; the counter is gone (`git grep __opp_played_unit_id -- src tests` → nothing); Vex 712 reads `chain.resuming->triggering_subject` (`units/0712:29-31`). Tests through real events: `SweepTriggersTest.TokenUnit_IsAUnitTheOpponentPlayed`, `SweepFlowTest.Vex_ThePlayedUnitCannotBeMovedThisTurn` (a real play, the played unit is the one stunned), `C7Test` Vex test at `test_review_fix_c7.cpp:203` (subject passed). No test plays two units before the first trigger resolves; the cause of that scenario (one shared counter) no longer exists.

### L319 — cards-c6 — "BLOCKED: Blue Sentinel 649 and Red Brambleback 591 ("hold"
**Verdict: MIXED** — no ENGINE-GAP / BLOCKED comment left in the seven files.
- Blue Sentinel 649 / Red Brambleback 591: DONE — `Card::extraScoreTriggers` (`units/0649:24`, `units/0591:26`), read in `TriggerManager::onScore` (`trigger_manager.cpp:1088-1118`: every ability this score queued for the scoring player is queued once more per extra); batch-06 649 and batch-10 591 ticked; tests `SweepTriggersTest.BlueSentinel_HoldEffectsHereTriggerAnAdditionalTime`, `.BlueSentinel_DoesNotDoubleAHoldAtAnotherBattlefield`, `.RedBrambleback_ConquerEffectsHereTriggerAnAdditionalTime`.
- Undying Legion 587: DONE — `Card::trashPlayCost` (`units/0587:20`); batch-06 587 ticked; test `SweepCostsTest.UndyingLegion_IsPlayedFromTheTrashForItsLegionCost`.
- Arachnoid Horror 679: DONE — `extraPlayLocations() = kPlayToLoneEnemyBattlefield` (`units/0679:27`), grant `PlayerState::grant_units_lone_enemy_bf` (`:36`), both read in `GameEngine::unitPlayableAt`; batch-06 679 ticked; tests `SweepFlowTest.ArachnoidHorror_MayBePlayedToALoneEnemyUnitsBattlefield`, `.ArachnoidHorror_GrantsThePermissionToOtherFriendlyUnits`.
- Lillia, Protector of Dreams 620: DONE — `TriggerType::WhenYouPlayATokenUnit` (`units/0620:20`), fired in `TriggerManager::onTokenCreated` (`trigger_manager.cpp:427-438`); batch-06 620 ticked; tests `SweepTriggersTest.TokenUnit_IsAUnitBeingPlayed`, `.TokenGear_DoesNotTriggerPlayAUnit`.
- Zilean 648: DONE, with the new finding 9.K16 — see L316.
- Soraka 550: DONE — `EffectExecutor::healExhaustRecall` (`units/0550:49`), `recallToBase`; batch-06 550 ticked; test `SweepTriggersTest.Soraka_RecallsTheUnitWithoutMovingIt`. The two-copies test is DUPLICATE of plans/09 **9.I5** (open).

### L320 — cards-c6 — "Wily Newtfish 670: XP sources outside its range still"
**Verdict: DUPLICATE** — plans/cards/batch-06.md **670 Wily Newtfish** (line 62) is still `[ ]` with a stale `BLOCKED` sub-bullet; the work is done and it is **ready to tick**: the only write of `xp +=` left in `src/` is inside `gainXp` (`card_helpers.h:1137`; `git grep -nE "\.xp *\+=|xp\+\+" -- src` → that line only), so all twelve sites the sub-bullet lists (gear 658, 671, 698, 720; legends 759, 787; spells 657, 669, 751; battlefield 769; units 719, 724) go through it. Tests: `SweepTriggersTest.GainXp_IsCountedForTheTurn` (a gear outside c6's range, engine path, asserts `xp_gained_this_turn`), `C6Test.WilyNewtfish_SeesXpGainedThisTurn`.

### L321 — cards-c6 — "`shared:` commit `65518a8` to review: `Card::canBeReadied()` (checked in"
**Verdict: MIXED (review)** — five items reviewed OK, one finding
- `Card::canBeReadied()` (`card.h:1023`): reviewed OK — honoured by `GameEngine::awakenPhase` (`game_engine.cpp:833`) and `EffectExecutor::readyObject` (`effect_executor.cpp:815-818`); the other engine writes of `is_exhausted = false` are zone changes (bounce, recycle), rune payment and the legend in the Awaken Phase, none of them "ready this unit". No card logic in the engine. Card text: Maduli 706 "I can't be readied." Test `C6Test.Maduli_CannotBeReadied` (Awaken Phase and `readyObject`, with a control unit that is readied).
- `Card::optionalAdditionalCost(state, player)` overload (`card.h:796-799`): reviewed OK — defaults to the stateless one; asked at `game_engine.cpp:6028` and `card.cpp:913, 927`; only Crescent Guardian 684 overrides it. Tests: the Crescent Guardian tests in `tests/cards/test_review_fix_c6.cpp` (around `:1000-1020`, `__crescent_paid`).
- `recalculateAuras` resets `untargetable_by_enemy` in its own pass (`game_engine.cpp:5272-5277`): reviewed OK — required because a hook sets the flag on other objects (Alpha Wildclaw 619, Master Yi 621, Akali 825). Tests `tests/cards/test_review_fix_c6.cpp:733-741` (619: smaller unit protected, larger / away / enemy / Alpha itself not, and it ends) and `:761-764` (621), `SweepFlowTest` at `tests/test_review_fix_sweep_flow.cpp:1252`.
- helper `gainXp` (`card_helpers.h:1134-`): reviewed OK — see L320.
- helper `selectAnyNumber` (`card_helpers.h:1117-`): finding — it carries its own cap `kSelectManyMax = 10` with the comment "ChainManager allows 16 re-entries per resolution", which is stale (the bound is 1024 and throws) and is **not** among the caps plans/09 9.D3 lists. Folded into 9.K3 (L322). Users: Aphelios 372, Azir, Sovereign 497 ("any number" of tokens — silently capped at 10), Bard 540.
- `testHook_awakenPhase` (`game_engine.h:250`): reviewed OK — test-only forwarder, used by `C6Test.Maduli_CannotBeReadied`.

### L322 — cards-c6 — "DUPLICATE: `selectAnyNumber` (c6, state in slots, composes with"
**Verdict: OPEN** — both helpers still exist in `card_helpers.h` (`pickSeveral` `:620`, state in `resume_point` 110–112 and the whole of `resume_data`, cannot share a callback with a `pick*` helper, cap 12; `selectAnyNumber` `:1117`, state in slots from 7, composes, cap 10). Users: `pickSeveral` — Albus Ferros 230, Fae Dragon 423; `selectAnyNumber` — 372, 497, 540. No plan box asks for the unification (9.A22 and 9.D3 are about `pickSeveral`'s cap only). OPEN 9.K3.

### L323 — cards-c6 — "residuals on ticked items: Bard 540 pays "exhaust your"
**Verdict: MIXED**
- Bard 540 pays "exhaust your legend" at trigger finalization: DONE in code — `units/0540_bard_mercurial.cpp:50-72` (`additionalPlayCost` / `payAdditionalPlayCost` exhaust the legend as the card is played, CR 356.2.b); test `SweepCostsTest.Bard_ExhaustsTheLegendAsItIsPlayed` (`tests/cards/test_sweep_costs.cpp:277`). But the file header (`:17-20`) still says "ENGINE-GAP: OptionalAdditionalCost can only charge Energy / Power / a discard ... It is offered and paid when the play trigger is finalized instead" — it now contradicts the code below it. OPEN 9.K29.
- Scuttle Crab 615: DONE — see L311.
- Yone 544: DONE — see L311. One stale comment in the same file (`units/0544_yone_blademaster.cpp:49-51`: "the Weaponmaster base decides at resolution, so this card is not opted into choices at finalization" — the base returns `kFinalizeTrigger` since 9.A24, so his conquer target is now chosen at finalization): folded into 9.K29.

### L324 — cards-c6 — "NEW: Ambush units carry `Keyword::Reaction` from reminder text"
**Verdict: DONE (this stream, commit `8f1feb1`; run)**
- Decided from CR 822.1.b ("I have [Reaction] as long as I'm being played to a battlefield where you control Units") and CR 813.4 (a conditionally granted Reaction is not the keyword until the condition holds): the def must not carry it. Removed from all **14** [Ambush] units — the row's eight (560, 574, 583, 622, 633, 676, 682, 705) and 711, 728, 738, 740, 804, 910.
- Readers checked: the Ambush generators read `Keyword::Ambush` (`GameEngine::generateShowdownActions` / the Closed-State generator, `game_engine.cpp`), `extraAnyPowerCost` reads `Reaction || is_hidden || Ambush`, the reaction-play generators filter spells first. Nothing changes for an Ambush unit.
- Tests (in `tests/cards/test_review_fix_ledger.cpp`, run green; the first two red against the unfixed defs): `LedgerTest.Defs_APermanentHasReactionOnlyWhenTheCardItselfDoes` (whole registry), `LedgerTest.Ambush_StillGrantsReactionTimingToABattlefieldWithYourUnits` (offered as `PlayReaction` to the battlefield with her units, not to base or an empty battlefield), `.Ambush_StillGrantsItsTimingInAShowdown`, `.MysticVortex_StillTaxesReactionQuickDrawAndAmbushCards`.
- plans/08 **8.8** ticked with the list.

### L327 — cards-c6 — "reprint region tags (539, 543, 546, 547, 537, 548)"
**Verdict: USER** — closed NOT A DEFECT with pin (`VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting`) and comments in the six files; plans/03 3.21 is open only for the tick (see L304). The data it rests on is `cards/raw/gallery_raw.json`; question 1 below.

### L328 — cards-c5 — "BLOCKED batch-05: Weaponmaster 324 / 414 / 421 / 448"
**Verdict: MIXED** — no ENGINE-GAP / BLOCKED comment left in the thirteen files.
- Weaponmaster 324 / 414 / 421 / 448 / 407 / 435: DONE — all six derive from `WeaponmasterUnit` (`units/weaponmaster_base.h`), which pays the gear's declared Equip cost one Power less through `Card::payEquipAndAttach` (`:81`); batch-05 line 98 ticked; tests `SweepCostsTest.Weaponmaster_ChargesTheEquipCostLessOnePower`, `.Weaponmaster_MovesAnAttachedEquipmentAndPaysANonResourceCost`, `.Weaponmaster_TheMayAndTheEquipmentAreChosenAtFinalization`.
- Jax, Unrelenting 440: DONE — `class JaxUnrelenting : public WeaponmasterUnit` (`units/0440:16`), no text parsing left; batch-10 440 ticked; tests `RulesCards2Test.RulesCards2_Jax_WeaponmasterChargesANonResourceEquipCost`, `_WeaponmasterStillChargesThePrintedEnergy`, `_WeaponmasterEquipCostsOnePowerLess`.
- Minotaur Reckoner 337: DONE — `GameObject::aura_cant_move_to_base` (`units/0337:35`), refused in `EffectExecutor::moveToBase` via `moveToBaseBlocked` (`card_helpers.h:837-844`), recall unaffected; batch-05 337 ticked; tests `SweepTriggersTest.MinotaurReckoner_StopsEffectMovesToBase`, `C5Test.MinotaurReckonerStopsMovesToBaseOnlyWhileOnBoard`.
- Commander Ledros 231: DONE — `units/0231:30-56` (`payAdditionalPlayCost`: `chooseObjectNow` one unit at a time, each kill one Power off); batch-05 231 ticked; test `SweepCostsTest.Ledros_KillsUnitsOfThePlayersChoiceForPowerOff`.
- Fae Dragon 423 second sentence: DONE — `TriggerType::WhenYouSpendABuff` (`units/0423:27-31`), announced by `EffectExecutor::spendBuff(unit, spender)`; batch-05 423 ticked; tests `SweepTriggersTest.FaeDragon_PlaysGoldWhenItsControllerSpendsABuff`, `SweepCostsTest.BuffSpentAsACost_IsCreditedToThePlayerWhoSpentIt` (9.D13).
- Lux, Crownguard 312: DONE — `rune_pool.spell_energy += 2` (`units/0312:26`), spent only for spells; batch-05 312 ticked; test `SweepCostsTest.RestrictedEnergy_SpellOnlyPaysForSpellsOnly`.
- Irelia, Graceful 462: DUPLICATE — plans/cards/batch-05.md **462** (line 197) is `[ ]`, and its `BLOCKED` sub-bullet is stale. What is done: the discount is counted before payment and the player picks "[1] less" or "[A] less" when both apply (`GameEngine::targetCostDiscount` `game_engine.cpp:1888-1907`, the choice at `:2182-2199`), and a single-target spell that takes its target through `pickTarget` (`needsPlayTimeTarget`) now chooses it at play, before payment (`:2096-2128`); test `SweepCostsTest.PlayGate_ADiscountFromTheChosenTargetCountsBeforePayment` (Energy half). What is left: (1) the "[A] less" half has no engine-path test — plans/09 **9.J4** (open); (2) spells that choose at chain time are not covered — the 17 `needsPlayTimeTargetPair` spells and any spell that picks in `onResolve` without `needsPlayTimeTarget` still pay full price when they choose Irelia (`:2099-2100` handles the single-target case only). The box stays open for (2); its sub-bullet should be rewritten to say so.
- Ezreal, Prodigy 470: DONE — `Card::optionalCostDiscount` (`units/0470:20`), applied in `GameEngine::payOptionalCost`; batch-10 470 ticked; test `SweepCostsTest.Ezreal_OptionalAdditionalCostsCostOneLess`.

### L330 — cards-c5 — "`shared:` changes to review: `GameObject::aura_cant_move_to_base` (read by the"
**Verdict: MIXED (review)**
- `GameObject::aura_cant_move_to_base`: reviewed OK — set by Minotaur Reckoner's `applyPassiveAura`, reset each aura pass (`game_engine.cpp:5153`), read by `standardMoveDestinations` (`:3214`) and by `moveToBaseBlocked` for effect moves. Card text "Units can't move to base." No card logic in the engine (two trailing comments name the card). Tests as under L328.
- `EffectExecutor::lookBeforeReveal(player)` (`effect_executor.cpp:1745-1761`): the primitive is fine (private look, keep / recycle; Void Hatchling 341 "If you would reveal cards from a deck, look at the top card first. You may recycle it. Then reveal those cards."; test `C5Test.ApprenticeSmithRevealHonoursVoidHatchling`, `FinishBatch3.VoidHatchlingRevealPeek`). **The parenthesis in the row is not done**: it is called by `revealUntil`, `revealAndChoose`, Apprentice Smith 364 and Rek'Sai 491 only. Cards whose text reveals from the top of their Main Deck and that take the cards by hand without calling it: Ravenbloom Conservatory 529, Void Burrower 553, Void Rush 505, Teemo, Strategist 121, Diana, Lunari 641, Pakaa, Protector 820. Needing a ruling first: Blind Fury 25 (each OPPONENT reveals — their Hatchling), Twisted Fate 200 (reveals from the RUNE deck; the helper only knows the Main Deck), and the "look at the top N ... you may reveal one from among them" cards 381, 594, 613, 626, 741. OPEN 9.K30.
- helper `pickSeveral`: DUPLICATE — plans/09 9.A22 / 9.D3 (open), and 9.K3.
- `createGoldExhausted` honouring `tokens_enter_ready` (`card_helpers.h:258-270`): reviewed OK for the rule (CR 184.1 as cited; test `C5Test.GoldTokensEnterExhaustedUnlessRenataReadiesThem`), with a finding: it sets `is_exhausted` AFTER `createToken` has announced the token — the same defect plans/09 **9.H3** (open) describes for Bloodharbor Ripper 746 and Card Sharp 403, but 9.H3 does not list this helper, which 23 card files call (Fae Dragon 423, Plundering Poro 392 / 778, Chem-Baroness 556, ...). Also `createGoldToken` passes tags `{"Gold"}` and `createGoldExhausted` passes `{}` for the same token card. OPEN 9.K17.
- `CostModifier::max_printed_energy_cost` (`game_state.h:257`, `costModifierApplies` `game_engine.cpp:6118-6124`): reviewed OK — generic filter; Jayce 406 sets 7 ("Energy cost no more than [7]"); test `C5Test.JayceKillsTheChosenGearAndGrantsAPlayThisTurn` (a [2] gear is affordable with no Energy, a [9] gear is not). The "from hand" half is plans/09 9.A21 (open).
- Karthus repeat filter (`trigger_manager.cpp:913-916`): reviewed OK — `obj.hasKeyword(Keyword::Deathknell) ? deathknell_double_count : 0`; keyed on a keyword and a player counter, no card id; text "Your Deathknell effects trigger an additional time."; tests `C7Test.Karthus_RepeatsAnEmpoweredDeathknell`, `C7Test.EmpoweredDeathknellCardsCarryTheDeathknellKeyword`.
- "3 existing tests corrected" (`git diff b95b4e4^1...b95b4e4^2 -- tests`): reviewed OK — `AuditFix1Test.Akshan_*` ×4 (`WhenYouPlayMe` → `AsYouPlayMe`, comment "the theft; WhenYouPlayMe is [Weaponmaster]"); `RulesCards2Test.RulesCards2_JaeMedarda_DrawsWhenASpellOfMineChoseHer` (the enemy-spell half now asserts `triggerCondition` false, comment cites batch-05 463); `WaveABatch3Test.DirewingReadyWithAnotherDragon` (cites the text "I enter ready if you control another Dragon" and 3.16: a second Direwing counts). Plus offender-list rows removed for fixed cards. None is a weakening.

### L332 — cards-c5 — "Karma, Channeler 235 tags set to `{Karma, Ionia}`"
**Verdict: USER** — verified: `units/0235_karma_channeler.cpp:53` `d.tags = {Karma, Ionia}`; `cards/raw/gallery_raw.json` entry `ogn-235-298` lists tags `["Vi", "Ionia"]`; entry `sfd-237-221` lists `["Karma"]` and `units/0548_karma_channeler.cpp:55` has `{Karma}`. Question 2 below.

### L335 — cards-c5 — "NEW: Legion Quartermaster 367 / Zaun Punk 481 additional cost"
**Verdict: DONE** — both declare the cost as a play cost: `units/0367_legion_quartermaster.cpp:34-43` (`additionalPlayCost`, `mandatory = true`; `payAdditionalPlayCost`), `units/0481_zaun_punk.cpp:41-49` (optional), as Cruel Patron 208 (`units/0208:33-42`); plans/07 7.6 ticked. Tests `SweepCostsTest.LegionQuartermaster_ReturnsAGearAsItIsPlayed`, `.ZaunPunk_OptionalKillIsPaidDuringThePlay`, `.CruelPatron_KillIsPaidBeforeItEntersAndGatesThePlay`. (The ticked batch-09 367 line still describes the older finalization-time fix; harmless, but it is not what the code does now.)
- Same family, **not converted**: Safety Inspector 726 — "You may spend 3 XP as an additional cost to play me." is still asked and paid when the play trigger is finalized (`units/0726_safety_inspector.cpp:21-24` ENGINE-GAP comment, `:40-50`), after the unit has entered; the hook Poppy 740 uses for exactly this cost now exists. OPEN 9.K28.

---

## Found while auditing, outside the rows' wording

- ENGINE-GAP comments left in `src/` (8): `card_helpers.h:701` and `units/0878:31` → plans/09 9.D3 (open); `gear/0412:48` → 9.A13 (open); `units/0540:18` stale, `gear/0401:23` stale (fixed: `SweepCostsTest.GrantedRepeatEqualToItsCost_ChargesThePowerToo`), `spells/0931:45` stale (fixed: `SweepCostsTest.DeathMark_FlowPaysTwoAnyPowerFromTwoDomains`) → 9.K29; `units/0726:22` real → 9.K28; `card_helpers.h:696` real and in no plan → 9.K4.

## Boxes that are still `[ ]` although the work is done (ready to tick, evidence above)

- plans/cards/batch-00.md:101 "Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)" — L299.
- plans/cards/batch-06.md:62 "670 Wily Newtfish" — L320.
- plans/09 9.B1 — L307.
- plans/03 3.13 and 3.21 — L304.
- plans/07 7.33 — L304 (check the two `MakeChoice` values land in different slots first).

### L336 — cards-c5 — "NEW: Aphelios 372 "Ready 2 runes" readies the first two"
**Verdict: DONE**
- Fix: `src/cards/units/0372_aphelios_exalted.cpp` `ApheliosExalted::onTrigger` case 0 — with more than two exhausted runes the player picks through `selectAnyNumber` (sorted list, filled from lowest ids if fewer than 2 picked).
- Test: `SweepCostsTest.Aphelios_ReadiesTheTwoRunesThePlayerChooses` (SC:807) — three exhausted runes, script picks r3 and r2, asserts r1 stays exhausted; engine path (`fireTriggerThroughEngine`).
- Set the row to `done (sweep-costs)`.

### L337 — cards-c5 — "NEW: stale CLAUDE.md pitfall — says `combat_designation` is cleared"
**Verdict: DONE**
- `CLAUDE.md` line 125 now reads "Combat designations follow the combat, not the trigger (CR 323.2)" and describes `processContestedBattlefields` re-deriving the designation and `__defend_attacker_id` being recorded by `TriggerManager`. Matches the code: `src/engine/game_engine.cpp` `GameEngine::processContestedBattlefields` (first loop, CR 323.2.a-c); `src/engine/trigger_manager.cpp:664` stamps `__defend_attacker_id`.
- Documentation only; no test needed. Set the row to `done`.

### L340 — cards-c3 — "open batch-03: Death Mark 931 `[A][A]` Flow from two domains"
**Verdict: MIXED (fix DONE; stale comment OPEN)**
- Fix: DONE — `src/engine/game_engine.cpp` `GameEngine::trashGrantCost` (domain mask `resource::maskOf(any_domain ? Domain::Count : power_domain)`), `canPayTrashGrant` (`resource::canPayIn`), `payTrashReplayGrant` (`resource::payIn`). Test `SweepCostsTest.DeathMark_FlowPaysTwoAnyPowerFromTwoDomains` (SC:772): 1 Fury + 1 Calm Power, asserts the trash play is offered, the card ends banished, Power is 0. Box `plans/cards/batch-03.md` "931 Death Mark" is ticked.
- OPEN — `src/cards/spells/0931_death_mark.cpp:45-47` still carries `// ENGINE-GAP: the engine's any-domain grant path pays the whole [A][A] from ONE domain, so the Flow play is not offered ...`. That is now false. See 9.K29.

### L341 — cards-c3 — "open batch-03: Square Up 579, Curtain Call 743 Repeat cost"
**Verdict: DONE (with a ledger note)**
- Fix: `Card::repeatCosts()` read in `src/engine/game_engine.cpp` `executePlaySpell` (the `RepeatTranche` block: energy / power / domain mask / `discard_cards`); no text parsing. Boxes `plans/cards/batch-03.md` 579 and 743 ticked; `plans/08` 8.9 ticked.
- Tests: `C3Test.SquareUp_RepeatIsPaidWithADiscard` (`tests/cards/test_review_fix_c3.cpp:458`), `SweepCostsTest.CurtainCall_EachRepeatCostIsOfferedOnceAndRunsAnotherMode` (SC:454: pays the [1] Repeat, asserts Draw 1 ran once and the repeat dealt 2 with a different mode).
- NOTE for the ledger (outside this slice): `plans/09` **9.B1** (`C3Test.SquareUp_DoesNotForceADiscard` "must change with 8.9") is still `- [ ]` although sweep-costs' report says the test was corrected to expect the Repeat offer. Slice owner of 9.B should verify and tick.

### L343 — cards-c3 — "DUPLICATE: `DelayedAbility::repeats` (c3) and `DelayedAbility::every_time` (c2)"
**Verdict: DONE**
- `src/core/game_state.h` `struct DelayedAbility` has one flag, `bool repeats`; `git grep every_time src tests` returns nothing. `TriggerManager::checkDelayedAbilities` (`src/engine/trigger_manager.cpp:101`) reads `da.repeats` only.
- Covered by the existing repeat users' tests (Rally the Troops: `C3Test.RallyTheTroops_*`; Imperial Decree). A pure rename/removal; no new behaviour to pin.

### L344 — cards-c3 — "`shared:` changes to review"
**Verdict: MIXED (reviewed OK except: 1 duplicate of 9.G1, 3 OPEN)**
Commits: `2098475`, `423a690`, `334cee5`, `e976900`, `a2bbb8c`, `d765f48`; card commit `7296ea6`.
- `GameObject::banish_if_would_die_this_turn` (`src/core/game_object.h:246`): read in `EffectExecutor::killObject` (`effect_executor.cpp:265`) and `GameEngine::killUnit` (`game_engine.cpp:6810`), cleared in the expiration step (`game_engine.cpp:1205`). (a) no card logic in the engine (Smite sets the flag in `spells/0568_smite.cpp:24`; the engine only names it in comments). (b) matches CR 370.1.b (replacement replaces the event: banished, no death, no `UnitDiedEvent`). (c) `C3Test.Smite_BanishesTheUnitIfItDiesLaterThisTurn` (`test_review_fix_c3.cpp:848`) — covers the `killObject` path only (asserts banished, not in trash, 0 `UnitDiedEvent`). The `GameEngine::killUnit` path (lethal damage in cleanup / combat) has no test: grep of `tests/` for the field finds only two Dusk Rose Lab tests that also kill through a card. **OPEN 9.K5.**
- `__facedown_play_bf` / `__facedown_play_turn`: stamped in `GameEngine::executePlayFromHidden` (`game_engine.cpp:3112`) and in the reaction path `ChainManager` (`chain_manager.cpp:377`); read only by `facedownPlayBattlefield` (`card_helpers.h:890`, turn-checked). (a) OK. (b) CR 811.1.d / 811.1.d.5. (c) `C3Test.Guards_PlayedFromFacedownTheTokenGoesToThatBattlefield` (c3:924), `C3Test.LotusTrap_PlayedFromFacedownKeepsTheTargetItWasPlayedWith` (c3:952). **reviewed OK.**
- `playPayingCost` / `canPlayPayingCost` stage `current_play_source` (`effect_executor.cpp` `StagedEffectCost`, lines 2001-2024): restores the previous value in the destructor; readers are `GameEngine` cost code (`game_engine.cpp:6442`, `from_hand` for `grantsAccelerateTo`), Drag Under 485 and Void Drone 333, all comparing against `Hand` only. (a) OK. (c) `C3Test.DragUnder_CostsTwoLessWhenAnEffectPlaysItFromOutsideTheHand` (c3:773). Finding: a card played by an effect from a deck, the Champion Zone or facedown is labelled `Intent::PlaySource::Banishment` (`else if (from != ZoneType::Hand)`), which is false; harmless today because every reader tests `!= Hand`, but the enum has `ChampionZone` / `Hidden` and nothing for a deck. **OPEN 9.K19.**
- `checkDelayedAbilities` on unit plays and conquers: `TriggerManager::announceUnitPlayed` (`trigger_manager.cpp:393`) and `onScore` (`:1025`, scoped to the conquering unit via `target_filter`). (a) OK (card names in comments only). (b) CR 391-392 (delayed abilities active during their window). (c) `C3Test.RallyTheTroops_BuffsEveryFriendlyUnitPlayedThisTurn` (c3:509), `C3Test.RelentlessPursuit_AttachingIsOptionalAndTheConquerRiderIsArmed` (c3:561: asserts the ability is armed with `target_filter == unit`, then `testHook_scoreConquer` + `testHook_runChain` and asserts the unit is back at base). **reviewed OK.**
- `facedownPlayBattlefield` (`card_helpers.h:890`): **reviewed OK** (above).
- `playLocations` / `playTokensChoosingLocations` (`card_helpers.h:918`, `:981`): CR 355.2.a (base or a controlled battlefield), CR 811.1.d.5 (`only_bf`). Tests: `C3Test.TokensWithNoPrintedLocationCanGoToAControlledBattlefield` (c3:997), `C3Test.FlurryOfFeathers_PlaysFourBirdsOnceWhateverTheLocationPrompts` (c3:1013), `C3Test.Arise_EachSandSoldierCanBePlayedToAControlledBattlefield` (c3:1509). Finding: both ignore `BattlefieldState::blocks_unit_play` and `PlayerState::units_play_base_only` — **DUPLICATE of plans/09 9.G1 (open)**; confirmed still true in the code (`playLocations` has neither check; `pickEffectPlayLocation` / `effectPlayLocation` do).
- `lockedPlayTarget` (`card_helpers.h:959`): single-target only (`targets[0]`), returns `kInvalidId` for a locked target that is no longer legal (CR 359.3.e.5). Test: `C3Test.LotusTrap_PlayedFromFacedownKeepsTheTargetItWasPlayedWith`. **reviewed OK.**
- 7 existing tests corrected (all in `7296ea6`): each has a comment citing card text or CR and none is weaker than before:
  1. `AuditFix6Test.SmokeMirrors_FewerThanTwoTargetsNoOp` — now expects the Draw 1 (CR 359.3.e.5 cited; card text "... Draw 1."). OK, but the test name still says "NoOp".
  2. `EnginePathTest.SpellTargetThenModeIsAskedThroughTheEngine` — Keeper's Verdict: the owner answers top/bottom (card text cited). Stronger (two agents, one prompt each).
  3. `CardTestFixture.DeathFromBelow_GrantsTrashReplayWhenMightLow` — rewritten on the engine path; asserts the replay happened and `trash_replay_grants.empty()` (card text cited). Stronger, but the name says the opposite of what it asserts.
  4. `JhinDeckTest.RocketBarrage_WithRepeat_TwoEffectsResolve` — from two direct `invokeOnResolve` calls to `playThroughEngine` with the Repeat paid. Stronger.
  5. `JhinDeckTest.PickMode_AgentSelectsModeIdx_FromMultipleLegalModes` — adds the target prompt after the mode. OK.
  6-7. `AurasCostsTest.Repeat_PowerComesFromRecyclingTheRuneToTheRuneDeck`, `.Repeat_FloatingResourcesPayTheTranche` — target tagged Mech (Danger Zone text cited). OK.
  Finding: misleading names in 1 and 3. **OPEN 9.K32.**

### L345 — cards-c3 — "Dancing Grenade 582 and Death from Below 747 replay through"
**Verdict: DONE**
- Fix: `src/engine/chain_manager.cpp` (around line 568, the `replayed` check after a spell resolves: a spell whose source is on the chain again is not filed in the trash; the new play files it). Both card files' ENGINE-GAP comments are gone and replaced by a description of the mechanism (`spells/0582_dancing_grenade.cpp`, `spells/0747_death_from_below.cpp`).
- Test: `SweepTriggersTest.ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves` (ST:855) — plays 747, replays it, a watching agent counts "seen in trash while on chain" == 0 and the card is in the trash exactly once at the end. Mutation-tested per `plans/reports/sweep-triggers-followup.md` build A ("replayed-spell detection off").
- NOTE (ledger wording): `plans/cards/batch-03.md` line 127 (582) still ends "Remaining engine gap noted in the card: the card object is in its owner's trash while the replay is on the chain" — stale; the box is ticked and the gap is closed. Reword when the ledger is edited.

### L346 — cards-c3 — "NEW: `resolveSpell` drops illegal play-time targets and the rest shift"
**Verdict: MIXED (audit DONE: no other card reads a shifted role; one OPEN)**
- Engine fix (not re-audited beyond reading it): `Card::chosenTarget` (`src/cards/card.cpp:763`) reads the role from `chain.resuming->targets`; tests `SweepFlowTest.TargetRoles_TheSecondTargetIsNotPromotedWhenTheFirstIsGone` (SF:719), `.TargetRoles_Deathgrip_ThePumpTargetIsNotKilledInPlaceOfTheFirst` (SF:742).
- Audit of the remaining multi-target cards. Every card in `src/cards` with `TargetRequirements.count >= 2` or an `enumerateLegalTargetSets` override (19 spells; no unit / gear / legend / battlefield has one):
  - `chosenTarget`: 346, 484, 645, 758.
  - Ignore `targets` and take both roles from `pickTargetPair` (roles in `resume_data[3..4]`, never positional): 29, 128, 220, 334, 429, 436, 466, 472, 672, 690, 717, 870.
  - No roles (loop over `targets`): 105 Singularity.
  - Single target: 224 Salvage (`targets[0]`, one slot).
  - `git grep 'targets\[1\]'` / `targets[2]` / `targets.back()` in `src/cards`: no hit.
  - **402 Bellows Breath** (`spells/0402_bellows_breath.cpp` `onResolve`): `targets[0]` is the anchor for "at the same location". A dropped first target is harmless (the survivors shared its location). But if the first-chosen unit MOVED in response it is still a legal unit, the anchor becomes its new location, and the two units that stayed together take nothing — the outcome depends on pick order. No test covers a first target that left or moved (`C3Test.BellowsBreath_TargetSetsAreUnitsAtOneLocation` and the four `AuditFix6Test.BellowsBreath_*` are static). **OPEN 9.K33.**

### L347 — cards-c3 — "NEW: Called Shot 443 "draw one" moves the card to hand"
**Verdict: DUPLICATE (of a ticked box) — set the row to done**
- `plans/09` **9.A17**, ticked (FIXED by sweep-triggers: `EffectExecutor::drawHeldCard`). Test exists: `SweepTriggersTest.CalledShot_DrawingOneCountsAsADraw` (ST:797) asserts `draws_this_turn` +1 and one `CardsDrawnEvent`, through `playThroughEngine`.

### L348 — cards-c3 — "NEW (latent): `checkDelayedAbilities` iterates `state_.delayed_abilities` by reference"
**Verdict: MIXED (fix DONE; test missing → OPEN)**
- Fix present: `TriggerManager::checkDelayedAbilities` (`src/engine/trigger_manager.cpp:101-140`) decides what fires on a copy (`std::vector<DelayedAbility> firing`), erases the fired one-shots, and only then calls `chain_.addAbility`; the comment says why.
- No test adds a delayed ability from inside a delayed ability's finalize pass (grep of `tests/` for `delayed_abilities`: none does). See 9.K18.

### L349 — cards-c3 — "NEW: spell defs carrying a keyword only mentioned in their text"
**Verdict: DONE (this stream, commit `8f1feb1`; run)**
- The four the row names (`Temporary` on 632, 645, 727; `Assault` + `assault_value` on 579) and the rest of the class, found by scanning every def for a keyword not printed as the card's own: `Assault` on 4, 325, 572 and legend 502; `Shield` on 57, legend 551, battlefields 274, 764; `Tank` on 57; `Temporary` on 69, 94, 180, 631, 757, legends 749, 756, battlefield 764; `Ganking` on 572, legend 262, battlefield 292; `Deflect` on 606, battlefield 773; `Hidden` on legend 259; `Weaponmaster` on legend 512; `Repeat` on gear 401, battlefields 525, 772; `Equip` on 544. Their `assault_value` / `shield_value` / `deflect_value` went with them. Kept because printed: `Temporary` on gear 426, 504, 640, 647.
- No card read its own def for the effect (each passes a literal to `giveTemporaryKeyword` / `createToken`), so behaviour is unchanged; `GameEngine::beginningStep` only kills [Temporary] objects that have a location.
- Test: `LedgerTest.Defs_AKeywordTheTextGivesToSomethingElseIsNotOnTheCard` (33 rows, values, and positive controls; red against the unfixed defs). plans/08 **8.8** ticked.

### L350 — cards-c3 — "NEW: Undying Loyalty 730 `selfCostReduction` cannot see printed costs"
**Verdict: MIXED (fix DONE; stale comment OPEN)**
- Fix: DONE — `src/cards/spells/0730_undying_loyalty.cpp` `UndyingLoyalty::additionalPlayCost` (sees the CardDB; `best.energy_reduction = 2` only if a Bird/Cat/Dog/Poro of cost ≤ [2] and ≤ [A] is in the trash) and `payAdditionalPlayCost` (`chooseObjectNow` among units still affordable; returns the discount for the unit actually chosen). The cleaner hook the row asked for exists.
- Test: `SweepCostsTest.UndyingLoyalty_TheDiscountFollowsTheUnitChosenAsItIsPlayed` (SC:439) — not offered with only a [4] unit in trash; offered with a Poro; plays for the Power alone and the Poro is on the board.
- OPEN — the file's header comment (lines 19-23) still describes the old scheme ("The cost path cannot see that choice, so the [2] discount is granted up front ... pays the [2] back as the choice is made"), contradicting the code below it. See 9.K29.

### L351 — cards-c1 — "BLOCKED batch-01: Prodigal Explorer 514 (needs a counter"
**Verdict: MIXED (all five sub-items DONE; one test gap OPEN)**
- Prodigal Explorer 514: DONE — `legends/0514_prodigal_explorer.cpp` `canActivateAbility` reads `PlayerState::enemy_choices_this_turn >= 2`; counted in `GameState::noteChosen` (`src/core/game_state.h:864`), called from `chain_manager.cpp:75,153`, `game_engine.cpp:1446`, `card.cpp:415`, `card_helpers.h:727`. Box `plans/cards/batch-01.md` 514 ticked. Test `SweepFlowTest.ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities` (SF:1178): friendly choice 0, enemy unit 1 (not offered), enemy gear 2 (offered). OPEN: the test only chooses with a spell; the "unit abilities" half of `noteChosen` (`by->is_ability && source isUnit()`) and the negative (a gear's or legend's ability does not count) are untested. See 9.K7.
- Emperor of the Sands 512: DONE — keyword ability (`src/engine/keyword_abilities.cpp` `WeaponmasterAbility`, `TriggerManager::queueKeywordAbilities`); box ticked; `plans/08` 8.4 ticked. Tests `SweepTriggersTest.Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed` (ST:551), `.Weaponmaster_IsAMay` (ST:566), `SweepCostsTest.SandSoldier_HasWeaponmasterFromTheEmperor` (SC:615, through the legend's real activation).
- Endless Riches 809: DONE — box ticked; `GameEngine::generateTrashReplayActions` (the `may_play_from_trash` branch, any card type at its own cost) and `engine/trash_replacement.h` `settleInTrash`. Tests `SweepCostsTest.EndlessRiches_CardsArePlayedFromTheTrashAtTheirOwnCost` (SC:867), `.EndlessRiches_WhatWouldGoToTheTrashIsBanishedExceptFromTheDeck` (SC:899).
- 294 / 506 / 786 restricted pools: DONE — the cards add to `RunePool::spell_power` (294:25), `gear_power` (506:25), `showdown_energy` (786:23); box ticked. Tests `SweepCostsTest.RestrictedPower_SpellOnlyPaysForSpellsOnly` (SC:646), `.RestrictedPower_GearOnlyPaysForGearAndGearAbilities` (SC:677), `.RestrictedEnergy_ShowdownOnlyIsUnusableOutsideAShowdown` (SC:697), `.RestrictedResources_EmptyWithThePool` (SC:710).
- Swift Scout 259: DONE — `legends/0259_swift_scout.cpp` `adjustHideCost`; `GameEngine::hideCostFor` (`game_engine.cpp:2997`); box `plans/cards/batch-08.md` 259 ticked. Test `SweepCostsTest.SwiftScout_HidesForOneEnergyInsteadOfThePower` (SC:722): hide not offered with [1] and no Power, offered with the legend, Energy spent.

### L353 — cards-c1 — "Battle Mistress 517 once-per-batch is in c1's range"
**Verdict: DONE**
- Fix: `Card::triggersOncePerBatch` (`card.h:930`), overridden in `legends/0517_battle_mistress.cpp:27`, honoured in `TriggerManager::fireTrigger` (`trigger_manager.cpp:310`); batches opened by `processLethalDamage` and by the `DeathBatch` guard (9.D2, ticked).
- Tests: `SweepTriggersTest.BattleMistress_TriggersOnceForABatchOfDeaths` (ST:304: two lethally damaged enemy units, one chain item from the legend), `SweepTriggersTest.KillAllEffect_UnitsItKillsDiedTogether` (ST:1059).

### L356 — cards-c1 — "`shared:` changes to review: `TriggerType` `WhenAnEnemyUnitAttacksYourBattlefield`"
**Verdict: MIXED (reviewed OK except 1 OPEN)**
Commit `ef01bd9`; tests commit `3a7f069`.
- `WhenAnEnemyUnitAttacksYourBattlefield` — `TriggerManager::onCombatStarted` (`trigger_manager.cpp:716-739`): once per attacking unit as subject, for the battlefield controller's on-board cards and legend; skipped when the controller is the attacker. (a) OK. (c) `ReviewFixC1Test.NineTailedFox_AttackersOfYourBattlefieldGetMinusOne` (c1:851), `.NineTailedFox_NotAtABattlefieldYouDoNotControl` (c1:872). **reviewed OK.**
- `WhenYouMoveAnEnemyUnit` — `TriggerManager::onUnitMoved` (`:1214-1234`): non-standard move while a chain item controlled by another player is resolving; moved unit is the subject. (a) OK. (c) `ReviewFixC1Test.BlastCone_StunsAUnitMovedByAnotherOfYourAbilities` (c1:670), `AuditFix6Test.BlastCone_ExhaustsToStunMovedEnemy`. **reviewed OK.**
- `WhenYouRecycleARune` — `TriggerManager::onLeftBoard` (`:925-942`): a rune leaving the board to the Rune Deck. (a) OK. (c) `ReviewFixC1Test.BattleMistress_RecyclingARuneOffersAGold` (c1:1074). **reviewed OK.**
- `beginningStep` no longer kills attached [Temporary] gear — `GameEngine::beginningStep` (`game_engine.cpp:861-867`), citing CR 718.2. Test `ReviewFixC1Test.SpinningAxe_TemporaryOnlyKillsItWhileUnattached` (c1:513). Correct for PRINTED [Temporary] (CR 718.2 "the card's printed Rules Text is Inactive"; the example after CR 721.2 is Spinning Axe). Finding: the check is on `obj.keywords`, which is also where Turn to Dust 632 ("Give a gear [Temporary]") and Fading Memories 180 write a GRANTED [Temporary]; a granted keyword is not printed rules text, so an attached Equipment given [Temporary] should still be killed, and is not. **OPEN 9.K6.**
- Tests corrected in 12 existing files (`test_audit_fixes_0`, `test_audit_fixes_6`, `test_choice_helpers`, `test_equipment`, `test_ivern_deck`, `test_jhin_deck`, `test_the_list`, `test_wave_a_batch3`, `test_wave_a_batch4`, `test_wave_b_per_turn`, `test_review_fix_chain_triggers`, `test_rules_compliance`): read every hunk. Each carries a comment citing card text; most move a direct `onTrigger` / `applyReplacement` call to the engine path and add assertions (Zero Drive Deathknell, Shard of Undoing "each opponent must kill" answered by P2, Frigid Jewel real draws, Last Rites pays costs, The Boss pays [C]). None weakens an assertion. Two remarks, no action: `test_choice_helpers.cpp` drops the `TargetedTrigger{695 ... Blast Cone}` row without a comment (Blast Cone no longer takes a target on play; it is covered by the two Blast Cone tests above), and `test_the_list.cpp` still calls `onTrigger` directly (its header says the engine-path test is in `test_review_fix_c1.cpp`). **reviewed OK.**

### L357 — cards-c1 — "NEW: `grant_repeat_base_to_next_spell` carries only Energy"
**Verdict: MIXED (fix DONE; stale comments OPEN)**
- Fix: `GameEngine::executePlaySpell` (`game_engine.cpp:2231-2240`) builds the granted tranche from `base.energy_cost` AND `base.power_cost`, Power in the spell's own Domains. Test `SweepCostsTest.GrantedRepeatEqualToItsCost_ChargesThePowerToo` (SC:487): with one Power short the Repeat is not payable; with two it is paid and Draw 4 runs twice. `plans/08` 8.9 ticked.
- OPEN — comments that still say "Energy only": `gear/0401_temporal_portal.cpp:20-24` (`ENGINE-GAP: the grant carries the spell's Energy cost only ...`), `battlefields/0772_the_academy.cpp:19-21` ("tranche cost = the spell's base energy"), `src/core/game_state.h:200` ("tranche cost = its base energy"). See 9.K29 (cards) and 9.K8 (game_state.h).

### L358 — cards-c1 — "NEW: `is_rules_text_inactive` is set by only one attach path"
**Verdict: MIXED (engine fix DONE; one card still attaches by hand → OPEN)**
- Fix: `attachEquipped` / `attachFree` (`card_helpers.h:110-118`) are one-line calls to `EffectExecutor::attachGear` (`effect_executor.cpp:952-972`: on-board checks, detach from the old bearer, `is_rules_text_inactive = true`); `unattachGear` clears it (`:843`). Blighted Battleaxe 581 is an equipped trigger (`equippedTriggerType() == AtEndOfTurn`). Tests `SweepTriggersTest.AttachGear_RefusesAUnitThatIsNotOnTheBoard` (ST:762, asserts the flag), `.BlightedBattleaxe_EndOfTurnAbilityFiresWhileAttached` (ST:784).
- OPEN — `spells/0334_angle_shot.cpp:72-81` still attaches by writing `g.attached_to`, `u.attachments`, `location`, `zone`, `attachment_might_bonus` itself and emitting the two events: no `is_rules_text_inactive`, and its only guard on the unit is `objectExists` (a unit that left the board is "equipped"). It is the only such site left (`git grep 'attachments.push_back'` in `src`: executor and this card). See 9.K34.
- OPEN — stale comments "attachFree has no board check of its own" in `gear/0460_edge_of_night.cpp:49` and `legends/0554_grandmaster_at_arms.cpp:49,55`. See 9.K29.

### L359 — cards-c1 — "NEW: `TriggerManager::onUnitStunned` queues with `addAbility` directly"
**Verdict: DONE**
- Fix: `TriggerManager::onUnitStunned` (`trigger_manager.cpp:71-87`) collects watchers with `cardFiresOn` (→ `Card::firesOn`, which covers `triggerTypes()`) and queues through `fireTrigger(..., WhenYouStun, subject = stunned unit)`, so `fired_trigger`, `triggerCondition` and the inactive-text gate apply.
- Test: `SweepTriggersTest.WhenYouStun_FiresForAnEnemyUnitWithItAsTheSubject` (ST:625): stunning your own unit does nothing; stunning an enemy moves Vex to that unit's battlefield. Mutation-tested (follow-up report, build A).

### L363 — cards-c4 — "BLOCKED batch-04: Brazen Buccaneer 2 (`canAfford` ignores"
**Verdict: DONE**
- Brazen Buccaneer 2: box `plans/cards/batch-04.md` line 40 ticked. Pre-payment hook `Card::additionalPlayCost` (default built from the card's optional cost), counted by the play gate. Test `SweepCostsTest.BrazenBuccaneer_DiscardCountsTowardAffordability` (SC:387): not offered with [4] and an empty hand; offered with a card to discard; the discard is forced and Energy is 0.
- Kraken Hunter 150: box line 73 ticked. `units/0150_kraken_hunter.cpp` `additionalPlayCost` / `payAdditionalPlayCost` (buffs spent one at a time before payment through `ctx.executor.spendBuff(unit, ctx.controller)`, 9.D13). Test `SweepCostsTest.KrakenHunter_SpendsBuffsBeforePaying` (SC:351): [3] and no Power, three buffs → played, exactly two buffs spent.

### L368 — cards-c4 — "`shared:` changes to review: `PlayerState::units_enter_ready_by`"
**Verdict: MIXED (reviewed OK; 1 duplicate; 2 OPEN)**
Commits `8f4acfb`, `b475300`, `5f3057d`, `151a8f5`, `56f226f`.
- `PlayerState::units_enter_ready_by` (`game_state.h:184`, `otherUnitsEnterReady`): read in `GameEngine::resolvePermanent` (`game_engine.cpp:2588`), `EffectExecutor::createToken` (`effect_executor.cpp:1604`) and `playIgnoringCost` (`:2202`); cleared each aura pass (`game_engine.cpp:5177`) and refilled by Magma Wurm 11 / Wuju Master 784 from `applyPassiveAura`. (a) OK. (b) CR 143.4.a. (c) `C4Test.MagmaWurm_OtherFriendlyUnitsEnterReady` (c4:360: the Wurm itself enters exhausted; a card and a token enter ready; the opponent's unit does not), `SweepFlowTest.WujuMaster_LevelElevenUnitsEnterReady` (SF:1166). The third read site (`playIgnoringCost` with a Wurm, or with `units_enter_ready_this_turn`) has no test — `SweepTriggersTest.PlayIgnoringCost_AsksTheUnitWhetherItEntersReady` covers only `entersReadyOnPlay`. **OPEN 9.K20.**
- `BattlefieldState::opp_hidden_unrevealable[2]` (`game_state.h:493`): read in `GameEngine::generateHiddenPlayActions` (`game_engine.cpp:3775`), reset each aura pass (`:5203`), set by Noxus Saboteur 18 for the opponent only. (a) OK. (c) `C4Test.NoxusSaboteur_OpponentsCantPlayTheirHiddenCardsHere` (c4:389). **reviewed OK.**
- `GrantedAbilityRef::exhaust_cost_only` (`game_object.h:171`): the aura-granted ability generator skips a ref whose ability has no [E] cost (`game_engine.cpp:4248`). (a) OK. (c) `C4Test.Heimerdinger_HasOnlyTheExhaustAbilities` (c4:535). It is enforced in the generator only — an applied intent is not re-checked: **DUPLICATE of plans/09 9.G5 (open)**.
- `pickEffectPlayLocation` / `effectPlayLocation` (`card_helpers.h:857-883`): CR 355.2.a; honour `blocks_unit_play` and `units_play_base_only`. Tests `C4Test.FlameChompers_PaysAndChoosesItsLandingPlace`, `C4Test.Soulgorger_PlaysTheChosenUnitWhereItsControllerChooses`, `VerifyCardsTest.SpectralMatron_DoesNotOfferABattlefieldUnitsCannotBePlayedTo`. **reviewed OK.**
- "other streams' batch-09 'lands in base' cards should use them": the ten cards the box lists (`plans/cards/batch-09.md` line 68, "Effect-played units always land in base", still `- [ ]`) are all reported fixed there; what keeps it open is 9.G1. But nine more cards play a unit by an effect with no printed location and pass no location to `playIgnoringCost` / `playPayingCost` (default: base), in no box: Blind Fury 25 (`:44`), Reinforce 62 (`:87`), Promising Future 115 (`:95`), The Harrowing 198 (`:39`), Baited Hook 242 (`:99`), The Zero Drive 412 (`:74`), Last Rites 471 (`:79`), Void Burrower 553 (`:136`), Cursed Sarcophagus 710 (`:83`). Every other caller passes a location (checked all 38 files that call either function; 112 and 461 play spells). **OPEN 9.K35.**
- New state fields: NOTE, see the feature-layout note under L375.

### L369 — cards-c4 — "bisect note: commits `b475300` up to the batch-04 card commit"
**Verdict: NOTE (final report)**
- Verified with `git log`: `8f4acfb` → `b475300` → `5f3057d` → `151a8f5` → `18489ec` → `72615f2` (the batch-04 card commit) → `1d9bc7f` → `a98a7fd`, all ancestors of HEAD. Verified by reading (not by building): at `b475300` `BattlefieldState::opp_hidden_unrevealable` is `bool[2]` while `src/cards/units/0018_noxus_saboteur.cpp:32` still assigns `b.opp_hidden_unrevealable = true` and `tests/cards/test_wave_b_noxus.cpp` still reads it as a bool; both are fixed in `72615f2` / `a98a7fd`. So `b475300`, `5f3057d`, `151a8f5`, `18489ec` do not compile on their own.
- Same class, for the same paragraph of the final report: sweep-triggers reports `7dc7aee` does not build without `947e61d` (`plans/reports/sweep-triggers.md` §1; not re-verified here).

### L371 — cards-c4 — "NEW: `EffectExecutor::playIgnoringCost` never asks `entersReadyOnPlay`"
**Verdict: DONE**
- Fix: `EffectExecutor::playIgnoringCost` (`effect_executor.cpp:2196-2205`) — `ready = otherUnitsEnterReady || units_enter_ready_this_turn || self_card->entersReadyOnPlay(...)`.
- Test: `SweepTriggersTest.PlayIgnoringCost_AsksTheUnitWhetherItEntersReady` (ST:734): Yi, Honed enters ready, a plain unit enters exhausted (CR 143.4). Mutation-tested (build A).

### L372 — cards-c4 — "NEW: Vision look in `TriggerManager::onCardPlayed` never asks"
**Verdict: DONE**
- Fix: `VisionAbility` (`src/engine/keyword_abilities.cpp:22`), queued once per instance by `TriggerManager::queueKeywordAbilities` (`trigger_manager.cpp:356`, called at `:464`). Matches CR 817.2 / 817.2.a. `plans/08` 8.4 ticked.
- Tests: `SweepTriggersTest.Vision_TheControllerChoosesWhetherToRecycle` (ST:512: keep on top vs recycle), `.Vision_EachInstanceTriggersSeparately` (ST:529), `.Forecaster_HasOneInstanceOfVision` (ST:540).
- Related open box, not a blocker: 9.D8 (fixture default answers Predict with "recycle").

### L373 — cards-c4 — "NEW: Darius, Trifarian 27 does not see himself"
**Verdict: DONE (both)**
- Darius 27: `units/0027_darius_trifarian.cpp` `triggerTypes()` includes `WhenYouPlayMe`; `triggerCondition` uses `playOrdinalThisTurn(state, played) == 2`. Tests `SweepTriggersTest.Darius_SeesHimselfAsTheSecondCard` (ST:643, `playThroughEngine`), `.Darius_IsNotTriggeredAsTheFirstCard` (ST:651). Box `plans/cards/batch-04.md` 27 ticked.
- Cruel Patron 208: `units/0208_cruel_patron.cpp` `additionalPlayCost` (mandatory, CR 356.2.a) / `payAdditionalPlayCost`. Test `SweepCostsTest.CruelPatron_KillIsPaidBeforeItEntersAndGatesThePlay` (SC:220): not offered without a friendly unit; the player chooses the unit; the Patron itself is not offered; no chain item. `plans/07` 7.6 ticked by sweep-costs.

### L374 — cards-c2 — "BLOCKED: Wallop 146 / Call to Glory 207 "spend a buff"
**Verdict: DONE**
- Fix: `spells/0146_wallop.cpp:35-44`, `spells/0207_call_to_glory.cpp:30-39` — `additionalPlayCost` / `payAdditionalPlayCost` returning `PlayCostAdjust::ignore_cost`; the spell play gate counts it. Box `plans/cards/batch-02.md` line 139 ticked.
- Tests: `SweepCostsTest.Wallop_SpendingABuffIgnoresTheCost` (SC:400: unplayable with no Energy and no buff; playable with a buff; buff spent; and with Energy in hand the buff may be kept), `.CallToGlory_SpendingABuffIgnoresTheCost` (SC:426), `.BuffSpentAsACost_IsCreditedToThePlayerWhoSpentIt` (SC:822).

### L375 — cards-c2 — "`shared:` engine changes to review: `PlayerState::units_enter_ready_this_turn`"
**Verdict: MIXED (reviewed OK; 1 duplicate of 9.G1; 1 OPEN; feature-layout NOTE)**
Commits `2073a7a`, `53538d1`, `209c06a`; card commit `310bf85`.
- `PlayerState::units_enter_ready_this_turn` (`game_state.h:166`): read at the same three sites as `units_enter_ready_by`; reset at end of turn (`:390`). (c) `ReviewFixC2Test.ConfrontAndBushwhack_UnitsPlayedLaterThisTurnEnterReady` (c2:500), `.Bushwhack_DoesNotReadyUnitsAlreadyOnTheBoard` (c2:516). **reviewed OK** (effect-play site: 9.K20).
- `PlayerState::unit_died_this_turn` (`:167`): set in both kill paths (`game_engine.cpp:6885`, `effect_executor.cpp:345`). (c) `ReviewFixC2Test.SpoilsOfWar_DiscountOnlyWhenAnEnemyUnitDiedThisTurn` (c2:564). **reviewed OK.**
- `PlayerState::buff_might_bonus_this_turn` (`:168`): folded as an aura in `recalculateAuras` (`game_engine.cpp:5391-5399`), `bonus * buff_count`. (c) `ReviewFixC2Test.StandUnited_BuffsAreWorthOneMoreThisTurn` (c2:541). **reviewed OK.**
- `PlayerState::hide_ignoring_costs_this_turn` (`:169`): `GameEngine::hideCostFor` (`game_engine.cpp:3000`), CR 811.1.b. (c) `ReviewFixC2Test.GuerillaWarfare_ChoosesTheCardsAndHidingIsFreeThisTurn` (c2:583). **reviewed OK.**
- `TurnState::victory_score_bonus` (`:529`): rebuilt in `recalculateAuras` (`game_engine.cpp:5162`); cards read `GameState::effectiveVictoryScore()`. (c) `SweepFlowTest.VictoryScore_*` (SF:1078-1127), `ReviewFixC2Test.FindYourCenter_UsesTheVictoryScoreInEffect` (c2:1200). **reviewed OK.**
- `TriggerType::WhenAUnitTakesDamage` + `TriggerManager::onDamageDealt` (`trigger_manager.cpp:89-95`): delayed abilities only, both players, the damaged unit as the event object. (c) `ReviewFixC2Test.ImperialDecree_KillsUnitsDamagedAfterItResolves` (c2:409), `.ImperialDecree_KillsAUnitThatSurvivesCombatDamage` (c2:436). **reviewed OK.**
- `DelayedAbility::every_time`: removed, see L343. **reviewed OK.**
- Delayed-ability chain items carry `triggering_subject` (`trigger_manager.cpp:133-137`). (c) the Imperial Decree tests ("kill IT"), `C3Test.RallyTheTroops_BuffsEveryFriendlyUnitPlayedThisTurn`. **reviewed OK.**
- `onScore` fires `WhenIConquer` for spells in the conquerer's trash (`trigger_manager.cpp:989-1001`): any SPELL in that trash whose class `firesOn(WhenIConquer)`. (a) no card name in logic. (c) `ReviewFixC2Test.SuperMegaDeathRocket_ReturnsFromTheTrashWhenYouConquer` (c2:636), `.SuperMegaDeathRocket_NotOnAHoldOrAnOpponentsConquer` (c2:662). Only 250 declares it today (Relentless Pursuit 503 arms a delayed ability and declares no trigger type). **reviewed OK.**
- `pickMoveDest` (`card_helpers.h:1098`): through `Card::pickMode`, destinations from `moveDests` (excludes the current location and a blocked base). (c) `ReviewFixC2Test.RideTheWind_*`, `.Charm_*`, `.Showstopper_*`, `.Stormbringer_*`, `.DragonsRage_*` (c2:678-759). **reviewed OK.**
- `tokenPlayDests` / `destLocation` / `pickTokenDest` (`card_helpers.h:1056-1091`): CR 355.2.a. (c) `ReviewFixC2Test.RecruitTheVanguard_TokensCanGoToAControlledBattlefield` (c2:1225), `.SpriteCallAndDesertsCall_TokenToAControlledBattlefield` (c2:1252). They ignore `blocks_unit_play` / `units_play_base_only` — **DUPLICATE of plans/09 9.G1 (open)**.
- Found next to the reviewed code: two "APPROX:" comments in `trigger_manager.cpp` — `onCombatEnded` (`:764`: "combat designations are already cleared here, so every unit still at the battlefield counts as having been in the combat") and `onUnitDied` (`:877`: "When you kill a unit with a spell" is keyed on a spell being the resolving item, so a unit that dies to that spell's damage in the cleanup after the item cleared is not seen). Neither is in a plan; a documented approximation is not a closure. **OPEN 9.K21.**
- **NOTE (checkpoint / v6 token layout): state added by the `shared:` commits of c2, c3 and c4 that is in neither `src/ml/feature_extractor.cpp` nor `src/ml/token_layout.h`** (grep of `src/ml` and `src/openspiel` for each name: no hit; `kStateFeatureDim` is frozen until stage 5):
  - `PlayerState::units_enter_ready_this_turn` — `bool`, `src/core/game_state.h:166`
  - `PlayerState::unit_died_this_turn` — `bool`, `game_state.h:167`
  - `PlayerState::buff_might_bonus_this_turn` — `int`, `game_state.h:168`
  - `PlayerState::hide_ignoring_costs_this_turn` — `bool`, `game_state.h:169`
  - `PlayerState::units_enter_ready_by` — `std::vector<GameObjectId>`, `game_state.h:184` (derived each aura pass)
  - `TurnState::victory_score_bonus` — `int`, `game_state.h:529` (derived each aura pass)
  - `BattlefieldState::opp_hidden_unrevealable` — `bool[2]`, `game_state.h:493` (derived each aura pass; was a single bool)
  - `DelayedAbility::repeats` — `bool`, `game_state.h:741`; `DelayedAbility::expires_on_turn` sentinels `kThisTurn = -1`, `kUntilFired = -2` (only `delayed_abilities.size()` is a feature, `feature_extractor.cpp:387`)
  - `GameObject::banish_if_would_die_this_turn` — `bool`, `src/core/game_object.h:246`
  - `GameObject::GrantedAbilityRef::exhaust_cost_only` — `bool`, `game_object.h:171` (derived)
  - `GameObject::card_counters["__facedown_play_bf"]`, `["__facedown_play_turn"]` — `int` entries stamped by the engine
  - Listed by other streams, same destination: sweep-flow (`cant_move_this_turn`, `enemy_choices_this_turn`, `grant_units_lone_enemy_bf`, `multi_move_tax`, `conquered_from`), sweep-triggers (`temp_deflect_value`, `pending_death_replacements`, extra `LastKnown` fields, `may_look_at_opp_facedown_turn`, `ChainItem::keyword_ability`), sweep-costs (`RunePool::spell_energy`, `showdown_energy`, `spell_power`, `gear_power`).

### L376 — cards-c2 — "partial by design: Defy 45 cost caps checked at resolution"
**Verdict: MIXED (Defy 45 DONE; Defender of Tomorrow 936 OPEN)**
- Defy 45: DONE — `spells/0045_defy.cpp:33-36` overrides `hasLegalTargets(state, controller, card_db)` → `counterable(state, db)`. Tests `SweepFlowTest.Defy_IsNotOfferedAgainstASpellItCannotCounter`, `.Defy_IsOfferedAgainstACheapSpell`. Ledger wording: `plans/cards/batch-02.md` line 156 (ticked) still says "Defy can still be offered against a too-expensive spell and then does nothing" — stale, reword.
- Defender of Tomorrow 936: OPEN. `legends/0936_defender_of_tomorrow.cpp` has one activation for the two printed abilities ("[1], [E]: Ready a gear." / "[Empowered][>] [1], [E]: Ready 2 gear."). The comment says the Empowered one "strictly dominates". It does not: while Empowered the card always runs `pickTargetPair`, whose second pick cannot be declined, and the list includes enemy gear — with one exhausted gear of your own and one exhausted enemy gear the player must ready the enemy's gear, where the printed first ability would ready only their own. Tracked only as prose in `plans/STATUS.md` line 62 ("needs stage 5"), not as a box; the batch-02 box (line 168) is ticked. Fixable without a vocabulary change (ask which ability through `pickMode` while Empowered); splitting into two `ActivatedAbility` descriptors is the stage-5 version. See 9.K38.

### L378 — cards-c2 — "NEW: delayed-ability expiry compares `expires_on_turn == turn_number`"
**Verdict: DONE**
- Fix: `DelayedAbility::expiresAtEndOf` (`game_state.h:733-736`; default `kThisTurn` expires at the first expiration step, `kUntilFired` never), used by the expiration step (`game_engine.cpp:1304`). Blue Sentinel 649 and Iascylla 612 use `kUntilFired`.
- Test: `SweepTriggersTest.DelayedAbility_DefaultExpiresAtEndOfTurn` (ST:687): default and past-dated are removed, `kUntilFired` and a future date stay. Mutation-tested (build A).

### L379 — cards-c2 — "NEW: `EffectExecutor::recycleCards` on a board permanent"
**Verdict: DONE**
- Fix: `EffectExecutor::recycleCards` (`effect_executor.cpp`, the on-board branch around lines 1206-1216: token ceases to exist, attachments come off, `LeftBoardEvent`). Divine Judgment 244's workaround removed (report; `ReviewFixC2Test.DivineJudgment_EachPlayerChoosesWhatToKeep` still passes per the mutation table).
- Test: `SweepTriggersTest.RecycleCards_APermanentLeavesTheBoardProperly` (ST:710): unit in deck with no location, its gear detached and still on board, token not in the deck, `LeftBoardEvent` for both.

### L381 — cards-c2 — "NEW: hand-built multi-prompt cards cap at 12 prompts"
**Verdict: MIXED (engine bound DONE; tests DUPLICATE of 9.C5; the cards' own caps OPEN)**
- Engine: `ChainManager::stepResolve` `kMaxResumeIterations = 1024` and throws naming the card (`chain_manager.cpp:450-458`); `ChainManagerFixTest.SafetyCap_ACardThatNeverStopsAskingIsAnError`.
- "sweep-verify confirms Fox-Fire / Overt Operation with many units": no such test exists (`tests/`: `ReviewFixC2Test.FoxFire_*` use a handful of units, `ReviewFixC2Test.OvertOperation_PerUnitChoice_AndNoDoubleDebuff` likewise). **DUPLICATE of plans/09 9.C5 (open).**
- OPEN beyond 9.C5 / 9.D3: both cards still carry the cap the row is about, with the stale reason. `spells/0253_fox_fire.cpp:91-92` `kMaxPicks = 12` ("the resolve loop allows 16 re-entries") — with thirteen 0-Might units at a battlefield the thirteenth cannot be chosen. `spells/0153_overt_operation.cpp:88-89` `kMaxAsked = 12` ("later buffed units keep theirs") — the thirteenth buffed unit is never asked. `card_helpers.h:1116` `kSelectManyMax = 10` ("ChainManager allows 16 re-entries") caps "any number" for Bard 540 and Azir, Sovereign 497. 9.D3 names only `kSplitDamageMaxPicks`, `kPickSeveralMaxPicks` and Corrupted Dragon's `kMaxMoves`. See 9.K3, 9.K31.

### L382 — cards-c2 — "fixture note: `testHook_executeIntent` does not run the cleanup"
**Verdict: DUPLICATE**
- plans/09 **9.F1**, open (`- [ ]`, line 98): `fireTriggerThroughEngine` / `resolveThroughEngine` and the 61 direct calls. The `playThroughEngine` half is done (cleanup `2c7d674`).

### L383 — cards-c2 — "trainer note: several spells now ask more or different"
**Verdict: NOTE (trainer developers)**
Source: commit `310bf85` ("Choices that were taken for the player") and the current card files. Prompt order below is from the code as it is now (later streams changed some again; noted). "play target" = the engine's play-time `MakeChoice` for a `needsPlayTimeTarget` spell.
- Fox-Fire 253: play target (a unit at a battlefield, Might ≤ 4), then repeated object prompts "kill a unit at a battlefield (total Might 4 or less)" at the same battlefield within the remaining total, each with a first option value 0 "Stop here"; ends when nothing fits (cap of 12 picks, see 9.K31).
- Ride the Wind 173: play target (friendly unit), then `pickMode` "move it to" (0 = base, 1+i = battlefield i).
- Charm 43: play target (enemy unit), then `pickMode` "move it to".
- Showstopper 265: target on the Play intent (friendly unit in base), then `pickMode` "move it to" (battlefields only).
- Stormbringer 248: `pickTarget` "friendly unit in base", then `pickMode` "battlefield" (battlefields only).
- Dragon's Rage 255: play target (enemy unit), `pickMode` "move it to", then an object prompt "another enemy unit at its destination".
- Zenith Blade 258: `pickTarget` "stun enemy unit" (at a battlefield), yes/no "move a friendly unit to that battlefield?", then (only with more than one candidate) an object prompt for the unit to move.
- Flash 309 (choices at finalization): up to two object prompts "move up to 2 friendly units to base"; the second has a first option value 0 "No more units".
- Salvage 224: the gear is an optional target ("up to one gear") on the Play intent — a no-gear play is offered.
- Overt Operation 153: one yes/no per buffed friendly unit, value 0 "Keep its buff" / 1 "Spend its buff to ready it" (at most 12 units asked, see 9.K31).
- Reinforce 62: object prompt over the units among the top 5 with a first option value 0 "Banish none".
- King's Edict 237: the OPPONENT answers an object prompt "choose one of your units to be killed".
- Cull the Weak 209: each player in turn answers an object prompt "kill one of your units" (caster first).
- Shakedown 33: the targeted unit's CONTROLLER answers value 0 "Take 6 damage" / 1 "Opponent draws 2".
- Meditation 48 (choices at finalization): yes/no "exhaust a friendly unit to draw 2 (else draw 1)?", then `pickTarget` "unit to exhaust".
- Divine Judgment 244: each player answers object prompts "choose 2 units / gear / runes / cards to keep" (four categories, two picks each where there are more than two).
- Recruit the Vanguard 313: one value prompt per Recruit "where to play Recruit n of 4" (0 = base, 1+i = battlefield i); none when the base is the only place.
- Sprite Call 94, Desert's Call 354: `pickMode` for where the token is played; none when the base is the only place.
- Curator of the Sands 932: yes/no "exhaust to ready 2 runes?", then `pickTargetPair` "runes to ready".
- Defender of Tomorrow 936 (choices at activation): `pickTarget` "ready a gear", or `pickTargetPair` "ready 2 gear" while Empowered.
- Thwonk! 363: play target restricted to attacking units (`pickTarget` "stun an attacking unit").
- Piercing Light 346: target sets on the Play intent — first unit at a battlefield, optional second unit anywhere.
- Also changed in the same commit (from "missing effects"): Back to Back 206 (`pickTargetPair` "two friendly units", at finalization); Last Breath 257 (`pickTargetPair` "friendly unit, then enemy unit at a battlefield", at finalization); Icathian Rain 247 (play target, then five more object prompts "deal 2 to a unit (n of 6)"); Guerilla Warfare 260 (up to two object prompts over [Hidden] cards in the trash, first option value 0 "Return no more"); Super Mega Death Rocket! 250 (trigger from the trash on a conquer: yes/no "discard 1 to return it to your hand?", then `pickTarget` "card to discard"); Whirlwind 187 (each player: "you may return a unit to its owner's hand"); Party Favors 71 (opponent: value 0 "Cards" / 1 "Runes" — the two answers now have distinct values).
- Changed again after c2: Bullet Time 263 is now `pickMode` "battlefield" first, then X (9.A10, sweep-verify-cards).
- Not traced to the last branch for every card: the order of prompts above is read from the call sites; for Divine Judgment, Cull the Weak, Whirlwind and Guerilla Warfare only the prompt calls and labels were read, not every re-entry path.

### L385 — cards-c0 — "BLOCKED batch-00 items needing engine work: Svellsongur 382"
**Verdict: MIXED (all sub-items DONE; two test gaps are existing open boxes; ledger ticks needed)**
- Svellsongur 382: DONE — `Card::copiesBearerText()` (`card.h:940`, `gear/0382_svellsongur.cpp:28`), `EffectExecutor::textCopiesOn` (`effect_executor.cpp:979`), `TriggerManager::fireTrigger` (`trigger_manager.cpp:332-336`). Box `plans/cards/batch-00.md` line 48 ticked; 9.C1 ticked. Tests `SweepTriggersTest.Svellsongur_*` (ST:947-1037, six tests).
- Vilemaw's Lair 290: DONE — `EffectExecutor::moveToBase` refuses a blocked move (`effect_executor.cpp:882` "can't move to base"); box line 85 ticked. Test `SweepTriggersTest.MoveToBase_AnEffectCannotMoveAUnitOutOfVilemawsLair` (ST:232).
- Back-Alley Bar 272: DONE — `EffectExecutor::recallToBase` (no `UnitMovedEvent`); box line 137 ticked. Tests `SweepTriggersTest.Recall_DeathReplacementOnAnEffectKillKeepsGearAndIsNotAMove` (ST:178), `.Recall_DeathReplacementOnLethalDamageKeepsGearAndIsNotAMove` (ST:197), `.Soraka_RecallsTheUnitWithoutMovingIt` (ST:215). The missing positive control (a real move out of the Bar gives +1) is **plans/09 9.I4, open**.
- Equip generator offers an unaffordable Equip: DONE — `GameEngine::canPayEquip` (`game_engine.cpp:1341`) gates the generator (`:3430`) and `Card::payEquipAndAttach` the payment (`:1603`); `standardEquip` is gone (`git grep`: no hit). Test `SweepCostsTest.Equip_IsOfferedOnlyWhenItsCostCanBePaid` (SC:513). Ledger: the box for this, `plans/cards/batch-00.md` line 101 "Equip cost (332, 339, ...)", is still `- [ ]` with a BLOCKED sub-bullet — it should be ticked FIXED (sweep-costs) with that test.
- Piltovan Forge 948: DONE — `GameEngine::equipTerms` (`game_engine.cpp:1333`); box line 148 ticked (its `BLOCKED: engine` sub-bullet at line 152 is stale). Test `SweepCostsTest.Equip_FirstGearAbilityDiscountAppliesToTheEquipCost` (SC:547).
- Risen Altar 950: DONE — `Card::activationPowerReductionFor` (`card.cpp:834`), `GameEngine::netActivationCost` (`game_engine.cpp:6621`); box line 154 ticked. Test `SweepCostsTest.EmpowerDiscount_TakesAPowerWhereThereIsNoEnergy` (SC:789) is card-level (calls the virtual, sets `empower_cost_reduction` by hand); the engine-path test is **plans/09 9.J4, open**.

### L386 — cards-c0 — "`shared:` engine changes to review in the sweep"
**Verdict: MIXED (6 changes reviewed OK on card-independence and tests; 2 OPEN, 1 NOTE)**
- `predict` ordering prompt: reviewed OK — `EffectExecutor::predict` (`src/engine/effect_executor.cpp:1806-1911`):
  keep/recycle per card, then `while (kept.size() > 1)` one "Predict: put on top" prompt per kept card beyond the
  first. (a) no card name / id / text in it. (b) CR 437.1.a "Recycles any number of them before putting the rest back
  on top of their Main Deck in any order" — matches; an answer not in `kept` falls back to `kept.front()`.
  (c) `C0Test.CandlelitSanctum_KeptCardsGoBackInTheOrderThePlayerChooses` (`tests/cards/test_review_fix_c0.cpp:342`,
  asserts 3 prompts, default order unchanged, chosen card ends on top), `ReviewFixC1Test.ScryersBloom_KeptCardsGoBackInTheChosenOrder`.
  All Predict users go through this one function (`grep executor.predict(`: 286, 767, 625, 843, 624, 641, 834,
  `keyword_abilities.cpp`), so 834 / 624 / 843 get the prompt with no card code.
  - OPEN (comment only): the function still says "Per CR 436.1, Predict is a PRIVATE look" (Predict is CR 437; 436 is
    Detach) and "For now: random agent recycles all ... Future: proper MakeChoice with multiple selections" above code
    that already asks. See 9.K22.
- `recalculateAuras` grants Equipment keywords before card hooks: reviewed OK — Step 1a' in
  `GameEngine::recalculateAuras` (`src/engine/game_engine.cpp:5213-5257`) pushes `equippedKeywords()` /
  `equippedAssault()` / `equippedShield()` as `AuraEffect`s before the `applyPassiveAura` loop (Step 1b, 5278).
  (a) dispatch through `Card` virtuals only (card names appear in comments only). (b) CR 718.3 / 718.5.g (an Attached
  card's Effect Text abilities are appended to the Top-Most Card's Rules Text) — the unit has the keyword, so a hook
  reading it must see it. (c) `C0Test.KinkouTemple_TankFromAnEquipmentCounts` (`test_review_fix_c0.cpp:489`);
  readers use `hasKeywordInAuraPass` (`card_helpers.h:423`: 946, 764, 638, 63).
- `killObject` asks the dying unit's battlefield for a replacement: reviewed OK for (a) and (c) —
  `EffectExecutor::killObject` (`effect_executor.cpp:273-295`) calls the battlefield card's
  `hasReplacementEffect()` / `applyReplacement(rctx, target)` for the dying unit's controller, same block as
  `GameEngine::killUnit` (`game_engine.cpp:6817-6837`). No card id in either. Test:
  `C0Test.AltarOfBlood_IsAskedForASpellKillDuringCombatToo` (`test_review_fix_c0.cpp:706`: unit recalled, exhausted,
  3 Power spent).
  - OPEN for (b): CR 372 — "If more than one Replacement Effect applies to the same event ... the controller of the
    object being acted on determines the order". Both kill paths apply a fixed order (the unit's own pending
    replacement → `banish_if_would_die_this_turn` → battlefield card → the controller's cards in
    `state_.objects` order, an `unordered_map`, `game_state.h:764`) and stop at the first that applies; the controller
    is never asked and the order among their cards is hash order (Zhonya's 77 vs Guardian Angel 374 vs The Boss 264
    vs Altar of Blood 762 on one death). No plan box covers it (`grep -i "replacement.*order" plans/` → nothing).
    See 9.K23.
- Academy's granted Repeat charges Power: reviewed OK — `GameEngine::executePlaySpell`
  (`game_engine.cpp:2231-2243`): `repeat_costs.push_back({base.energy_cost, base.power_cost, maskOf(base.domains), 0})`,
  read from `card_db_` for whatever the spell is (no card id; comment names 772 / 401 / 708). CR 820 (Repeat cost is
  the stated cost; "equal to its base cost" = Energy and Power). Tests:
  `C0Test.TheAcademy_GrantedRepeatCostsTheWholeBaseCost` (`test_review_fix_c0.cpp:674`: not offered with one [O],
  offered and both paid with two), `SweepCostsTest.GrantedRepeatEqualToItsCost_ChargesThePowerToo`
  (`tests/cards/test_sweep_costs.cpp:487`).
- `TriggerType::WhenILeaveTheBoard`: reviewed OK — enum `src/effects/effect_types.h:197`; dispatched from
  `TriggerManager::onLeftBoard` (`trigger_manager.cpp:952-955`) through `cardFiresOn(...)` → `fireTrigger`; no card
  id. Only user: Treasure Trove 186. Test: `C0Test.TreasureTrove_TriggersHoweverItLeavesTheBoard`
  (`test_review_fix_c0.cpp:1084`: bounce draws and channels; kill by its own ability triggers once, not twice).
- `PlayerState::chosen_champion`: reviewed OK — `game_state.h:140`, set in `GameEngine` setup
  (`game_engine.cpp:430`), read only by Hallowed Tomb 276. Test:
  `C0Test.HallowedTomb_ReturnsTheChosenChampionAndCanBeDeclined` (`test_review_fix_c0.cpp:244`).
  - NOTE (checkpoint / v6 token layout): `chosen_champion` is not in `src/ml/feature_extractor.cpp` nor in
    `src/openspiel` (`grep chosen_champion src/ml src/openspiel` → nothing). The layout is frozen until stage 5; it
    joins the list of new state fields the checkpoint stream decides on (with L302's five fields, sweep-triggers'
    `temp_deflect_value`, `pending_death_replacements`, `may_look_at_opp_facedown_turn`, and sweep-costs' four
    restricted pools).

### L389 — cards-c0 — "NEW: `EffectExecutor::killObject` `death_replacement_recall_pending` block detaches gear"
**Verdict: DONE**
- Fixed by sweep-triggers: `killObject` → `EffectExecutor::applyOwnDeathReplacement` (`effect_executor.cpp:921-951`)
  → `healExhaustRecall` → `recallToBase` (899-912), which never detaches and calls `carryAttachments` ("CR 719.3.a").
  No `detachAllGear` on that path. Test: `SweepTriggersTest.Recall_DeathReplacementOnAnEffectKillKeepsGearAndIsNotAMove`
  (`tests/test_review_fix_sweep_triggers.cpp:178`: gear still attached, gear at base, `moves_this_turn == 0`, no
  Back-Alley Bar +1).

### L390 — cards-c0 — "NEW: Equip generator and `payAndQueueAbility` skip a gear that is already attached"
**Verdict: DONE (NOT A DEFECT, pinned and clarified)**
- CR 718.2 "While in this state, the card's printed Rules Text is Inactive", 721.2 "Inactive Abilities ... cannot be
  activated"; only Weaponmaster lifts it (CR 821.1.c "Necessary portions of its Rules Text are no longer Inactive").
  So an attached Equipment's own Equip is not available.
- Clarified at the code sites: `Card::canPayEquip` (`src/cards/card.cpp:865-869`, "Attached: its Rules Text, the Equip
  ability included, is Inactive (CR 718.2, 721.2). Weaponmaster lifts that (CR 821.1.c)" — `terms.even_if_attached`),
  the generator (`game_engine.cpp:3411-3415`) and `payAndQueueAbility` (`game_engine.cpp:1583-1592`).
- Pin: `SweepCostsTest.Equip_AttachedEquipmentIsNotOfferedAgain` (`tests/cards/test_sweep_costs.cpp:532`: two offers
  before, none after attaching). The Weaponmaster side: `SweepCostsTest.Weaponmaster_MovesAnAttachedEquipmentAndPaysANonResourceCost`.

### L392 — cards-c0 — "approximations left documented: Solari Shrine 72 (killer not tracked)"
**Verdict: DONE (all three)**
- Solari Shrine 72: DONE — `triggerCondition` in `src/cards/gear/0072_solari_shrine.cpp:24-30` requires
  `lk.valid && lk.stunned && lk.killed_by == controller`; `LastKnown::killed_by` (`src/core/game_object.h:107`) is
  set in `EffectExecutor::snapshotLastKnown` (`effect_executor.cpp:226`, `killerOf`) on both kill paths. Tests:
  `SweepTriggersTest.SolariShrine_TriggersOnlyWhenItsControllerKilledTheUnit`,
  `.SolariShrine_DoesNotTriggerWhenTheOpponentKilledTheirOwnUnit` (`tests/test_review_fix_sweep_triggers.cpp:450, 466`),
  `C0Test.SolariShrine_OnlyAStunnedEnemyUnitsDeathTriggers`. Plan 07 7.35 is `[x]` and names it.
- Unlicensed Armory 23: DONE — the ability pushes `{cardDefId(), controller, source, turn}` onto the unit's
  `pending_death_replacements` (`src/cards/gear/0023_unlicensed_armory.cpp:57-58`);
  `EffectExecutor::applyOwnDeathReplacement` (`effect_executor.cpp:934-950`) looks the card up by def id
  (`cardFor(r.card)`), so no Armory needs to be in play, and drops entries from an earlier turn. Tests:
  `SweepTriggersTest.UnlicensedArmory_ReplacementAppliesWithTheGearGone` (source `kInvalidId`, unit recalled, [C]
  paid, entry used up), `.UnlicensedArmory_ReplacementEndsWithItsTurn`; activation path
  `C0Test.UnlicensedArmory_SavingTheUnitCostsPowerAndIsOptional` (9.B4, corrected). batch-00 line 117 is `[x]`.
- Baited Hook 242 `[C]` domain: DONE (confirmed Order) — def `d.domains = {Domain::Order}`, cost
  `.power_domain = Domain::Order` (`src/cards/gear/0242_baited_hook.cpp:27, 132`); CR 135.2.e.6 "[C] ... one Power of
  this card's Domain". Test: `C0Test.BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit` asserts the Order Power
  is spent ("[C] = Order"). batch-08 line 10 is `[x]` and records that the 02 item 2.27 note ("Chaos") was wrong.

### L393 — integrity — "5.12 warnings carry-over (`-Wall -Wextra`): 120 sites"
**Verdict: DUPLICATE**
- plans/09 9.C6 (open, line 72: tree must build warning-free; owners cleanup / sweep-verify-cards / sweep-verify),
  9.F2 (open, line 99: three engine warnings, `chain_manager.cpp` `finalized_something`, `game_engine.cpp`
  `greedyCascade`, `trigger_manager.cpp` `bf_loc`), 9.F3 (open, line 100: 18 unused parameters in `card.h`).
  9.F4 (libstdc++ warnings in card code) is `[x]` and says "0 warnings in card files; 3 left in the tree, all
  engine (9.F2)". Same finding as L297. UNDETERMINED whether `card.h` still warns: needs a build (not allowed here).

### L401 — io-cli — "fair-play mode now forwards no `log` lines"
**Verdict: DONE**
- `PlayServer::broadcastLog(level, message, LogAudience)` (`src/io/play_server.cpp:1271-1302`): with
  `!config_.reveal_hidden` a line is sent when every human seat may read it; `src/main.cpp:830` passes `e.audience`.
  Test: `PlayServerProtocol.FairPlayFramesWithholdOpponentHandAndDeckOrder` (`tests/test_review_fix_io_cli.cpp:509`;
  asserts "public" and "P1 only" lines arrive, "P2 only" / "nobody" / untagged do not, and no engine line tagged
  not-for-P1 is received).

### L402 — io-cli — "NEW: `LogEvent` (`src/core/events.h`) has no player / visibility tag"
**Verdict: DONE**
- `LogAudience` and `LogEvent::audience` (`src/core/events.h:258-283`), `EventBus::logTracePrivate` / `logTraceFor`
  / `logFor` (449-455). Tests: `SweepTriggersTest.LogEvent_ADrawIsVisibleOnlyToThePlayerWhoDrew`,
  `SweepTriggersLogAudience.SaysExactlyWhichPlayersMayRead` (`tests/test_review_fix_sweep_triggers.cpp:898, 918`),
  `SweepTriggersTest.PromptLines_NamingHiddenCardsArePrivate` (9.D11).

### L403 — io-cli — "NEW: god-mode edits do not recompute the pending decision's legal actions"
**Verdict: DUPLICATE**
- Main-phase half is done (row says so). The "sweep-verify decides" half is plans/09 9.C3, open (line 69: recompute
  at a nested decision, or show with a test that a stale intent is refused) — owner sweep-verify.

### L407 — io-cli — "not verified in a browser: page JS for `decision_id`"
**Verdict: NOTE (final report)**
- Nothing in the tree runs the page: `tests/test_review_fix_io_cli.cpp` talks to `PlayServer` over a socket client
  (JSON frames); its only use of the page is `buildPlayIndexHtml(...)` compared as a string (line 895), no
  JavaScript is executed. So, for the final report, still unverified: the page JS that echoes `decision_id`, the
  token query on the WebSocket URL, per-unit owner / controller shown on moves, hidden-zone counts as rendered.
  No ThreadSanitizer run either: `grep -i "sanitize\|tsan" CMakeLists.txt src/openspiel/CMakeLists.txt` finds no
  such option. A manual browser pass and one TSAN build are the two things left to ask the user about.

### L412 — search — "`CLAUDE.md` still describes the old replay model"
**Verdict: DONE**
- `CLAUDE.md` has the bullet "Action history and replay (OpenSpiel)" (history tokens, `findHistoryToken`, lazy `Clone()`, `isResumableDecision`, the mismatch check, `syncToHistory`) and its Build section names `riftbound_openspiel_tests` as a separate binary to build and run. Checked against the code: `openspiel::historyToken` / `findHistoryToken` (`src/openspiel/action_vocab.h`), `GameEngine::isResumableDecision` and the check at the end of `resumeFromSnapshot` (`src/engine/game_engine.cpp`), `RiftboundState::ensureLive` / `syncToHistory` (`src/openspiel/riftbound_state.cpp`), the target in `src/openspiel/CMakeLists.txt`. Every backticked identifier in `CLAUDE.md` was grepped in `src/`, `tests/`, `scripts/` and `CMakeLists.txt`: none is missing.
- Documentation only; no test applies. (This stream's own CLAUDE.md additions are plans/09 9.D12.)

### L414 — search — "behaviour changes for other streams: `GameEngine::applyChoice` throws"
**Verdict: NOTE (trainer developers, in the final report) — every claim verified in the code**
- `const StepResult& GameEngine::applyChoice(int legal_index)` (`src/engine/game_engine.h:175`): a bad index throws
  `std::out_of_range("GameEngine::applyChoice: legal_index N is not one of the M legal actions.")`
  (`game_engine.cpp:374-380`); called before `beginGame` it throws `std::logic_error`; on a finished game it returns
  the Done step. An engine failure is rethrown here and by every later `applyChoice` / `currentStep`.
- `RiftboundState` apply path (`src/openspiel/riftbound_state.cpp:432-447`): an action id that no legal Intent
  encodes to throws `std::invalid_argument("RiftboundState: action N is not legal in this state.")`; on a finished
  game `std::logic_error`. Neither applies action 0 any more.
- `std::vector<int64_t> RiftboundState::actionHistory() const` (`riftbound_state.h:171`): by value; one vocab slot
  per decision, -1 where the Intent had no slot.
- `static std::unique_ptr<RiftboundState> makeFromSnapshot(game, uint64_t engine_seed, const GameState& snap_state,
  StepResult snap_step, GameResult snap_result, uint64_t rng_seed)` (`riftbound_state.h:112-118`): `rng_seed` is the
  new last parameter.
- Also true now (perf-search, L308): `beginGame` / `resumeFromSnapshot` / `currentStep` / `applyChoice` return
  `const StepResult&` (`game_engine.h:113, 145, 164, 175`) and `RiftboundState::legalIntents()` returns
  `const std::vector<Intent>&` (`riftbound_state.h:207`) — views the next applied choice overwrites.
  `resumeFromSnapshot(GameState, const std::mt19937_64& rng_state, const StepResult& snapshot_step)` throws
  `std::invalid_argument` when the snapshot is not at a resumable decision (`game_engine.cpp:266-271`) and
  `std::logic_error` when the resumed engine does not reach the snapshotted decision (header comment, 141-144).
- `InfoSetId` / `ObservationString` values changed (1.13) and the feature layout did not: not re-derived here
  (UNDETERMINED by reading alone; `kStateFeatureDim = 6188` is unchanged in `CLAUDE.md` and the trainer survey).

### L417 — search — "NEW (by design, note only): MCTS agents fall back to a uniformly random"
**Verdict: NOTE (final report / trainer developers) — verified**
- `LiveMirror::rootFor` (`src/agents/mcts_agent.cpp:243-280`): if `syncToHistory` and then `rebuildFromHistory`
  fail, `unreproducible_ = true` for the rest of the game ("the seed we were given is not the engine's, or the state
  was edited outside the engine (the play UI's god mode)"). After that the condition is
  `if (GameEngine::isResumableDecision(state, legal_actions))` → search from an approximate snapshot
  (`makeFromSnapshot(..., rng_())`: right board, guessed future shuffles); otherwise `return nullptr`, and the caller
  uses `LiveMirror::unsearched` (284-287): `std::uniform_int_distribution<size_t> pick(0, legal_actions.size() - 1)`.
  So the uniform pick happens only when the game is unreproducible AND the decision is not a top-level main-phase
  one (mulligan, chain priority, showdown, combat, payment, prompts).
- Covered by `MctsAgentTest.StillPlaysLegalMovesWhenGivenTheWrongEngineSeed`
  (`src/openspiel/openspiel_tests.cpp:769`, in `riftbound_openspiel_tests`).

### L418 — search — "1.3: `ResumedEngineKeepsTheMainPhaseActionCount` not confirmed to fail"
**Verdict: DONE (run)**
- Shown red by mutation: in `GameEngine::runTurnFromPhase` (`src/engine/game_engine.cpp`) the resumed main-phase loop is entered with `mainPhaseLoop(resume_at == TurnPhase::MainPhase ? turn_decisions_pN : 0)`; with that argument replaced by `mainPhaseLoop(0)` (the pre-fix behaviour: the count restarts) `StepApiTest.ResumedEngineKeepsTheMainPhaseActionCount` fails at `tests/test_step_api.cpp:324` for both positions it checks ("seed 61 / seed 62: the resumed engine restarted the per-turn action count instead of continuing it"). With the line restored it passes (full suite 2500 / 2500). The engine file was restored from a scratch copy; `git diff -- src/engine` is empty.
- The test needed no change.

### L419 — engine-auras-costs — "5.4 remainder: three text matches left in the engine"
**Verdict: DONE**
- plans/05 5.4 is `[x]` (line 16). `parseRepeatCost` / `payRepeatCost` no longer exist
  (`grep -rn parseRepeatCost src` → nothing); `executePlaySpell` reads `Card::repeatCosts()`
  (`game_engine.cpp:2219-2225`). `grep -rn "ability_text\|effect_text" src/engine src/core` finds comments only.
  Tests: `C3Test.SquareUp_RepeatIsPaidWithADiscard`, `SweepCostsTest.CurtainCall_EachRepeatCostIsOfferedOnceAndRunsAnotherMode`,
  `JhinDeckTest.RepeatCost_*`; play locations / battlefield restrictions: `SweepFlowTest.*` (sweep-flow item 1).

### L420 — engine-auras-costs — "8.4 remainder: Weaponmaster for Sand Soldier tokens"
**Verdict: DONE**
- plans/08 8.4 is `[x]` (line 61). `src/engine/keyword_abilities.{h,cpp}` + `TriggerManager::queueKeywordAbilities`
  (called from `onTokenCreated`, `trigger_manager.cpp:457-462`, after refreshing continuous effects); no card id in
  it. Tests: `SweepTriggersTest.Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed`, `.Weaponmaster_IsAMay`
  (`tests/test_review_fix_sweep_triggers.cpp:551, 566`), `SweepCostsTest.SandSoldier_HasWeaponmasterFromTheEmperor`.

### L422 — engine-auras-costs — "2.2 contract: `applyPassiveAura` is called once per (card, controller)"
**Verdict: DUPLICATE**
- The "sweep-verify ticks" part is plans/08 **S3**, still `- [ ]` (line 208). Its last sub-bullet: "LEFT, with
  sweep-costs: 407 Ornn, Forge God and 950 Risen Altar. Nothing else." sweep-costs has merged, so the box waits only
  on that re-read — owner sweep-verify-cards. (9.I5 "Soraka 550: no test with two copies" is a separate open box.)

### L424 — engine-auras-costs — "`EffectExecutor::payAnyPower` duplicates the rule in `payAdditionalCost`"
**Verdict: DUPLICATE (fix in, test owed)**
- Fix is in: `EffectExecutor::payAnyPower` (`effect_executor.cpp:494-499`) is one line,
  `return resource::pay(state_, events_, player, 0, amount, Domain::Count);` — the rule lives once, in
  `src/engine/resource_payment.h`.
- No test of its own: sweep-costs' report says so, and that is plans/09 **9.J4** (open, line 142: "`EffectExecutor::
  payAnyPower` after it was made to delegate") — owner sweep-costs (follow-up). Not done until 9.J4 is.

### L425 — engine-auras-costs — "NEW: `killUnit` `death_replacement_recall_pending` block (Tactical Retreat)"
**Verdict: DONE**
- `GameEngine::killUnit` (`game_engine.cpp:6800-6806`) calls `effect_executor_->applyOwnDeathReplacement(unit_id)`
  and returns; no `detachAllGear` before the recall (the only one left in `killUnit` is after a real death, 6902).
  `recallToBase` carries the attachments. Test:
  `SweepTriggersTest.Recall_DeathReplacementOnLethalDamageKeepsGearAndIsNotAMove`
  (`tests/test_review_fix_sweep_triggers.cpp:197`).

### L427 — engine-auras-costs — "NEW: `TriggerManager::onCardPlayed` — Vision fires once however many instances"
**Verdict: MIXED**
- Vision once per instance: DONE — `TriggerManager::onCardPlayed` (`trigger_manager.cpp:540-551`) calls
  `queueKeywordAbilities(e.object, e.player, Keyword::Vision, false)`, one chain item per instance (CR 817.2), each
  asking its controller (817.2.a). Tests: `SweepTriggersTest.Vision_EachInstanceTriggersSeparately` (two Gemcraft
  Seers → 2 prompts), `.Vision_TheControllerChoosesWhetherToRecycle`, `.Forecaster_HasOneInstanceOfVision`.
- Full aura pass per permanent played (perf): OPEN — still there and now also per token:
  `effect_executor_->recalculateContinuousEffects()` at `trigger_manager.cpp:548` (every permanent played) and
  `:460` (every token unit / gear created). The comments say why (the permanent arrives after the last aura pass
  and may have the keyword from a continuous effect). Neither measured nor in plans/06 (6.2 is about text parsing,
  which is gone). See 9.K24.

### L429 — engine-auras-costs — "NEW: `CLAUDE.md` stale on three points"
**Verdict: MIXED (three points DONE; the header comment DUPLICATE)**
- killUnit replacements described as text matching: DONE — `CLAUDE.md` pitfall "Death replacement effects are card-owned, not text-matched" (`Card::hasReplacementEffect()` / `applyReplacement`); matches `GameEngine::killUnit` and `EffectExecutor::killObject`.
- power recycled from exhausted runes only: DONE — "Cost payment" now says "exhausts runes for energy (agent-chosen), then recycles runes for power (agent-chosen)" and names `engine/resource_payment.h` for every other payment.
- the once-per-(card, controller) aura contract: DONE — "Aura system" bullet (`Card::applyPassiveAura` is called ONCE per (card, controller) ... `forEachSelfOnce`); matches `GameEngine::recalculateAuras` step 1b.
- `game_engine.h` comment "cleanup has no agent-query sites": DUPLICATE — plans/05 **5.14** (open; see L296: still at `game_engine.h`, with "stubbed" and "worker thread").

### L430 — engine-auras-costs — "unplanned behaviour change to review: \"to a minimum of [N]\""
**Verdict: DONE (NOT A DEFECT, pinned and clarified) — the behaviour is right**
- CR 356.4.e: "If a discount applies a minimum cost, that minimum applies only to that discount." Its example
  (Eager Apprentice "reduced by [1], to a minimum of [1]" + Sky Splitter) ends at Energy cost 0, so a minimum is a
  limit on that one discount, not a floor on the card: a card already cheaper than N is not raised, and another
  discount may take it below N.
- Code: `netCardCost` body in `src/engine/game_engine.cpp:6193-6235` — discounts with `min_cost > 0` go into
  `limited` and are applied first as `min(energy_needed, max(minimum, energy_needed - reduction))` (never raises),
  then the unlimited ones, then `max(0, ...)`. The comment at 6195-6197 cites CR 356.4.e and says "never raises a
  cost that is already lower"; 6231 cites 356.4.d.1 for the order (limited first is the player's best order, as in
  the CR example).
- Pin: `AurasCostsTest.MinimumCost_LimitsTheReductionOnly` (`tests/test_review_fix_auras_costs.cpp:703`: [2] with
  two limited discounts stays at the floor of [1]; a [0] card stays [0] — "A card that already costs less than the
  minimum is not made dearer").

### L431 — helpers — "3.1 card work: Blast Cone 695"
**Verdict: DONE**
- plans/03-cards.md line 12 is ticked: "- [x] **3.1 Helper ordering bug in `Card`** [verified] — FIXED: helper flags
  and all seven cards verified in the code by sweep-verify-cards (745, 258, 490, 695, 263, 760, 607: no state change
  between two helper calls); test: ChoiceHelperTest.EveryOrderOfTheHelpersAsksEachOnceAndKeepsEachAnswer, ...".

### L432 — helpers — "3.24 card work: 751, 41 → `splitDamageAmong`"
**Verdict: DONE**
- plans/03-cards.md line 134 is ticked: "- [x] **3.24 Copy-pasted helpers** [confirmed] — FIXED: verified in the code
  by sweep-verify-cards — no local `splitDamage` / `kSplit*` / `discardThenAct` left, and no `rune_deck.insert`
  outside `card_helpers.h` ...; test: VerifyCardsTest.AlphaStrike_ThroughTheEngine_..., ...".

### L433 — helpers — "7.30 card work: 927, 894, 937, 892, 935"
**Verdict: DONE**
- plans/07-cards-second-pass.md line 253 is ticked: "- [x] **7.30 Hand-rolled enemy picks bypass Deflect and
  chosen-object bookkeeping** [plausible] — FIXED: 927, 894, 937, 892, 935 verified in the code by
  sweep-verify-cards ...; test: C3Test.ShurikenFlip_PaysDeflectForTheEnemyUnitItChooses, ...".

### L435 — helpers — "Weaponmaster 7.7: Equip cost still not charged"
**Verdict: DONE**
- plans/07 line 147 is ticked (`- [x] **7.7 Costs never charged**`). Equip-cost descriptor: `Card::equipCost()`,
  `canPayEquipExtraCost` / `payEquipExtraCost`, `Card::canPayEquip`, `Card::payEquipAndAttach`
  (`src/cards/card.h:703-728`, `card.cpp:859-905`). `WeaponmasterUnit::weaponmasterChoices` / `weaponmasterEquip`
  (`src/cards/units/weaponmaster_base.h:47, 70`) pay through it and attach with `ctx.executor.attachGear`.
- Own copies gone: `SentinelAdept : public WeaponmasterUnit` (`units/0331_sentinel_adept.cpp:17`),
  `YoneBlademaster : public WeaponmasterUnit` (`units/0544_yone_blademaster.cpp:18`).
- Tests: `SweepCostsTest.Weaponmaster_ChargesTheEquipCostLessOnePower`,
  `.Weaponmaster_MovesAnAttachedEquipmentAndPaysANonResourceCost`,
  `.Weaponmaster_TheMayAndTheEquipmentAreChosenAtFinalization` (`tests/cards/test_sweep_costs.cpp:583, 600, 992`),
  `RulesCards2Test.RulesCards2_Jax_WeaponmasterChargesANonResourceEquipCost`.

### L437 — helpers — "NEW: `payEnergy` / `payableEnergy` in `card_helpers.h` count and exhaust any rune"
**Verdict: OPEN (fix is in, no test)**
- Fix: `payableEnergy` / `payEnergy` (`src/cards/card_helpers.h:67-77`) delegate to `resource::payableEnergy` /
  `resource::pay`, which use `runesInBase` (`src/engine/resource_payment.h:63-73`: `*obj.location == base`, then
  `std::sort` — "a payment must not depend on its iteration order").
- Test: none found. `grep -rn "payEnergy\|payableEnergy\|resource::" tests` → nothing; no test places a rune outside
  its controller's base (possible until the next cleanup, CR 323.6) or checks the lowest-id order. sweep-costs'
  report lists the change without a test. See 9.K27.

### L440 — chain-triggers — "7.1: 26 cards to convert per the rule under the item"
**Verdict: DONE**
- plans/07 line 28 is ticked: "- [x] **7.1 26 cards whose triggered effect never happens** ... — FIXED: all 26 files
  read in full and verified in the code by sweep-verify-cards against the 7-step rule ...; `kKnownOffenders` is
  empty".

### L441 — chain-triggers — "8.9 last bullet: Square Up (\"Discard 1\") and Curtain Call"
**Verdict: DONE**
- plans/08 line 108 is ticked (`- [x] **8.9 Repeat**`, last sub-bullet "sweep-costs: bullet 2 FIXED").
  `Card::repeatCosts()` (`src/cards/card.h:857`), overridden in `spells/0579_square_up.cpp:21` and
  `spells/0743_curtain_call.cpp:28`, read by `GameEngine::executePlaySpell` (`game_engine.cpp:2219-2225`).
- Tests: `C3Test.SquareUp_RepeatIsPaidWithADiscard` (`tests/cards/test_review_fix_c3.cpp:458`: discard paid, Assault
  8; with nothing to discard no offer), `SweepCostsTest.CurtainCall_EachRepeatCostIsOfferedOnceAndRunsAnotherMode`.
- Ledger remark (not this row): plans/09 **9.B1** is still `- [ ]` although
  `C3Test.SquareUp_DoesNotForceADiscard` was rewritten by sweep-costs (now expects exactly one prompt, the Repeat
  offer, declined) — it is ready to be ticked by whoever owns section B.

### L444 — chain-triggers — "8.16 bullet \"spell that banishes itself is also filed in trash\""
**Verdict: DUPLICATE**
- plans/08 **8.16** is still `- [ ]` (line 141) — owner sweep-verify-cards. The bullet this row is about is fixed:
  `ChainManager` (`src/engine/chain_manager.cpp:584-589`): `if (spell_obj.zone != ZoneType::Chain)` → "The spell's
  own effect already moved it (\"Banish this.\"): ... not filed a second time". Tests:
  `C3Test.ArcaneShift_ReplaysTheUnitOnceAndBanishesItself`, `ReviewFixC2Test.TimeWarp_IsBanishedAndNotAlsoFiledInTheTrash`.
- For the tick: every other 8.16 bullet now has a fix and a test — 670 (`gainXp` is the only `xp +=` left in
  `src/cards`, `card_helpers.h:1137`; `SweepTriggersTest.GainXp_IsCountedForTheTurn`), 809
  (`SweepCostsTest.EndlessRiches_*`), 931 (`SweepCostsTest.DeathMark_FlowPaysTwoAnyPowerFromTwoDomains`), 875
  (`SweepTriggersTest.TemporaryDeflect_EndsAtTheExpirationStep`), 635 / 933
  (`C3Test.SiphoningStrike_DeathWatchExpiresWithTheTurn`, `SweepTriggersTest.DelayedAbility_DefaultExpiresAtEndOfTurn`),
  and the c0 / c1 / c2 / c3 sub-bullets. Nothing found that still blocks it.

### L445 — chain-triggers — "2.11 root cause: the play gate should check card cost plus Deflect"
**Verdict: DONE**
- `GameEngine::canAffordChoosing(player, card_id, targets)` (`src/engine/game_engine.cpp:1910-1939`): sums
  `deflectCostFor` over the targets (skipped when `ignoresDeflect()`), then `canAffordWith(player, card_id, deflect,
  adjust)` — cost and Deflect together; called for the play-time target list in `executePlaySpell` (2105) and by
  the `affordable` gate of the play generator at 4040 (which checks each target set, 4113-4122).
  Test: `SweepCostsTest.PlayGate_ATargetWhoseDeflectCannotBePaidWithTheCostIsNotOffered`
  (`tests/cards/test_sweep_costs.cpp:921`). plans/02 2.11 is `[x]`.

### L446 — chain-triggers — "NEW: `ChainManager::addSpell` / `addAbility` charge Deflect without"
**Verdict: DONE**
- `ChainManager::payDeflectForChoices` (`src/engine/chain_manager.cpp:23-47`), called by both `addSpell` (65) and
  `addAbility` (141), returns before charging when `card->ignoresDeflect()`. Test:
  `SweepTriggersTest.IgnoresDeflect_IsHonouredForATargetChosenAsTheSpellIsPlayed`
  (`tests/test_review_fix_sweep_triggers.cpp:875`; strengthened and shown red in the 9.D5 mutation run).

### L447 — chain-triggers — "NEW: `TriggerManager::onUnitDied` queues a unit's own death ability directly"
**Verdict: DONE**
- `TriggerManager::queueOwnDeathTrigger` (`src/engine/trigger_manager.cpp:910-923`) goes through `fireTrigger(object,
  controller, 0, TriggerType::WhenIDie, object)` — the one path that applies `triggerCondition` and the [Temporary]
  suppression — for units and killed gear alike (`onLeftBoard`, 958). Test:
  `SweepTriggersTest.OwnDeathTrigger_IsSuppressedForATemporaryUnitAtLeBlancsBattlefield`
  (`tests/test_review_fix_sweep_triggers.cpp:357`). It pins the LeBlanc half; the `triggerCondition` half has no
  test of its own but is the same single change (the test is red if the routing through `fireTrigger` is undone).

### L449 — chain-triggers — "NEW: Battle Mistress 517 triggers once per dead enemy unit"
**Verdict: DONE**
- "superseded" pointed at the cards-c1 row (L353); the fix is in: `Card::triggersOncePerBatch` (`src/cards/card.h:930`),
  overridden in `legends/0517_battle_mistress.cpp:27`, honoured in `TriggerManager::fireTrigger`
  (`trigger_manager.cpp:310`). Tests: `SweepTriggersTest.BattleMistress_TriggersOnceForABatchOfDeaths` (two lethal
  deaths → one chain item), `.KillAllEffect_UnitsItKillsDiedTogether` (9.D2, effect kills in a `DeathBatch`).

### L454 — executor — "8.14: gear attached to an off-board unit (`attachFree`, hand-rolled attaches)"
**Verdict: MIXED**
- `attachFree` / `attachEquipped`: DONE — both are one call to `ctx.executor.attachGear` (`src/cards/card_helpers.h:110-118`);
  `EffectExecutor::attachGear` (`effect_executor.cpp:953-977`) requires gear and unit on the board, detaches from a
  previous bearer, sets `is_rules_text_inactive`. Test: `SweepTriggersTest.AttachGear_RefusesAUnitThatIsNotOnTheBoard`
  (`tests/test_review_fix_sweep_triggers.cpp:762`), `SweepCostsTest.QuickDraw_AttachesThroughTheExecutorsAttachPath`.
  plans/08 8.14 is `[x]`.
- Hand-rolled attaches: OPEN — one is left. `spells/0334_angle_shot.cpp:72-81` writes `g.attached_to = unit;
  u.attachments.push_back(equip); ...` itself after only `objectExists(unit) && objectExists(equip)`: no on-board
  check on either end, `is_rules_text_inactive` not set (so the gear's own printed triggers keep firing while
  attached — the L358 defect by another door). `grep -rn "attached_to = \|attachments.push_back" src/cards src/engine`
  finds only this site outside `effect_executor.cpp`. See 9.K34.

### L457 — executor — "NEW: `recycle_self` cost and the mulligan recycle in `game_engine.cpp`"
**Verdict: DONE**
- `EffectExecutor::announceRecycledToMainDeck` (`effect_executor.cpp:1010-1014`) is called by the `recycle_self` cost
  block of `GameEngine::payAndQueueAbility` (`game_engine.cpp:1695-1715`) and after the mulligan's
  `putOnBottomTogether` (`game_engine.cpp:2877-2878`, once for the set).
- Tests (9.D4, `[x]`): `SweepTriggersTest.Mulligan_AnnouncesTheRecycleOnce`
  (`tests/test_review_fix_sweep_triggers.cpp:1089`),
  `PlayCombatFixTest.RecycleSelfCost_GoesToTheOwnersDeckAndAnnouncesLeavingTheBoard` (asserts
  `recycled_announced == 1`) and `..._TokenCeasesToExistInstead` (`tests/test_review_fix_play_combat.cpp:759, 792`).

### L458 — executor — "NEW: `EffectExecutor::moveToBase` is used for recalls too"
**Verdict: MIXED (primitive DONE, caller audit leaves 2 OPEN)**
- Recall primitive: DONE — `EffectExecutor::recallToBase` (`effect_executor.cpp:899-912`): no `moves_this_turn`, no
  `UnitMovedEvent`, not stopped by `moveToBaseBlocked` (CR 458, 458.3), carries attachments (719.3.a), emits
  `"recalled"`. `moveToBase` (856-897) is now the move only and is refused where moves to base are blocked. Tests:
  `SweepTriggersTest.Recall_DeathReplacementOnAnEffectKillKeepsGearAndIsNotAMove`, `..._OnLethalDamage...`,
  `.Soraka_RecallsTheUnitWithoutMovingIt`, `.MoveToBase_AnEffectCannotMoveAUnitOutOfVilemawsLair`,
  `.MinotaurReckoner_StopsEffectMovesToBase`.
- Caller audit, cards: DONE — every card whose text says "recall" calls `recallToBase` / `healExhaustRecall` or sets
  `death_replacement_recall_pending` (23, 77, 203, 264, 318, 374, 516, 550, 702, 737, 762, 920; Symbol of the
  Solari 227 through the combat-tie branch, `game_engine.cpp:4878-4880`). The remaining `moveToBase` callers print
  "move" (The Syren 184; Voidreaver 787 "Move an exhausted friendly unit from a battlefield to its base").
  - OPEN (wording): Voidreaver 787 labels that move "Voidreaver: recall" in its prompt and trace
    (`legends/0787_voidreaver.cpp:76, 87`) — now that recall is a different primitive the label says the wrong thing.
    See 9.K37.
- Caller audit, engine: OPEN — `GameEngine::recallMisplacedObjects` (`game_engine.cpp:5755-5776`, CR 323.6) is a
  second recall implementation: it writes `obj.location = home` itself, so a unit recalled from another player's
  base leaves its attached gear's `location` behind (CR 719.3.a; `carryAttachments` is only called from the
  executor) and no `"recalled"` event is emitted. See 9.K9.

### L459 — executor — "temp Deflect lives in turn-stamped `card_counters`"
**Verdict: DONE**
- `GameObject::temp_deflect_value` (`src/core/game_object.h:140`), added in `EffectExecutor::giveTemporaryKeyword`
  (`effect_executor.cpp:763`), read in `deflectCostFor` (462-464), cleared with the other this-turn keywords
  (`game_engine.cpp:1148`). Test: `SweepTriggersTest.TemporaryDeflect_EndsAtTheExpirationStep`
  (`tests/test_review_fix_sweep_triggers.cpp:748`). Not in the feature layout (checkpoint NOTE under L386).

### L460 — engine-play-combat — "8.7: engine half NOT A DEFECT; cards still to fix"
**Verdict: DONE**
- plans/08 line 85 is ticked (`- [x] **8.7 `requiresLegion()` and `requiresLevel()` are never read by the engine**`;
  last sub-bullets: sweep-flow deleted the three virtuals, sweep-costs fixed 587 —
  `SweepCostsTest.UndyingLegion_IsPlayedFromTheTrashForItsLegionCost`). `grep -rn "requiresLegion\|requiresLevel\|levelThreshold" src`
  → nothing.
  - OPEN (stale comments in tests): `tests/cards/test_review_fix_c1.cpp:979` and
    `tests/cards/test_wave_a_batch2.cpp:258-260` still say "`Card::requiresLegion()` is read by nothing ... the
    play-from-trash ability itself is not implemented yet (it needs engine support)". See 9.K36.

### L463 — engine-play-combat — "`generateTrashReplayActions` duplicates the two legality gates"
**Verdict: MIXED**
- Duplicate gates: DONE — `GameEngine::passesPlayGates` (`game_engine.cpp:6260`) is called by `canAfford` (6276),
  `canAffordChoosing` (1916) and `generateTrashReplayActions` (4035, "The legality gates every play passes").
  Covered by the existing trash-replay tests (`tests/test_review_fix_play_combat.cpp`, `tests/cards/test_death_from_below.cpp`)
  and `SweepCostsTest.UndyingLegion_...`, `.EndlessRiches_CardsArePlayedFromTheTrashAtTheirOwnCost`.
- Discounts on a grant cost (CR 356.4): DUPLICATE (fix in, test owed) — `GameEngine::trashGrantCost` (3976-3986) runs
  the grant's cost through `netCardCost` (same increases and discounts as a normal play), and the payment uses the
  same function (6688-6691). No test applies a discount to a grant cost: plans/09 **9.J4**, open ("a discount
  (CR 356.4) applied to a trash-replay grant cost") — owner sweep-costs (follow-up).

### L465 — engine-play-combat — "NEW: `chain_manager.cpp` `stepExecuteAndPass` PlayReaction branch"
**Verdict: MIXED (DONE for the engine; 1 OPEN)**
- With an engine: DONE — `GameEngine` installs `chain_manager_->setPlayCard([this](const Intent& i) {
  executeIntent(i); })` (`game_engine.cpp:83`); `ChainManager::stepExecuteAndPass` (`chain_manager.cpp:332-350`) calls
  it for `PlayReaction`, so a reaction play is the engine's own play (`onPlay`, additional costs, per-turn counters,
  `"played_not_from_hand"`, Repeat). Tests: `SweepTriggersTest.ReactionPlay_ThroughTheEngineIsAFullPlay` (a [Repeat]
  reaction: Repeat offered and paid, `cards_played_this_turn == 1`, effect twice),
  `.ReactionPlay_GoesThroughThePlayCallbackAndAFailedPlayIsNotReoffered` (9.D4 / 9.D10, `[x]`).
- OPEN — the old branch is still there as a fallback: `chain_manager.cpp:351-418` ("No engine attached (a
  ChainManager driven on its own): the bare mechanics — leave the zone, pay the printed cost, go on the chain").
  It is the code this row reported (no `onPlay`, no additional cost, no counters, no event), reachable only from
  tests that build a `ChainManager` without `setPlayCard` (`tests/test_review_fix_chain_triggers.cpp`,
  `tests/test_rules_compliance.cpp:544`). A second play path that only tests can reach. See 9.K25.

### L469 — engine-play-combat — "NEW: `Keyword::Reaction` set on ~25 permanents"
**Verdict: DONE (this stream, commit `8f1feb1`; run)**
- `Keyword::Reaction` removed from the 24 permanents that only have a [Reaction] ability (CR 813.1.a: the keyword can be on a card or on an ability; 813.1.d: "[Reaction][>]" on abilities) — gear 40, 81, 98, 120, 163, 204, 245, 326, 405, 438, 536, 538, 541, 542, 545, 549, 564, 611; legends 294, 296, 506, 514, 786 — and from legend 746 (Gold reminder text). Kept on 241, 348, 376, 647 (the card prints [Reaction]; `hasReactionTiming()`) and on the Quick-Draw gear 345, 379, 387, 504 (CR 819.1.b). The same commit removes `Keyword::Action` from the six permanents with an [Action] ability (113, 373, 404, 607, 723, 752).
- The reader the row names, `extraAnyPowerCost` (`game_engine.cpp`), needed no change: with the keyword gone a Seal is no longer taxed by Mystic Vortex. Test through the engine's aura pass and `canAfford`: `LedgerTest.MysticVortex_DoesNotTaxACardWhoseAbilityIsAReaction` (red against the unfixed def), `.MysticVortex_StillTaxesReactionQuickDrawAndAmbushCards`.
- Reaction timing for the abilities still comes from the descriptor: `LedgerTest.ReactionAbility_OfASealIsOfferedInAClosedStateWithoutTheKeywordOnTheDef` (`ActivateReactionAbility` offered; the Seal in hand is not playable in a Closed State), `.ReactionAbility_OfASealIsOfferedInAShowdown`, `.ReactionUnit_IsStillOfferedInAClosedState`.
- Left for sweep-verify-engine (comment only): the doc comment on `Card::hasReactionTiming()` in `card.h` still says the def keyword "is also set for permanents that merely have a [Reaction] ABILITY (the Seals, Gold)" — see New checkboxes.

---

## Counts

The table has 178 rows (L294–L471). 70 were already `done` and were not
audited (L294 and the 69 listed here): L309, L325, L326, L329, L331, L333,
L334, L338, L339, L342, L352, L354, L355, L360, L361, L362, L364, L365, L366,
L367, L370, L377, L380, L384, L387, L388, L391, L394, L395, L396, L397, L398,
L399, L400, L404, L405, L406, L408, L409, L410, L411, L413, L415, L416, L421,
L423, L426, L428, L434, L436, L438, L439, L442, L443, L448, L450, L451, L452,
L453, L455, L456, L461, L462, L464, L466, L467, L468, L470, L471.

The other 108 rows:

| Verdict | Rows | Lines |
|---|---|---|
| DONE | 45 | 300, 317, 318, 324, 335, 336, 337, 341, 343, 345, 349, 353, 359, 363, 371, 372, 373, 374, 378, 379, 389, 390, 392, 401, 402, 412, 418, 419, 420, 425, 430, 431, 432, 433, 435, 440, 441, 445, 446, 447, 449, 457, 459, 460, 469 |
| DUPLICATE | 13 | 296, 297, 304, 305, 307, 320, 347, 382, 393, 403, 422, 424, 444 |
| OPEN | 2 | 322, 437 |
| NOTE | 11 | 295, 298, 302, 303, 308, 315, 369, 383, 407, 414, 417 |
| USER | 2 | 327, 332 |
| MIXED | 35 | 299, 301, 306, 310, 311, 312, 313, 314, 316, 319, 321, 323, 328, 330, 340, 344, 346, 348, 350, 351, 356, 357, 358, 368, 375, 376, 381, 385, 386, 427, 429, 454, 458, 463, 465 |

In every MIXED row the fix the row names is in the code and tested, except
Defender of Tomorrow 936 (L311, L376) and Irelia, Graceful 462 (L328); what
keeps the row from DONE is a missing test, a stale comment, a box that was
never ticked, or a new finding from a `shared:` review — each is a line below
or an existing open box named in the entry.

**What the orchestrator can do with this:** set the 45 DONE rows to `done`
with the entry's evidence; set the 13 DUPLICATE rows to `done (tracked by
<box>)`; move the 11 NOTE rows to the final report (trainer facts are in
`docs/trainer-notes-2026-10.md`); paste the checkboxes below into
`plans/09-found-during-fix-run.md` as section K and set each MIXED / OPEN row
to `done (remainder: 9.K<n>)`; ask the two USER questions.

## Ledger corrections (no code work)

Boxes that are still `[ ]` although the work is done and tested — ready to
tick by their owner (sweep-verify-cards unless noted), with the evidence in the
entry named:

- plans/cards/batch-00.md "Equip cost (332, 339, 345, 353, 356, 365, 374, 379, 382, 387)" — L299, L385 (`SweepCostsTest.Equip_IsOfferedOnlyWhenItsCostCanBePaid`); its BLOCKED sub-bullet is stale.
- plans/cards/batch-06.md "670 Wily Newtfish" — L320 (`SweepTriggersTest.GainXp_IsCountedForTheTurn`); its BLOCKED sub-bullet is stale.
- plans/cards/batch-08.md the 29 / 466 / 334 / 187 box — L377 is done; tick after 9.K34 (Angle Shot attaches by hand).
- plans/09 **9.B1** — L307, L341, L441 (`C3Test.SquareUp_DoesNotForceADiscard` now expects the Repeat offer; `C3Test.SquareUp_RepeatIsPaidWithADiscard`).
- plans/03 **3.13** and **3.21** — L304 (nothing left but the tick).
- plans/07 **7.33** — L304: the c2 sub-bullet says FIXED without a vocabulary change (`ReviewFixC2Test.PartyFavors_TheTwoAnswersAreDistinctValues`); the STATUS row sends it to stage 5. Check the two values land in different `MakeChoice` slots, then tick here rather than carry it.
- plans/08 **8.16** — L444: every bullet has a fix and a test.
- plans/08 **S3** — L422: waits only on the re-read of 407 and 950.

Stale wording inside ticked boxes (reword when the files are next edited):
plans/cards/batch-03.md 582 ("remaining engine gap"), plans/cards/batch-02.md
Defy 45 ("can still be offered against a too-expensive spell"),
plans/cards/batch-00.md 948 ("BLOCKED: engine"), plans/cards/batch-07.md S5
("847 Sky Crusier — BLOCKED"), plans/cards/batch-05.md 462 (the BLOCKED
sub-bullet: what is left is the chain-time-target spells, see L328),
plans/cards/batch-09.md 367 (describes the older finalization-time fix).

Closed by this stream while the audit ran (already ticked on
`fix/sweep-verify-ledger`): plans/08 **8.8** (L324, L349, L469), plans/09
**9.C4** (L301), the 02 item 2.20 plan line (L310).

## New checkboxes

Ready to paste as a new section of `plans/09-found-during-fix-run.md`
("K. From the carry-over audit"). Every line is work that is in no checkbox
today. Where two helpers found the same thing it is one line.

### Owner: sweep-verify-engine

- [ ] **9.K1** `GameEngine::executeStandardMove` (`game_engine.cpp`): a multi-unit Standard Move exhausts, moves and announces the units one at a time; CR 144.3 makes it one action and 144.3.c pays the exhaust costs simultaneously. Exhaust all, move all, then emit the `UnitMovedEvent`s (so a `triggerCondition` on the first event sees every unit arrived), with a test; or pin the current order with a test and a comment saying why it cannot be observed. — **owner: sweep-verify-engine**
- [ ] **9.K2** Retrofit of the one engine-site NOT A DEFECT left (02 item 2.20): add the comment at `GameEngine::isScoreGatedByTurn` (`game_engine.cpp`) whose text is in plans/reports/sweep-verify-ledger.md ("Needs another stream"). The plan line is already in the required form. — **owner: sweep-verify-engine**
- [ ] **9.K3** `card_helpers.h`: unify `selectAnyNumber` (cap `kSelectManyMax = 10`, stale comment "ChainManager allows 16 re-entries") and `pickSeveral` (cap `kPickSeveralMaxPicks = 12`; cannot share a callback with a `pick*` helper) into one "several objects, one prompt each" helper that composes with the `pick*` helpers and has no cap sized to the old resume bound (it is 1024 and throws); migrate 230, 423 (`pickSeveral`) and 372, 497, 540 (`selectAnyNumber`). "Any number" is capped at 10 today for Azir, Sovereign 497 and Bard 540. Test with more than 12 picks. Closes with 9.A22 / 9.D3, which name only `pickSeveral`'s and `splitDamage`'s caps. — **owner: sweep-verify-engine**
- [ ] **9.K4** `card_helpers.h` `splitDamageAmong` (ENGINE-GAP comment above it): the units a split is divided among are chosen at resolution; CR 355.14.a-b make each one a Target chosen when the spell or ability is finalized (355.14.e leaves only the amounts to resolution). In no plan. Choose the targets at finalization (Deflect, "chosen" bookkeeping and mistargeting then follow the usual path) for Alpha Strike 751, Volibear 41 and the other callers, with a test that the opponent can respond knowing the targets; or close from the CR with a pin and remove the ENGINE-GAP comment. — **owner: sweep-verify-engine**
- [ ] **9.K5** `GameObject::banish_if_would_die_this_turn` (Smite 568) is tested on the `EffectExecutor::killObject` path only (`C3Test.Smite_BanishesTheUnitIfItDiesLaterThisTurn`). Add the `GameEngine::killUnit` path: a unit under Smite that takes lethal damage (cleanup / combat) is banished, no `UnitDiedEvent`, and the flag is gone after the Expiration Step. — **owner: sweep-verify-engine**
- [ ] **9.K6** `GameEngine::beginningStep` skips every attached gear with [Temporary]. CR 718.2 makes the card's PRINTED rules text Inactive (Spinning Axe 504); a [Temporary] GRANTED to an attached Equipment (Turn to Dust 632, Fading Memories 180 — both write `GameObject::keywords`) is not printed text and should still kill it. Decide from `rules/core-rules.md` (718.2, 721.2, 816); fix (the def has the printed set, so the engine can tell a printed keyword from a granted one) or pin with a test and a comment. If the rules do not settle it, mark `BLOCKED: rules question`. — **owner: sweep-verify-engine**
- [ ] **9.K7** `GameState::noteChosen` (`game_state.h`): the "unit abilities" half of Prodigal Explorer 514's count has no test. Extend `SweepFlowTest.ProdigalExplorer_NeedsTwoEnemyChoicesBySpellsOrUnitAbilities`: an enemy unit chosen by a unit's ability counts; one chosen by a gear's or a legend's ability does not. — **owner: sweep-verify-engine**
- [ ] **9.K8** `src/core/game_state.h`: the comment on `grant_repeat_base_to_next_spell` says "tranche cost = its base energy"; the grant charges Energy and Power (`GameEngine::executePlaySpell`). Correct it. — **owner: sweep-verify-engine**
- [ ] **9.K9** `GameEngine::recallMisplacedObjects` (`game_engine.cpp`, CR 323.6) is a second recall implementation beside `EffectExecutor::recallToBase`: it sets `location` / `zone` itself, so the gear attached to a unit recalled from another player's base keeps its old `location` (CR 719.3.a) and no `"recalled"` event is emitted. Call `effect_executor_->recallToBase(id)` (collect ids first: the loop iterates `state_.objects`). Test: a unit with attached gear in the opponent's base after a control change → after cleanup both are in its controller's base, `moves_this_turn` unchanged. — **owner: sweep-verify-engine**
- [ ] **9.K10** `src/cards/card.h`, doc comment on `Card::hasReactionTiming()`: "Not the same as Keyword::Reaction on the def, which is also set for permanents that merely have a [Reaction] ABILITY (the Seals, Gold)" is false since 8.8 — a permanent's def has `Keyword::Reaction` exactly when the card overrides `hasReactionTiming()` or has [Quick-Draw] (`LedgerTest.Defs_APermanentHasReactionOnlyWhenTheCardItselfDoes`). Replace the sentence (text in plans/reports/sweep-verify-ledger.md); then either make the default `hasReactionTiming()` read the def keyword for a permanent without [Quick-Draw] and delete the four overrides (241, 348, 376, 647), or say at the virtual why both exist. The test card `TAbilityOnlyReaction` in `tests/test_review_fix_play_combat.cpp` now models a shape no real card has: keep it as the guard it is, with its comment corrected. — **owner: sweep-verify-engine**
- [ ] **9.K11** `GameEngine::scoreConquer` (`game_engine.cpp`) stamps `conquered_on_turn` / `conquered_by_player` / `conquered_from` before the two early returns that mean "no Score happened": `isScoreGatedByTurn` (Forgotten Monument 523) and "already scored this battlefield this turn". Conquer is one of the two ways of Scoring (CR 471.1: "gains Control of a Battlefield they did not yet Score this turn"), which is the reasoning the 2.20 closure rests on; yet Perched Grimwyrm 338 ("a battlefield you conquered this turn") and Yone 544's condition read the stamp. The comment says "Reaching here means `player` conquered `bf`, even if the point-gain below is blocked/gated". Decide from the CR whether taking control without scoring is a conquer for those texts; move the stamp below the two returns or pin today's behaviour with a test for 338 at a turn-gated battlefield and correct the comment. — **owner: sweep-verify-engine**
- [ ] **9.K12** Card names in engine identifiers and trace strings: `PlayerState::zilean_present` / `zilean_double_token_turn` (`game_state.h`; read in `effect_executor.cpp` `createToken`, reset in `recalculateAuras`), the trace `"SYMBOL OF THE SOLARI: attacker tie -> recall ALL units"` (`game_engine.cpp`, logic is the `recall_all_on_attacker_tie` flag) and `"can't gain points (Tianna)"` (`scoreConquer` / `scoreHold`). The mechanisms are generic (a card sets a flag from `applyPassiveAura`), the names are not: rename the two fields to what they mean (an "additional copy of a played token unit" permission and its per-turn use) and make the trace strings name the rule, not the card. — **owner: sweep-verify-engine** (the `effect_executor.cpp` lines with 9.K16)

### Owner: sweep-triggers

- [ ] **9.K13** `EffectExecutor::moveToBase` / `moveToBattlefield` (`effect_executor.cpp`): the refusal of a move by its controller's effect while `GameObject::cant_move_this_turn` is set has no test (the batch-07 712 line claims it under `SweepFlowTest.Vex_ThePlayedUnitCannotBeMovedThisTurn`, which only checks the Standard Move generator). Add an engine-path test: Vex 712 stuns a played unit, its controller's move effect does nothing, an enemy's move effect and a recall still work. — **owner: sweep-triggers**
- [ ] **9.K14** `tests/test_chain.cpp` `ChainTestFixture.LegionParsesAsConditionGate`: after sweep-flow removed `EXPECT_TRUE(card->requiresLegion())` it only asserts `triggerType() == WhenYouPlayMe` under a name that claims a Legion gate — the one corrected test in the run that was weakened. Delete it (the gate is tested by `C4Test.DangerousDuo_NeedsLegionAndThenChooses`) or make it assert the gate. — **owner: sweep-triggers**
- [ ] **9.K15** `DelayedAbility` (`game_state.h`) has one player field for both "whose event fires it" and "who controls the ability" (`TriggerManager::checkDelayedAbilities`; `onScore`). Ashe, Focused 731 sets `controller = opponent` so her "When they hold, return it" goes on the chain under the opponent's control (CR 391.1 / 392: it is her controller's triggered ability) and the holder's Blue Sentinel 649 repeats it. Add a separate field for the watched player, keep `controller` as the source's controller, convert 731 (and use `DelayedAbility::kUntilFired` instead of `numeric_limits<int>::max()`), with a test that the ability is controlled by Ashe's controller and is not repeated by the holder's Blue Sentinel. — **owner: sweep-triggers**
- [ ] **9.K16** Zilean, Time Mage 648: "Once each turn" is printed on each Zilean, but the use is one stamp per player and `applyPassiveAura` sets a bool, so two Zileans at battlefields allow one extra token copy a turn, not two. Count the instances and the uses this turn (`EffectExecutor::createToken`; `units/0648_zilean_time_mage.cpp`); test with two Zileans. — **owner: sweep-triggers** (card file with sweep-verify-cards)
- [ ] **9.K17** `card_helpers.h` `createGoldExhausted` sets `is_exhausted` after `createToken` has announced the token — the defect plans/09 9.H3 lists for 746 and 403 only, in a helper 23 card files call. Fix it with 9.H3 (one "enters exhausted" for gear tokens inside `createToken`, honouring `tokens_enter_ready` there). Also give the Gold token the same tags from `createGoldToken` (`{"Gold"}`) and `createGoldExhausted` (`{}`). — **owner: sweep-triggers**
- [ ] **9.K18** `TriggerManager::checkDelayedAbilities` (`trigger_manager.cpp`) was changed to decide what fires on a copy, with no test: add one where queueing a delayed ability's item adds another delayed ability for the same trigger (a `kFinalizeTrigger` test card), asserting the new one does not fire for that event and nothing is skipped or fired twice. — **owner: sweep-triggers**
- [ ] **9.K19** `effect_executor.cpp` `StagedEffectCost`: a card an effect plays from a deck, the Champion Zone or facedown is staged as `Intent::PlaySource::Banishment` (`else if (from != ZoneType::Hand)`). Every reader only tests `!= Hand` today, so nothing breaks, but the value is false. Map the zone to its own `PlaySource` (add one for a deck) or stage a plain "not from hand" flag; pin with Drag Under 485 played off the deck. — **owner: sweep-triggers**
- [ ] **9.K20** `EffectExecutor::playIgnoringCost` reads `otherUnitsEnterReady` and `units_enter_ready_this_turn`, with no test of either on that path: an effect-played unit enters ready with Magma Wurm 11 on the board, and after Confront 129. — **owner: sweep-triggers**
- [ ] **9.K21** Two documented approximations in `trigger_manager.cpp`: `onCombatEnded` ("APPROX: ... every unit still at the battlefield counts as having been in the combat" for `WhenACombatIWasInEnds`) and `onUnitDied` ("APPROX: keyed on a spell being the resolving chain item" for `WhenYouKillAUnitWithASpell`, Immortal Phoenix 37 — a unit killed by the spell's damage in the cleanup after the item cleared is missed). Fix each (record the combat's participants before designations are cleared; attribute a lethal-damage death to the source that dealt the damage — `LastKnown::killed_by` exists) or show with a test that the case cannot arise. — **owner: sweep-triggers**
- [ ] **9.K22** `EffectExecutor::predict` (`effect_executor.cpp`): two stale comments — "Per CR 436.1, Predict is a PRIVATE look" (Predict is CR 437; 436 is Detach) and "For now: random agent recycles all ... Future: proper MakeChoice with multiple selections" above code that already asks per card and for the order. Correct the citation, delete the "for now / future" lines. — **owner: sweep-triggers**
- [ ] **9.K23** Death replacements are applied in a fixed order and the controller is never asked (CR 372: "the controller of the object being acted on determines the order the Replacement Effects will apply"). `GameEngine::killUnit` and `EffectExecutor::killObject` both go: the unit's own pending replacement → `banish_if_would_die_this_turn` → the battlefield card → the controller's cards in `state_.objects` (an `unordered_map`) order, first that applies wins — so which of Zhonya's Hourglass 77 / Guardian Angel 374 / The Boss 264 / Altar of Blood 762 / Smite saves a unit depends on hash order. Collect the applicable replacements, ask the dying unit's controller when there is more than one, apply in that order; put the one sequence in the executor so both kill paths share it. Test: two applicable replacements, the agent's choice decides which is used, and the order is stable without a choice. — **owner: sweep-triggers** (the `killUnit` call site: sweep-verify-engine)
- [ ] **9.K24** `TriggerManager::onCardPlayed` and `onTokenCreated` (`trigger_manager.cpp`) run a full `recalculateContinuousEffects()` for every permanent played and every token created, on top of the cleanup's pass (carry-over "full aura pass per permanent played", never picked up; not in plans/06). Measure it (count passes per game, or a smoke batch with and without); then restrict it to the case that needs it (some continuous effect grants [Vision] / [Weaponmaster]) or close NOT A DEFECT with the measurement, a pinning test that a granted keyword is seen on the play, and the cost stated at both sites. — **owner: sweep-triggers** (perf-engine if it starts first)
- [ ] **9.K25** `ChainManager::stepExecuteAndPass` (`chain_manager.cpp`, the `else if (chosen.type == IntentType::PlayReaction)` branch after the `play_card_` one) keeps the old "bare mechanics" reaction play (no `Card::onPlay`, no additional cost, no per-turn counters, no `"played_not_from_hand"`) for a `ChainManager` with no engine attached. Only tests reach it (`tests/test_review_fix_chain_triggers.cpp`, `tests/test_rules_compliance.cpp`). Delete it and give those tests a play callback (or drive them through `GameEngine`), so there is one play path; if a reason to keep it is found, state it at the site and pin with a test that the engine path never takes it. — **owner: sweep-triggers**

### Owner: sweep-costs

- [ ] **9.K26** Typed discard as an activation cost (`GameEngine::payAndQueueAbility`): the player is now asked which matching card to discard, but nothing tests it. Add an engine-path test for Sky Crusier 847 with two gear in hand (the second is picked and discarded, the other stays; not offered with no gear in hand) and one for Mel, Defiant Soul 897, whose [Empower] cost "Discard a spell" uses the same descriptor. Then correct the stale "847 Sky Crusier — BLOCKED" sub-bullet in plans/cards/batch-07.md and strike 847 from the list under plans/08 S5. — **owner: sweep-costs**
- [ ] **9.K27** No test for the base-only rune rule in `resource::runesInBase` / `payableEnergy` / `pay` (`src/engine/resource_payment.h`, reached from `payEnergy` / `payableEnergy` in `card_helpers.h`): add one where a ready rune outside its controller's base is neither counted nor exhausted, and one that the lowest-id base rune is the one exhausted (the fix for the map-order half), through a card that calls `payEnergy` (e.g. Hard Bargain 457). — **owner: sweep-costs**

### Owner: sweep-verify-cards

- [ ] **9.K28** Safety Inspector 726 (`units/0726_safety_inspector.cpp`): "You may spend 3 XP as an additional cost to play me" is asked and paid when the play trigger is finalized, after the unit has entered (ENGINE-GAP comment in the file). Convert to `additionalPlayCost` / `payAdditionalPlayCost` as Poppy 740, remember it for the trigger, remove the comment; test that the XP is gone before the unit enters. — **owner: sweep-verify-cards**
- [ ] **9.K29** Stale comments that contradict the code beside them: `spells/0931_death_mark.cpp` (ENGINE-GAP "pays the whole [A][A] from ONE domain" — fixed by `trashGrantCost`); `spells/0730_undying_loyalty.cpp` (header describes the old "discount up front, pay back" scheme); `gear/0401_temporal_portal.cpp` (ENGINE-GAP "the grant carries the spell's Energy cost only") and `battlefields/0772_the_academy.cpp` ("tranche cost = the spell's base energy"); `gear/0460_edge_of_night.cpp` and `legends/0554_grandmaster_at_arms.cpp` ("attachFree has no board check of its own" — `EffectExecutor::attachGear` has); `units/0540_bard_mercurial.cpp` (ENGINE-GAP header: the cost is a play cost now); `units/0544_yone_blademaster.cpp` ("the Weaponmaster base decides at resolution" — it is `kFinalizeTrigger`). Rewrite each to say what the code does; afterwards `git grep ENGINE-GAP -- src` must list only gaps that are open boxes. — **owner: sweep-verify-cards**
- [ ] **9.K30** Void Hatchling 341 ("If you would reveal cards from a deck, look at the top card first"): cards that reveal from the top of their Main Deck by hand without `EffectExecutor::lookBeforeReveal` — Ravenbloom Conservatory 529, Void Burrower 553, Void Rush 505, Teemo, Strategist 121, Diana, Lunari 641, Pakaa, Protector 820. Call it in each, one engine-path test per card with a Hatchling on the board. Decide from `rules/core-rules.md` and fix or pin: Blind Fury 25 (the opponent reveals), Twisted Fate 200 (rune deck), and "look at the top N, you may reveal one from among them" (381, 594, 613, 626, 741). — **owner: sweep-verify-cards**
- [ ] **9.K31** Fox-Fire 253 (`kMaxPicks = 12`) and Overt Operation 153 (`kMaxAsked = 12`) keep a cap sized to the old 16-re-entry bound, with the stale comment: the 13th 0-Might unit cannot be chosen; the 13th buffed unit is never asked. Remove both caps; the engine-path tests with more than 12 units are 9.C5. — **owner: sweep-verify-cards**
- [ ] **9.K32** Two corrected tests have names that say the opposite of what they assert: `CardTestFixture.DeathFromBelow_GrantsTrashReplayWhenMightLow` (`tests/cards/test_death_from_below.cpp`, asserts `trash_replay_grants.empty()`) and `AuditFix6Test.SmokeMirrors_FewerThanTwoTargetsNoOp` (`tests/cards/test_audit_fixes_6.cpp`, asserts the Draw 1 happens). Rename. `C3Test.SquareUp_DoesNotForceADiscard` likewise now tests "the Repeat offer can be declined". — **owner: sweep-verify-cards**
- [ ] **9.K33** Bellows Breath 402 (`spells/0402_bellows_breath.cpp` `onResolve`): "the same location" is anchored on `targets[0]`'s CURRENT location, so if the first-chosen unit moved in response it alone is hit and the units that stayed together take nothing (the result depends on pick order). Decide from CR 359.3.e.5 which targets are still legal (the location is fixed when the targets are chosen), fix, and test: first target gone → the others still take 1; first target moved → the decided behaviour. — **owner: sweep-verify-cards**
- [ ] **9.K34** Angle Shot 334 (`spells/0334_angle_shot.cpp`, `onResolve`, the "Attach (free)" branch) attaches by writing `attached_to` / `attachments` / `location` / `attachment_might_bonus` itself after only `objectExists`: no on-board check on the unit or the Equipment, and `is_rules_text_inactive` is not set (CR 718.2). It is the last attach outside `EffectExecutor::attachGear`. Replace the block with `ctx.executor.attachGear(equip, unit)`; test on the engine path: the unit leaves in response → nothing is attached; the attached gear is text-Inactive. — **owner: sweep-verify-cards**
- [ ] **9.K35** Nine cards play a unit by an effect with no printed location and always put it in the base (no location passed to `playIgnoringCost` / `playPayingCost`), against CR 355.2.a: Blind Fury 25, Reinforce 62, Promising Future 115, The Harrowing 198, Baited Hook 242, The Zero Drive 412, Last Rites 471, Void Burrower 553, Cursed Sarcophagus 710. Ask with `pickEffectPlayLocation` / `effectPlayLocation` when the card played is a unit (after 9.G1 for the shared filter); one engine-path test per card. Extends plans/cards/batch-09.md "Effect-played units always land in base". — **owner: sweep-verify-cards**
- [ ] **9.K36** Stale comments in tests after 8.7 / sweep-costs: `tests/cards/test_review_fix_c1.cpp` (`ReviewFixC1Test.HandOfNoxus_NoEnergyWithoutLegion`, "requiresLegion() is read by nothing") and `tests/cards/test_wave_a_batch2.cpp` (`CardTestFixture.UndyingLegionRequiresLegion`, "... the play-from-trash ability itself is not implemented yet") — the virtual is deleted and Undying Legion 587 is implemented (`Card::trashPlayCost`). Correct both; the second test asserts only the def keyword and should say so or be folded into `SweepCostsTest.UndyingLegion_IsPlayedFromTheTrashForItsLegionCost`. — **owner: sweep-verify-cards**
- [ ] **9.K37** Voidreaver 787 (`legends/0787_voidreaver.cpp`, `onActivate`): the second ability's text is "Move an exhausted friendly unit from a battlefield to its base" and the code rightly calls `moveToBase`, but the prompt label ("Voidreaver: recall") and the trace line ("spent 2 XP to recall") call it a recall — a different primitive since `recallToBase` exists (CR 458). Reword both to "move"; add an assertion that the move counts (`moves_this_turn`) and is blocked by "can't move to base". — **owner: sweep-verify-cards**
- [ ] **9.K38** Defender of Tomorrow 936 (`legends/0936_defender_of_tomorrow.cpp`): "[1], [E]: Ready a gear." and "[Empowered][>] [1], [E]: Ready 2 gear." are one activation, and while Empowered it always takes two gear with a second pick that cannot be declined — with one exhausted gear of your own and an exhausted enemy gear the player must ready the enemy's; the printed first ability is not available, and the "strictly dominates" comment is wrong. Tracked today only as prose (`plans/STATUS.md` "needs stage 5"; the batch-02 box is ticked), so the final gate cannot count it. Ask which printed ability is used through `pickMode` while Empowered (no vocabulary change), with a test of that case; the split into two `ActivatedAbility` descriptors is the stage 5 version and, if chosen instead, must be a box in the checkpoint stream's list. — **owner: sweep-verify-cards** (`pickMode`) / **checkpoint** (split)

### Owner: cleanup

- [ ] **9.K39** `src/ml/feature_extractor.h`: the doc comment on `extractStateFeatures` says "Build a 354-dim state feature vector"; the vector is `kStateFeatureDim` = 6188. Correct it (and check the "25-dim" action comment below it against the code). — **owner: cleanup**
- [ ] **9.K40** `src/main.cpp` `--help` and header comment describe the search agents wrongly: "mcts: OpenSpiel MCTSBot with a RandomRolloutEvaluator" (the agent uses `RiftboundHeuristicEvaluator`, no rollout), "Each selectAction call rebuilds a fresh OpenSpiel state by replaying action_history" (it keeps a mirror and replays only the choices since its last decision, `LiveMirror`), "ismcts: OpenSpiel ISMCTSBot" (since 9.E2 the search is `IsMctsSearch` in `agents/mcts_agent.cpp`). Rewrite the three passages. — **owner: cleanup**
- [ ] **9.K41** `tests/test_step_api.cpp` `StepApiTest.PerspectiveIsTheDecidingPlayerNotTheTurnPlayer` still tolerates "the main-phase loop can reach its decision with an item still on the chain" (`stale_closed`, allowed up to 5% of decisions) — the defect sweep-flow fixed (`advanceMainPhase` settles the chain first; CLAUDE.md "Settling before the main-phase decision"). Assert `stale_closed == 0` and delete the tolerance and its comment, so the test fails if the state comes back. — **owner: cleanup**

## For the user

1. **Reprint region tags (L327; cards 539, 543, 546, 547, 537, 548).** Each def now matches its own printing in `cards/raw/gallery_raw.json` (the Spiritforged reprints list fewer tags than the Origins cards: e.g. SFD-237 Karma `["Karma"]`, OGN-235 `["Vi", "Ionia"]`). Pinned by `VerifyCardsTest.Pin_3_21_ReprintsCarryTheTagsOfTheirOwnPrinting`. Options: (a) accept the gallery data per printing (current code; the same champion then has different tags by set, which matters to any card that counts a region tag); (b) treat a reprint as the same card and give both printings the union of the tags; (c) check the physical cards and correct by hand.
2. **Karma, Channeler 235 (L332).** The gallery entry `ogn-235-298` lists tags `["Vi", "Ionia"]`; the engine has `{Karma, Ionia}` on the assumption that "Vi" is a data error (the Spiritforged reprint says `["Karma"]`). With `{Vi, Ionia}` the card was a legal Chosen Champion for a Vi legend. No physical card was checked. Options: (a) keep `{Karma, Ionia}`; (b) follow the gallery literally (`{Vi, Ionia}`); (c) confirm from the printed OGN-235 card.
3. **Already recorded elsewhere, listed here so the three are in one place:** a hidden Guards! 475 at a battlefield where units can't be played plays no token (plans/09 9.I7; the other reading is "token goes to base").

Two of the new boxes may come back as rules questions if `rules/core-rules.md`
does not settle them when the owning stream reads it: 9.K6 (does a GRANTED
[Temporary] kill an attached Equipment — CR 718.2 says "printed Rules Text")
and 9.K33 (which Bellows Breath targets are still legal after one of them
moved).

Not verified in a browser, and no ThreadSanitizer run (L407): the page
JavaScript that echoes `decision_id`, the token query on the WebSocket URL,
per-unit owner / controller on moves and the hidden-zone counts as rendered
are tested as server frames only. A manual browser pass and one TSAN build are
the two things left to ask for, if wanted.
