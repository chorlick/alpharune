# sweep-triggers — final report

## 1. Result

- Branch `fix/sweep-triggers`, head `1e9eb2a`, worktree clean. `fix/integration` (`fc2275d`, with sweep-flow and cleanup) is merged in; I resolved the conflicts myself.
- Clean rebuild (`--target clean` first) of `riftbound_tests`, `riftbound` and `riftbound_openspiel_tests` on the merged tree, with the PCH-safe launcher:
  - **full test suite: 2300 passed, 0 failed** (1 disabled, as before)
  - `riftbound_openspiel_tests`: 17 passed, 0 failed
  - smoke: 40 random games, yi_rebuild vs fiora, seed 7, rc=0
- I did not run the clone-equivalence test or the `RIFTBOUND_STEP_FAULT_AT` gate check.
- Build setup: switched to lane B (`.build-b.lock`, `-j3`) and to `ccache_pch_safe.sh`, then did a clean build; every result above is from after the switch.

Things to know before relying on this:

- **Scope was reconstructed.** After the interruption my original task brief was no longer in my transcript. I rebuilt the item list from the `STATUS.md` carry-over table by file ownership (`trigger_manager`, `chain_manager`, `effect_executor`) plus the orchestrator's later messages. Rows I did not recognise as mine are listed in section 3.
- **Red-before evidence is partial.** 12 of the first 13 sweep tests were shown failing on the unfixed code (the recall, move-block, score-doubling, death-batch and LeBlanc tests). The other 38 tests in `tests/test_review_fix_sweep_triggers.cpp` were written after the fixes and were never run against the old code.
- **Commits are not bisectable.** `7dc7aee` does not build without `947e61d`, and the first merge commit `673891a` also carries the Svellsongur work.
- **sweep-costs is not merged yet and will conflict with this branch** (see section 4).

## 2. Items closed

Tests are in `tests/test_review_fix_sweep_triggers.cpp` (`SweepTriggersTest.*`, 51 tests) unless another suite is named.

### Plan boxes ticked

| Box | Outcome | What changed |
|---|---|---|
| batch-00 **382 Svellsongur** and **9.C1** | FIXED | `Card::copiesBearerText()`. The bearer's own triggers are queued once more per such Equipment; its continuous effects and printed [Assault N] / [Shield N] / [Deflect N] apply once more; "trigger an additional time" counts again. A second copy of an activated ability adds nothing, so it is offered once (pinned by a test). |
| batch-00 **290 Vilemaw's Lair** | FIXED | `EffectExecutor::moveToBase` refuses a blocked move. |
| batch-00 **272 Back-Alley Bar** | FIXED | Recalls no longer emit `UnitMovedEvent`. |
| batch-05 **337 Minotaur Reckoner** | FIXED | Effect moves to base are refused; recalls still happen. |
| batch-05 **423 Fae Dragon** | FIXED | `EffectExecutor::spendBuff` announces every spend (`WhenYouSpendABuff`). |
| batch-06 **649 Blue Sentinel**, batch-10 **591 Red Brambleback** | FIXED | `Card::extraScoreTriggers`; `TriggerManager::onScore` queues every hold / conquer ability of that score once more. |
| batch-06 **620 Lillia** | FIXED | `WhenYouPlayATokenUnit`, fired from `TokenCreatedEvent`. |
| batch-06 **648 Zilean** | FIXED | `createToken` asks the "may" and lets the controller place the copy; `lastTokensCreated()` returns every token (Keeper of Masks 643 and Zed 810 use it). |
| batch-06 **550 Soraka** | FIXED | `recallToBase` / `healExhaustRecall`. |
| batch-07 **879 Renekton, Brute** | FIXED | `Card::mightThreshold()` and `WhenMyMightReachesThreshold`, judged on every settled state (new `StateSettledEvent`). |
| batch-07 **718 Loyal Poro**, **777 Lonely Poro** | FIXED | Last-known "other friendly units here", taken for a whole batch of deaths before the first unit leaves. |
| batch-07 **908 Reluctant Leader / 712 Vex** (token plays) | FIXED | A token is played (CR 350.2): `TriggerManager::onTokenCreated`. |
| batch-01 **512 Emperor of the Sands**, **8.4** | FIXED | [Vision] and [Weaponmaster] are keyword abilities, one chain item per instance (`src/engine/keyword_abilities.{h,cpp}`). |
| **9.A17** Ornn 381, Ivern 613 (with Called Shot 443, Fate Weaver 626) | FIXED | All four go through `EffectExecutor::drawHeldCard`; test asserts the draw event. |

