# sweep-triggers — fourth follow-up report (plans/09 9.M13–9.M25, 9.N7)

## 1. Result

- Branch `fix/sweep-triggers`, head `b9b6732`, worktree clean. `fix/integration` is merged in; I resolved two conflicts (`trigger_manager.cpp` `onCombatStarted`, and `plans/09`).
- Clean rebuild (`--target clean` first, lane B, `-j3`, `ccache_pch_safe.sh`) of all four targets at `a2c2a79`:
  - **full test suite: 2591 passed, 0 failed** (1 disabled, as before)
  - `riftbound_openspiel_tests`: 24 passed, 0 failed
  - clone-equivalence: 10/10 passed, rc=0
  - smoke: 40 random games, rc=0
  - fault-injection check from `gate.sh`: rc=0 without the variable; rc=1 with `RIFTBOUND_STEP_FAULT_AT=40` ("injected engine fault at decision 40")
- `b9b6732` on top merges one integration commit that changes only `plans/STATUS.md`; not rebuilt for it.
- One slip: while chasing a compile error I ran one `cmake --build` outside the lane lock. It lasted a few seconds and nothing else of mine was building; every result above is from locked runs.

All 14 boxes are ticked in `plans/09-found-during-fix-run.md`.

## 2. Items closed

| Item | Outcome | What changed | Test (`SweepTriggersTest.` unless noted) |
|---|---|---|---|
| **9.M23** death replacements, controller chooses | FIXED | `EffectExecutor::applyDeathReplacements` is the one sequence for both kill paths. It collects the replacements that apply (new `Card::replacementApplies`, implemented by 77, 374, 264, 762, 550), lists them in a fixed order (the unit's own, its battlefield's card, its controller's cards by object id) and asks the unit's controller when there is more than one. A declined "may" drops out and the rest are offered again. | `DeathReplacements_TheControllerChoosesWhenSeveralApply` (both kill paths), `DeathReplacements_OrderIsStableWithoutAChoice` |
| **9.M15** delayed ability: controller vs watched player | FIXED | `DelayedAbility::watched_player`; `controller` stays the source's controller. Ashe, Focused 731 converted, and uses `kUntilFired`. | `DelayedAbility_WatchedPlayerIsNotItsController` (controlled by Ashe's controller, not repeated by the holder's Blue Sentinel, not fired by Ashe's own hold) |
| **9.M16** Zilean once each turn per Zilean | FIXED | `PlayerState::additional_token_copy_sources` counts the Zileans at battlefields; uses are counted per turn. The two card-named fields are gone. | `Zilean_EachCopyIsOnceEachTurnOnItsOwn` |
| **9.M21** two approximations | FIXED | Combat: units at a battlefield while its combat is in progress are recorded as participants, and "a combat that I was in ends" fires for them wherever they are now. Spell kills: a death found in the cleanup counts as "killed with a spell" when that player's spell dealt damage still marked on the unit. | `CombatParticipants_AUnitRecalledBeforeTheEndWasInTheCombat`, `KillWithASpell_CountsLethalSpellDamageFoundInTheCleanup` |
| **9.M13** `cant_move_this_turn` refusal | FIXED (test added) | — | `CantMoveThisTurn_StopsItsControllersMoveEffectsOnly` |
| **9.M18** delayed abilities decided on a copy | FIXED (test added) | — | `SweepTriggersCardTest.DelayedAbilities_OneAddedWhileQueueingDoesNotFireForThatEvent` |
| **9.M20** `playIgnoringCost` enters-ready | FIXED (test added) | — | `PlayIgnoringCost_HonoursEntersReadyEffects` (Magma Wurm 11, Confront 129) |
| **9.M17** Gold helpers | FIXED | Exhausted-at-announce was done under 9.H3; verified. Both helpers now give the token no tag — CR 187.5 lists none, so the `{"Gold"}` tag was the wrong one. | `GoldHelpers_MakeTheSameToken` |
| **9.M19** staged play source | FIXED | `StagedEffectCost` maps each zone to its own `PlaySource`; new `PlaySource::MainDeck`, which no intent carries. | `SweepTriggersCardTest.EffectPlay_StagesTheZoneTheCardIsReallyPlayedFrom`; pin `DragUnder_PlayedOffTheDeckCostsTwoLess` |
| **9.M25** bare reaction-play branch | FIXED | Deleted; a `ChainManager` with no play callback now throws. | `ChainTestFixture.ReactionAddsToChainAndRestarsFEPR`, `…AmbushUnitPlayedAsReactionIsAPermanent` (now set a callback) |
| **9.M14** weakened Legion test | FIXED | Deleted; the gate is covered by `C4Test.DangerousDuo_NeedsLegionAndThenChooses`. | — |
| **9.M22** stale `predict` comments | FIXED | CR 437.1; "for now / future" lines replaced. | — |
| **9.M24** extra aura pass per play | NOT A DEFECT | Measured and kept; cost stated at both sites. See below. | pins: `Vision_*` (3), `Weaponmaster_ASandSoldierTokenMayEquipAsItIsPlayed` |
| **9.N7** Soraka recall events | NOT A DEFECT | Both paths emit what they should: one `"recalled"`, no death and no move from the lethal-damage recall; one `UnitDiedEvent` from the later kill at base. | pin: `Soraka_LethalDamageRecallThenALaterKillEmitWhatTheyShould` |

