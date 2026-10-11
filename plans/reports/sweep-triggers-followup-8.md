# sweep-triggers — eighth follow-up report (plans/09 9.DD4: the other reflexive-trigger cards)

## 1. Result

- Branch `fix/sweep-triggers`, head `1165fc2`, worktree clean.
- Verified against `fix/integration` at **`555b2db`**, merged in; integration has not moved since. One conflict, in `plans/09` only (my 9.DD4 tick against the new section EE).
- Clean rebuild (`--target clean` first, lane B, `-j3`, the tree's `scripts/ccache_pch_safe.sh`) of all four targets at `1165fc2`:
  - full test suite: **2830 passed, 0 failed** (1 disabled)
  - `riftbound_openspiel_tests`: 25 passed
  - clone-equivalence: 10/10, rc=0
  - smoke: rc=0
  - fault-injection check: rc=0 without the variable, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`

9.DD4 is ticked in `plans/09-found-during-fix-run.md`.

## 2. Reading the fourteen against CR 388 / 389

All fourteen are reflexive triggers. Each prints "do this:" (Wild Claw and Kharox as "you may do this:"), which CR 388.1 names. None needed a "not reflexive" marker.

| Card | First part (stays in the parent) | Reflexive trigger (own chain item) | Chosen at its finalization |
|---|---|---|---|
| Forgotten Relic 895 | Burn 1 | give a friendly unit +[M] equal to the burned unit's Might | the unit |
| Disintegrate 5 | deal 3; kill | draw 1 | — |
| Dragon's Rage 255 | move the enemy unit | it and another enemy unit there deal damage to each other | the other unit |
| Guards! 475 | play the Sand Soldier | you may pay [C] to ready it | the "may", and the [C] is paid |
| Arise! 513 | play the Sand Soldiers | ready up to two of them | which, and how many |
| Bone Skewer 701 | they play the chosen unit | stun it | — |
| Death from Below 747 | kill; had 3 [M] or less | you may play this from your trash for [A] | the "may", and the [A] is paid |
| Alpha Strike 751 | split damage; kills | gain 1 XP — one trigger per unit killed | — |
| Mirror Image 757 | play the ready Reflection | it becomes a copy; give it [Temporary] | — |
| Wild Claw 876 | banish, pay, play; recycle the rest | you may Empower it | the "may" |
| Rell, Magnetic 347 | play the Equipment | attach it to me | — |
| Ivern, Nurturer 613 | look, reveal, draw, recycle | buff a friendly unit | the unit |
| Keeper of Masks 643 | play two Reflections | they become copies of me | — |
| Kharox 901 | they Burn 3 | you may play a unit from their trash | the "may", the unit, where it goes |

Behaviour changes worth knowing, beyond the extra priority window:

- **Death from Below** now does what the rule book's example says: the spell resolves and goes to the trash, and the trigger plays it from there. The "replayed while it resolves" state in `ChainManager` is no longer used by this card; Dancing Grenade still uses it.
- **Mirror Image**'s token has no [Temporary] until the trigger resolves, as with Deceiver.
- **Arise!**'s "stop" option is now the shared helper's ("None — Arise!: ready a Sand Soldier (up to two)") instead of the card's own "Arise!: ready no more". Same choice, different label.
- **Ivern**'s buff target is chosen after the draw, not before it.

## 3. What the conversion kept

- **Alpha Strike 751**: split targets still chosen at the spell's finalization (9.M4); `mayChooseAtFinalization` untouched.
- **Guards! 475 / Arise! 513**: token locations through the shared helpers; the hidden-Guards! reading (9.G3) untouched.
- **Wild Claw 876**: the payment path (`playPayingCost` with the reduced cost) untouched; only the Empower offer moved.
- **Ivern 613**: the draw through `drawHeldCard` and the Void Hatchling comment untouched.
- **Keeper of Masks 643**: still collects Zilean's copies through `lastTokensCreated`; all of them are handed to the trigger.
- **`mayChooseAtFinalization` declarations: none changed.** sweep-costs' `FinalizePassSpellDeclarationsMatchTheirPickLists` passes.

## 4. Registry test

- `Card::hasReflexiveTrigger()` (true on all 17 converted cards) and `Card::notReflexiveBecause()` (no card uses it yet).
- `SweepTriggersTest.Reflexive_EveryCardWithThePhraseDeclaresItself` walks the registry: a card whose text has "do this", "do the following" or "do one / X of the following" must declare one of the two, and a card without the phrase may not claim a reflexive trigger.
- Its first run flagged Friendship 608, Ivern, Friend to All 739 and Daisy! 754. They print "for each of the following tags", a list, not one of the CR 388.1 phrases. I narrowed the phrase match to the rule's wording rather than marking them; the test has a comment naming the three.

## 5. Tests

- **New:** `ReflexiveCardsTest.*`, one per card (14). Each plays the card or fires its trigger through the engine and checks, from the opponent's first priority with the trigger on the chain, that the first part is done and the second is not; then that it resolves. P1's answers are declared per test and an undeclared prompt fails the test.
- **Moved onto the engine** (they drove the cards with a bare executor, which cannot queue a chain item): `AuditFix4Test.KeeperOfMasks_CreatesTwoReflectionCopies`, `AuditFix5Test.IvernNurturer_ThemedDraftBuffs`, three `RulesCards2Test.RulesCards2_Arise_*`, `RulesCardsTest…AlphaStrike_ControllerMayPutAllDamageOnOneUnit`, `RulesCardsTest…MirrorImage_TokenInControllersBaseWithLastingTemporary`, two `VendettaCTest.Vendetta_Kharox_*`, two `VendettaD.Vendetta_WildClaw_*`, `VendettaE.Vendetta_ForgottenRelic_BurnedUnitGivesItsMight`. Same pickers and assertions.
- **Changed expectation, one test:** `ChainTestFixture.DisintegrateKillsAndDraws` uses a bare `ChainManager` fixture with no engine. It now asserts the kill and that nothing is drawn there; the draw is checked by `ReflexiveCardsTest.Disintegrate`. The test's name is now misleading.
- **Label match updated:** `VerifyCardsTest.Pin_Arise_ReadyingNoneOfTheSandSoldiers` (the "None" option's new label).
- **Renamed:** the Hwei pin is `Pin_Hwei_TheRunesAreNamedAtTheReflexiveTriggersFinalizationNotTheMoveAbilitys`.

## 6. Mutation run (one build, two mutations)

| Fix disabled | Red tests |
|---|---|
| nothing is put on the chain | all 14 `ReflexiveCardsTest.*`, plus 46 existing tests of these seventeen cards |
| Kharox's `hasReflexiveTrigger` removed | `Reflexive_EveryCardWithThePhraseDeclaresItself` |

This is one mutation for the whole group, not one per card: every card goes through the same queue call, and each card has its own test that went red.

## 7. Needs another stream

- **Trainer note:** seventeen cards now put a second chain item on the chain, each with its own finalization and priority window. Prompt order changed for Ivern (buff target after the draw) and the Arise! stop label changed.

## 8. New defects found (not fixed)

- `ChainTestFixture.DisintegrateKillsAndDraws` (`tests/test_chain.cpp`) should be renamed or moved onto an engine: it no longer shows a draw.
- `Card::notReflexiveBecause()` has no user, so the "explicit marker" branch of the registry test is not exercised by any card.
- Death from Below no longer uses the "spell replayed while it resolves" state; only Dancing Grenade 582 does. Dancing Grenade's "Its controller may play this spell again for [A]" has no "do this" and I did not re-read it against the rules.