### Carry-over rows closed (FIXED, each with a test)

- **Recall primitive** (executor row, plus both "recall detaches gear" rows for `killUnit` and `killObject`): `recallToBase` keeps gear, is not a move, cannot be blocked. Callers migrated: Zhonya's 77, Guardian Angel 374, Possession 203, The Boss 264, Altar of Blood 762, Hostile Takeover 516, Conscription 702, Glowstone 920, Unlicensed Armory 23, and the combat-tie recall.
- **Battle Mistress 517** triggers once per batch of deaths (`Card::triggersOncePerBatch`).
- **Own-death triggers** go through `fireTrigger`, so `triggerCondition` and the LeBlanc [Temporary] suppression apply.
- **`onUnitStunned`** uses `fireTrigger` with the stunned unit as subject; a source-less stun is attributed to the resolving effect's controller. Vex, Mocking 617 reads the subject.
- **`WhenOpponentPlaysAUnit`** carries `triggering_subject`; the counter on the watcher is gone.
- **Vision** asks its controller and triggers once per instance (CR 817.2). Gemcraft Seer 100 and Forecaster 388 now grant one instance per copy on the board; Forecaster's def no longer carries a second, printed Vision.
- **Darius, Trifarian 27** sees himself as the second card (play ordinal stamped on the card).
- **Solari Shrine 72**: "you kill" is tracked (`LastKnown::killed_by`).
- **Unlicensed Armory 23**: the replacement belongs to the unit and applies with the gear gone.
- **Scuttle Crab 615**: cards hidden later that turn are shown too.
- **`DelayedAbility`**: one `repeats` flag (`every_time` removed); the default now expires at end of turn, `kUntilFired` for Blue Sentinel and Iascylla; `checkDelayedAbilities` no longer iterates the live vector while queueing.
- **`recycleCards` on a board permanent**: gear comes off, a token ceases to exist, `LeftBoardEvent` is emitted. Divine Judgment 244's workaround is removed.
- **`recycle_self` cost and the mulligan** announce `"recycled_main"`. This one has no test: no card uses `recycle_self` any more and I did not write a mulligan test.
- **`playIgnoringCost`** asks `entersReadyOnPlay`.
- **Attach paths (8.14 and the `is_rules_text_inactive` row)**: `attachEquipped` / `attachFree` call `EffectExecutor::attachGear` (on-board checks, text Inactive). Blighted Battleaxe 581's end-of-turn ability became an equipped trigger so it still fires while attached.
- **`ignoresDeflect`** is honoured for targets locked at play (`payDeflectForChoices`).
- **Reaction plays** in `stepExecuteAndPass` go through the engine's own play path (`ChainManager::setPlayCard`). No test isolates this; the existing reaction and ambush tests pass.
- **Dancing Grenade 582 / Death from Below 747**: a spell replayed while it resolves stays on the chain and is filed once.
- **Temp Deflect** is a field (`temp_deflect_value`), cleared with the other "this turn" keywords.
- **`gainXp`** at the ten remaining raw `xp +=` sites (658, 671, 698, 720, 759, 787, 657, 669, 751, 769).
- **`LogEvent` visibility** (both io-cli rows): `LogEvent::audience` is a `LogAudience` that says exactly which players may read a line — both, one named player, or neither. Draw, mulligan, peek, predict and hide lines are tagged private. The play server in fair-play mode forwards the lines every human seat may read (tested in `tests/test_review_fix_io_cli.cpp`).

### Existing tests corrected

Each has a one-line comment citing the card text: `C0Test.SolariShrine_*`, `C6Test.KeeperOfMasks_ZileansExtraReflection…`, `C6Test.BlueSentinel_AddsThePower…`, both `C7Test.RenektonBrute_*` (the second renamed `…IsEmpoweredWhateverTakesHimToTen`), both `MissingFixTest.BlightedBattleaxe_*`, `AuditFix4Test.Iascylla_HoldSchedulesDelayedAbility`, the Vex tests in `test_audit_fixes_0.cpp`, `test_trigger_events.cpp` and `test_review_fix_c7.cpp`, and `PlayServerProtocol.FairPlayFramesWithhold…`.