**9.M24 measurement** (temporary counters, 200 random games each way, yi_rebuild / fiora, one thread):

| | extra passes | all aura passes | time in extra passes | wall |
|---|---|---|---|---|
| yi_rebuild first | 3231 | 23111 | 13.0 ms | 0.57 s |
| fiora first | 3336 | 23371 | 13.2 ms | 0.57 s |

That is about 16 extra passes a game, 14% of aura passes, roughly 4 µs each, 2.3% of wall time. Removing them breaks keywords granted by a continuous effect on the play they apply to (the pins go red).

**9.M25 note.** The two tests that reached the deleted branch were in `tests/test_chain.cpp`, not the two files named in the item; `test_review_fix_chain_triggers.cpp` and `test_rules_compliance.cpp` passed untouched.

**Changes outside my three files, kept minimal:**

- `game_engine.cpp` `killUnit`: its own copy of the replacement sequence is replaced by the one call to `applyDeathReplacements`.
- `game_engine.cpp` `recalculateAuras`: one reset line renamed for the Zilean field.
- `game_state.h`: `watched_player`, the two renamed token-copy fields.
- `intent.h`: `PlaySource::MainDeck`.
- `card.h`: `replacementApplies`.
- `card_helpers.h`: the Gold tag.

This closes the field half of 9.M12 (sweep-verify-engine's); the trace strings there are untouched.

## 3. Mutation run (one build, 13 mutations)

| Fix disabled | Red test |
|---|---|
| 9.M13 refusal removed (both move functions) | `CantMoveThisTurn_StopsItsControllersMoveEffectsOnly` |
| 9.M15 match on `controller` again | `DelayedAbility_WatchedPlayerIsNotItsController` |
| 9.M16 aura sets 1 instead of counting | `Zilean_EachCopyIsOnceEachTurnOnItsOwn` |
| 9.M17 `{"Gold"}` tag put back | `GoldHelpers_MakeTheSameToken` |
| 9.M18 one-shots no longer marked fired | `SweepTriggersCardTest.DelayedAbilities_OneAddedWhileQueueing…` |
| 9.M19 deck staged as Banishment | `SweepTriggersCardTest.EffectPlay_StagesTheZone…` |
| 9.M20 enters-ready effects ignored | `PlayIgnoringCost_HonoursEntersReadyEffects` |
| 9.M21 participants not recorded | `CombatParticipants_AUnitRecalledBeforeTheEndWasInTheCombat` |
| 9.M21 spell damage not remembered | `KillWithASpell_CountsLethalSpellDamageFoundInTheCleanup` |
| 9.M23 controller never asked | `DeathReplacements_TheControllerChoosesWhenSeveralApply`, `…OrderIsStableWithoutAChoice` |
| 9.M24 both extra passes removed | `Vision_TheControllerChoosesWhetherToRecycle`, `Vision_EachInstanceTriggersSeparately`, `Forecaster_HasOneInstanceOfVision`, `Weaponmaster_*` (2) |
| 9.N7 `"recalled"` not emitted | `Soraka_LethalDamageRecallThenALaterKillEmitWhatTheyShould` |

Not mutated:

- 9.M25 is a deletion; the proof is that the two chain tests fail without a play callback, which they did before I gave them one.
- 9.M14 and 9.M22 change no behaviour.
- The 9.M18 mutation shows the test is sensitive to the fire-once bookkeeping. I did not restore the old iterate-while-pushing loop itself, since that is undefined behaviour rather than a deterministic red.

## 4. Needs another stream

- **Trainer note:** a new `MakeChoice` prompt when two or more replacements apply to one death. New state not in the feature layout: `DelayedAbility::watched_player`, `PlayerState::additional_token_copy_sources` / `_used` / `_turn`, `LastKnown::killed_with_spell`.
- **sweep-verify-engine:** `killUnit` now ends its replacement handling with one executor call; if that stream is editing the same lines it should keep the call and drop its copy of the old blocks.

## 5. New defects found (not fixed)

- `Card::replacementApplies` repeats the conditions already written at the top of each card's `applyReplacement` (five cards). The two can drift; `applyReplacement` should call the predicate.
- Two Zileans both apply to a turn, but only one additional copy is made per token play. Whether both replacements may apply to the same play (three tokens) is a rules reading I did not settle.
- The spell-damage marker is cleared by `healObject` and by its turn stamp, but not by cards that zero `damage_marked` directly. A unit healed that way and then killed by non-spell lethal damage the same turn would count as killed with a spell.
- `CombatParticipants_…` drives the combat with hand-emitted start and end events. The real tie-recall path through `runCombat` has no test showing a recalled attacker gets "a combat that I was in ends".
- `Card::hasReplacementEffect()` cards that leave `replacementApplies` at its default are listed as applicable whenever they are in play, so a future card that forgets the predicate produces a needless prompt.
