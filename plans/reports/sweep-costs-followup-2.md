# sweep-costs follow-up 2 — report

Branch `fix/sweep-costs`, head `6ce5ad2`, worktree clean. It contains `fix/integration` up to and including 9.T4 (`7324105`). Integration has since gained five commits (sweep-verify-cards tranche 8, eight source/test files); those are not merged or tested here.

## 1. Result

Clean rebuild at `6ce5ad2`, safe launcher, lane A, `-j3`:

| Check | Result |
|---|---|
| `riftbound_tests` | 2717 run, 2717 passed |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 3 threads | rc 0, P1 139 / P2 61 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, failure named, not a draw |

**Do not build or bisect at `0999945`.** That commit has conflict markers in `spells/0253_fox_fire.cpp` and `plans/09` (a script of mine committed through a failed merge). `14776ab`, the next commit, fixes both.

## 2. Items — all ticked in `plans/09`

New tests are in `tests/cards/test_sweep_costs.cpp` (`SweepCostsTest.*`, now 62).

**9.L2 — hidden and trash-replay generators.**
- Trash: a grant's cost ignored what the chosen targets take off. `trashGrantCost` now takes the targets; the generator, the play-time target choice and `payTrashReplayGrant` share `canPayTrashGrant`. Tests: `TrashReplayGrant_ATargetsDiscountCountsTowardTheGrantCost`, `..._TakesThePowerOfAGrantWithNoEnergy`.
- Hidden: NOT A DEFECT. A card played from facedown ignores its base cost (CR 811), so there is no cost gate and no discount can matter. Stated at `generateHiddenPlayActions`; pinned by `HiddenPlay_HasNoCostGateAndATargetIsOfferedOnlyIfItsDeflectCanBePaid`.
- That pin found a real bug: a hidden spell that takes its target through `pickTarget` asked again at resolution and ignored the target it was played with, after the Deflect was paid for it. Fixed in `executePlayFromHidden`.

**9.L1 — direct `Card::onEquip` calls in tests.** All 33 call sites now go through a new fixture helper, `equipThroughEngine(gear, unit)` in `card_test_fixture.h`, which executes the Equip intent. No test changed result when routed through the engine.

**9.N2 and 9.T1 — a finalize-pass spell chooses before it pays.**
- A `kFinalizeResolve` spell's first-execution choices are now always made in `executePlaySpell` before `payCardCost`. Each choice's Deflect is paid as it is made, in that step, and an object is on offer only while the spell's cost and that Deflect can be paid together.
- A discount from what is chosen (Irelia, Graceful) therefore counts. The generators offer such a spell when a choice would make it payable (`payableThroughAFinalizeChoice`, new `Card::mayChooseAtFinalization`).
- With 9.T4: the choices are kept in `prefinalized_spell_` and adopted in `chooseFirstExecution` at the Finalize step, so the item is not asked again; I did not use `finalized_early_`. The Repeat block stays where 9.T4 put it, after `repeats_paid` is set.
- **Prompt order changes for all 32 finalize-pass spells:** first-execution choices, then cost prompts (additional costs, Repeat), then each Repeat execution's choices. Three existing tests were corrected for this (Bonds of Strength, Undying Loyalty, Rocket Barrage).
- Tests: `FinalizePassSpell_ADiscountFromWhatItChoosesCountsBeforePayment`, `..._WithoutADiscountItPaysItsFullCostAndIsNotOfferedShort`, `..._AUnitIsOfferedOnlyIfItsDeflectCanBePaidWithTheCost`. The batch-05 box "462 Irelia, Graceful" is ticked.

**9.T2 — hidden finalize-pass spell with nothing to choose.** New `Card::hasLegalChoicesHiddenAt` (default `hasLegalTargets`), asked by the hidden generator for a spell with no declared targets; Resonating Strike 821 overrides it. Test: `HiddenPlay_AFinalizePassSpellNeedsSomethingToChooseAtItsBattlefield`.
- Fox-Fire 253: NOT A DEFECT after the verifier's rewrite — "any number" includes zero (CR 355.13), so it is playable with nothing to choose. Pinned by `HiddenPlay_FoxFireMayBePlayedWithNothingToChoose`.
- Facebreaker 220 and Switcheroo 466 declare targets and were already restricted. Moonfall 755 and Rocket Barrage 400 are not [Hidden].