## 3. Items not closed

- **Resolve-loop cap of 16 / 12 prompts**: the engine half was done by sweep-flow (bound 1024, throws); I dropped my own version. The helper caps that were sized to the old bound are still there: `kSplitDamageMaxPicks` and `kPickSeveralMaxPicks` in `card_helpers.h`, `kMaxMoves` in Corrupted Dragon 878, each with a now-stale ENGINE-GAP comment. Not changed by either stream.
- **Simultaneous deaths outside lethal damage**: only `processLethalDamage` opens a death batch. A card that kills several units by calling `killObject` in a loop still kills them one at a time, so Loyal / Lonely Poro and Battle Mistress see those as separate deaths.
- **"shared: commit to review" rows** (c0–c7): I did not review them as such. I read the trigger-manager parts while rewriting those functions and found nothing beyond what is fixed above.
- **Carry-over rows I took to belong to other streams and did not touch**: trigger queued while an Equip is being paid; `payAnyPower` duplicating `payAdditionalCost`; `payEnergy` / `payableEnergy` rune scope; the full aura pass per permanent played (perf); `GameEngine::attachGearToUnit` (see section 5).
- **`CLAUDE.md`** is not updated for the new primitives (recall, `LogAudience`, keyword abilities, text copies), to avoid a three-way conflict in that file. It needs a few bullets once the sweeps are merged.

## 4. Needs another stream

**`shared:` commit `947e61d`** (`game_engine.{h,cpp}`, `main.cpp`, `io/play_server.{h,cpp}`): both kill paths call `applyOwnDeathReplacement`; lethal-damage deaths are a batch; `StateSettledEvent` after cleanup; keyword-ability dispatch; the play-card callback; recycle announcements; tie recall; private log lines; log forwarding by audience. The merge commit adds `recalculateAuras` step 1b' for text copies.

**Expected conflicts with sweep-costs** (its commit `2053a08`):

- `units/weaponmaster_base.h`: I left this file untouched on purpose. My keyword-level Weaponmaster ability derives from `WeaponmasterUnit`, so it picks up the cost-charging version when sweep-costs merges.
- `card_helpers.h`: sweep-costs adds its own `spendBuff(ctx, unit)` that emits the same `"buff_spent"` event without naming the spender. It should call `ctx.executor.spendBuff(unit, ctx.controller)` instead, or Wallop 146 / Call to Glory 207 / Kraken Hunter 150 will credit Fae Dragon to the unit's controller rather than the spender.
- `attachEquipped` / `attachFree` are now one-line calls to `EffectExecutor::attachGear`; `payEquipAndAttach` should end up there too.

**Trainer notes:**

- New `MakeChoice` prompts: Vision (per instance), Weaponmaster on tokens, Zilean's "may" and copy location.
- New state fields, not added to the feature layout: `GameObject::temp_deflect_value`, `pending_death_replacements`, the extra `LastKnown` fields, `PlayerState::may_look_at_opp_facedown_turn`, `ChainItem::keyword_ability`.
- Five `TriggerType` values appended at the end of the enum.
- `LogEvent::audience` is the hook for a per-observer event history.

## 5. New defects found (not fixed)

- `GameEngine::attachGearToUnit` (`game_engine.cpp`) is a second attach path beside `EffectExecutor::attachGear`: it refuses an already-attached gear and has no on-board check.
- Stacked Deck 183 puts the kept card into the hand by hand (`ps.hand.push_back`) with no draw event. Its text needs checking for whether that is a draw; same class as 9.A17 if so.
- `EnginePathAgent`'s default answers Predict with "recycle" (the first option), so any test that plays a Vision unit without an agent silently recycles the top card.
- Zed 810 sets `card_def_id = 955` on the Shadow Clone after `createToken` has already emitted `TokenCreatedEvent`, so anything keyed on the token's card at play time does not see it.
- `ChainManager::stepExecuteAndPass`: an activation or reaction play that adds no chain item re-asks the same player with the same options; only the 1000-iteration bound stops a card that keeps failing.
- Card-level trace lines were only audited for hidden-card names in files that touch the deck (Scryer's Bloom 698 and Stacked Deck 183 fixed). A full audit of every `logTrace` in `src/cards` for names from a hand or facedown zone was not done.