**9.N5 — Weaponmaster and card-less Equipment.** `weaponmasterChoices` / `weaponmasterEquip` refuse an object with no card (CR 821.1.c.4). Seventeen tests relied on such gear; the ten helpers that built it now give it a card through `withEquipmentCard` (fixture, Long Sword). Test: `Weaponmaster_AnEquipmentWithNoEquipCostCannotBeEquippedThisWay`.

**9.M26 — typed discard cost.** Tests only: `TypedDiscardCost_SkyCrusierDiscardsTheGearThePlayerPicks`, `TypedDiscardCost_MelEmpowersByDiscardingTheSpellThePlayerPicks`. The batch-07 "847 — BLOCKED" sub-bullet is corrected and the S5 list in `plans/08` has a closing sub-bullet.

**9.M27 — base-only runes.** Test only: `PayEnergy_ExhaustsTheLowestIdBaseRuneAndNoRuneOutsideTheBase`, through Jax, Unrelenting's `payEnergy`.

**9.R2 — paid-flag residue.** One key for every card, `Card::kAdditionalCostPaid` (Zaun Punk 481, Bard 540, Safety Inspector 726 use it), and one helper that forgets it and any `optionalAdditionalCost` `paid_flag`: `Card::forgetPlayPayments`, called as every play begins.
- The engine has no single zone-change function to hang this on, so the start of a play is the shared point. It has five call sites: `executePlayCard`, `executePlaySpell`, `executePlayFromHidden`, `payAdditionalPlayCosts` (reaction and paying effect plays), `EffectExecutor::playIgnoringCost`.
- Tests: `PaidFlag_ZaunPunkReplayedWithoutPayingDoesNotKillAGear`, `PaidFlag_SafetyInspectorReplayedWithoutPayingStillKillsItsOwnUnit`. Both do: pay, trigger countered by a real [Reaction] test counter, bounce, replay without paying.

## 3. Red against mutated code

Three mutated builds, each running `SweepCostsTest.*`; in each, exactly the listed tests failed.

| Mutation | Tests that went red |
|---|---|
| grant cost ignores target discounts | both `TrashReplayGrant_ATargetsDiscount...` |
| hidden play-time target marker removed | `HiddenPlay_HasNoCostGate...` |
| pre-pass choices not counted toward the cost | `FinalizePassSpell_ADiscountFromWhatItChooses...` |
| typed discard ignores the pick | both `TypedDiscardCost_...` |
| Weaponmaster offers card-less Equipment | `Weaponmaster_AnEquipmentWithNoEquipCost...` |
| `runesInBase` counts any rune | `PayEnergy_...` |
| `forgetPlayPayments` disabled | both `PaidFlag_...` |
| choice filter always true | `FinalizePassSpell_AUnitIsOfferedOnlyIfItsDeflect...` (and the discount test) |
| hidden-choice check disabled | `HiddenPlay_AFinalizePassSpellNeeds...` |

The last row was run against the Fox-Fire version of that test. After the merge I rewrote it to use Resonating Strike and did not re-run the mutation.

## 4. Does a cost path execute an intent the generators would not offer?

I found none. Two things in the other direction:
- A finalize-pass spell offered only because a choice would make it payable can still end unpayable if the player's earlier choices rule that object out (Resonating Strike: pick the battlefield Irelia stands at, and she is no longer "at a different location"). `executePlaySpell` then puts the card back and plays nothing. Any Deflect already paid for a choice in that pass is not refunded.
- My tests execute some hand-built intents the generators would not emit (a `needsPlayTimeTarget` spell with explicit targets). If 9.G5 validates applied intents, those tests will need the generator's form.

## 5. New defects found, not fixed

- `Card::mayChooseAtFinalization` defaults to "is in `enumerateLegalTargets`". 20 of the 32 finalize-pass spells declare no targets and do not override it, so they are not offered when only a choose-me discount would make them payable. Once offered, the discount does apply. Only Resonating Strike overrides today.
- The abort path above: a play that is offered and then does nothing, with unrefunded Deflect.
- The choice filter judges one object at a time for trash-grant plays (discounts from earlier choices in the same pass are not carried); for normal plays it carries them.
- `EnginePathAgent::script` falls back to the first option when a value does not match; several of my tests rely on that for trailing prompts (Mel's Empowered trigger, location picks). Under the strict picker these may need announcing.
