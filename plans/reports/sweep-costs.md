# sweep-costs — final report

Branch `fix/sweep-costs`, head `3dac1ef`, worktree clean. `fix/integration` (`3b742d9`) is fully merged in: sweep-flow, sweep-triggers and its follow-up, cleanup and its follow-up, perf-search, sweep-verify-cards tranches 1-3. Merging the branch into integration should be a fast-forward.

## 1. Result

Clean rebuild of the merged tree with the safe launcher (`ccache_pch_safe.sh`, namespace `sweep-costs`), lane A, `-j3`:

| Check | Result |
|---|---|
| `riftbound_tests` | 2488 run, 2488 passed |
| `riftbound_openspiel_tests` | 20 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games fiora vs yi_rebuild, seed 20261010, 3 threads | rc 0, P1 137 / P2 63 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |

The fault-injection run of the gate was not run here.

Build notes: I switched to the safe launcher when told and did a `clean` then; every number above is from a clean rebuild after the last merge. The process hosting me was killed once mid-run; nothing on disk was lost.

## 2. What was done, by assignment row

All new tests are in `tests/cards/test_sweep_costs.cpp` (`SweepCostsTest.*`, 43 tests) unless another fixture is named.

**1. Equip-cost descriptor (7.7, Weaponmaster, Piltovan Forge 948, re-equip)**
- A gear declares its Equip cost: `Card::equipCost()`, `canPayEquipExtraCost` / `payEquipExtraCost` for non-resource parts, `onEquipped`. `Card::canPayEquip` is the one gate and `Card::payEquipAndAttach` the one payment. No gear overrides `onEquip` any more; `standardEquip` is deleted.
- The Equip generator no longer offers an Equip that cannot be paid. Tests: `Equip_IsOfferedOnlyWhenItsCostCanBePaid`.
- Piltovan Forge's first-gear-ability discount and the per-turn counter apply to Equip (`GameEngine::equipTerms`). Test: `Equip_FirstGearAbilityDiscountAppliesToTheEquipCost`.
- Weaponmaster charges the declared cost one Power less, skips Equipment it cannot pay for, and can take attached Equipment. Jax 440 now derives from `WeaponmasterUnit`; its text parser is gone. Tests: `Weaponmaster_ChargesTheEquipCostLessOnePower`, `Weaponmaster_MovesAnAttachedEquipmentAndPaysANonResourceCost`, `RulesCards2Test.RulesCards2_Jax_WeaponmasterChargesANonResourceEquipCost`.
- Re-equip of attached Equipment through its own Equip: NOT A DEFECT. Attached gear's rules text is inactive (CR 718.2, 721.2); only Weaponmaster moves it. Stated at `Card::canPayEquip` and in the generator; pinned by `Equip_AttachedEquipmentIsNotOfferedAgain`.
- A trigger queued by an Equip now resolves with it (also fixed by sweep-flow; both guards are in). Test: `Equip_ATriggerItCausesResolvesWithTheEquip`.

**2. Alternative cost for spells and pre-payment additional costs**
- New hooks `Card::additionalPlayCost()` and `Card::payAdditionalPlayCost(ctx, affordable)`, called after the card leaves its zone and before its cost is paid, on hand plays, reaction plays and paying effect plays. `canAfford` counts them; a mandatory cost that cannot be paid removes the play from the legal actions.
- Cards converted: Brazen Buccaneer 2, Wallop 146, Kraken Hunter 150, Call to Glory 207, Cruel Patron 208, Commander Ledros 231, Legion Quartermaster 367, Zaun Punk 481, Bard 540, Stalking Wolf 728, Undying Loyalty 730, Atakhan 732, Poppy 740. One test each, named after the card.
- Ezreal, Prodigy 470: `Card::optionalCostDiscount`, applied to Accelerate, Repeat and "you may pay". Test: `Ezreal_OptionalAdditionalCostsCostOneLess`.
- Irelia, Graceful 462: the target discount is counted before payment, [1] or one Power. Test: `PlayGate_ADiscountFromTheChosenTargetCountsBeforePayment` (Energy half only; I found no cheap targeted spell with a Power cost to test the Power half through the engine).
- Sky Crusier 847 was already correct as an activated ability with a typed discard; no change.

**3. Restricted resource pools (294, 506, 786, 312)**
- `RunePool::spell_energy`, `showdown_energy`, `spell_power`, `gear_power`, spent first and only for the matching `SpendFor`. All non-card-cost payments go through the new `engine/resource_payment.h`.
- Tests: `RestrictedPower_SpellOnlyPaysForSpellsOnly`, `RestrictedEnergy_SpellOnlyPaysForSpellsOnly`, `RestrictedPower_GearOnlyPaysForGearAndGearAbilities`, `RestrictedEnergy_ShowdownOnlyIsUnusableOutsideAShowdown`, `RestrictedResources_EmptyWithThePool`.

**4. Card-declared Repeat (8.9: 579, 743) and Power on granted Repeat (401, 772)**
- `Card::repeatCosts()`; all 18 printed Repeat spells declare theirs. `parseRepeatCost` and `payRepeatCost` are deleted, so the engine no longer reads `ability_text` anywhere (ticks 5.4).
- Square Up repeats for a discard, Curtain Call has three costs each payable once, and "[Repeat] equal to its cost" charges Power. Tests: `C3Test.SquareUp_RepeatIsPaidWithADiscard`, `CurtainCall_EachRepeatCostIsOfferedOnceAndRunsAnotherMode`, `GrantedRepeatEqualToItsCost_ChargesThePowerToo`, `JhinDeckTest.RepeatCost_*`.

**5. Play from trash (587, 809), trash-replay gates and discounts, Death Mark 931**
- `Card::trashPlayCost()` gives Undying Legion a standing permission. Test: `UndyingLegion_IsPlayedFromTheTrashForItsLegionCost`.
- Endless Riches: any card type is offered from the trash at its own cost, and new `engine/trash_replacement.h` (`settleInTrash`) banishes what would go to the trash from anywhere but the Main Deck. It is called at ten sites (three kill sites, both discard paths, resolved and countered spells, removed facedown cards, the counter helper, Virtuoso). Tests: `EndlessRiches_CardsArePlayedFromTheTrashAtTheirOwnCost`, `EndlessRiches_WhatWouldGoToTheTrashIsBanishedExceptFromTheDeck`.
- Grant costs use one function for the generator and the payment (`trashGrantCost` / `canPayTrashGrant`), with discounts applied. Death Mark's [A][A] may come from two Domains. Test: `DeathMark_FlowPaysTwoAnyPowerFromTwoDomains`. There is no dedicated test of a discount applied to a grant cost.

**6. Hide-cost hook (259)** — `Card::adjustHideCost`, `GameEngine::hideCostFor` / `canPayHideCost`, used by generator and execution. Test: `SwiftScout_HidesForOneEnergyInsteadOfThePower`.

**7. 2.11 root cause** — `canAffordChoosing` checks the spell's cost and its targets' Deflect together. Test: `PlayGate_ATargetWhoseDeflectCannotBePaidWithTheCostIsNotOffered`.

**8. Smaller items**
- `EffectExecutor::payAnyPower` delegates to `resource::pay`; no dedicated test, existing users cover it.
- `payEnergy` / `payableEnergy` use `resource::` and count base runes only.
- "To a minimum of [N]" not raising a cheaper card: NOT A DEFECT (CR 356.4.e). Already stated at the code site and pinned by `AurasCostsTest.MinimumCost_LimitsTheReductionOnly`.
- Risen Altar 950: `Card::activationPowerReductionFor`; the ability generators and `payAndQueueAbility` share `canPayActivation`. Test: `EmpowerDiscount_TakesAPowerWhereThereIsNoEnergy` (card-level, not through an activation).
- Aphelios 372 chooses which two runes to ready. Test: `Aphelios_ReadiesTheTwoRunesThePlayerChooses`.
- Undying Loyalty 730: unit chosen as the spell is played, discount follows the choice. Test: `UndyingLoyalty_TheDiscountFollowsTheUnitChosenAsItIsPlayed`.

**Plan 09 items given to me, all ticked**
- 9.A4 `costDueNow`: the end-of-choices marker now also ends the cost section, so a cost is due once per chain item on every path. Cards that prompt inside their cost section (Garbage Grabber, Dusk Rose Lab and four others) still work. Tests: `CostDueNow_IsPaidOnceWhenAPromptFollowsIt`, `CostDueNow_IsPaidOnceWhenTheCardIsPlayedByAnEffect` (the second was red before the fix).
- 9.A24 Weaponmaster: choices at finalization (`kFinalizeTrigger`), attach through the executor. Lucian, Riven, Yone, Akshan and Jax branches carry the marker. Tests: `Weaponmaster_TheMayAndTheEquipmentAreChosenAtFinalization`, `Weaponmaster_TakingAnAttachedEquipmentAnnouncesTheUnattach`.
- 9.D6 `GameEngine::attachGearToUnit` deleted; Quick-Draw uses `EffectExecutor::attachGear`. Test: `QuickDraw_AttachesThroughTheExecutorsAttachPath`.
- 9.D13 my `spendBuff` helper deleted; cards call `ctx.executor.spendBuff(unit, ctx.controller)`. Test: `BuffSpentAsACost_IsCreditedToThePlayerWhoSpentIt`.

**Sand Soldier Weaponmaster (8.4 remainder)** — I built a card-side version, then dropped it for sweep-triggers' keyword ability, which derives from `WeaponmasterUnit` and so charges the cost. Test: `SandSoldier_HasWeaponmasterFromTheEmperor`.

## 3. Plan boxes ticked

- `plans/09`: 9.A4, 9.A24, 9.D6, 9.D13.
- `plans/05`: 5.4. `plans/07`: 7.6, 7.7. `plans/08`: 8.7, 8.9.
- Batch files: 948, 950 (b00); 809, 294/506/786 (b01); 146/207 (b02); 579, 743, 931 (b03); 2, 150 (b04); the Weaponmaster six, 231, 312 (b05); 587 (b06); 732, 740 (b07); 259 (b08); 470, 440 (b10).
- I did not edit `STATUS.md` carry-over rows.

## 4. Existing tests corrected

Each has a comment saying why.
- `test_jhin_deck.cpp`: four `ParseRepeatCost_*` tests now read `Card::repeatCosts()`.
- `test_review_fix_c3.cpp`: `SquareUp_DoesNotForceADiscard` expects the Repeat offer.
- `test_rules_compliance_cards2.cpp`: the Jax "cost it cannot charge" test became `..._WeaponmasterChargesANonResourceEquipCost`.
- `test_champions_legends.cpp`, `test_audit_fixes_5.cpp`: Fire Below the Mountain adds gear-only Power.
- `test_review_fix_sweep_flow.cpp`: the test gear declares `equipCost` instead of overriding `onEquip`.
- `test_review_fix_c7.cpp`: Stalking Wolf no longer requires the opponent to get priority (the verifier's added check), since the kill is now a play cost with no chain item.
- `test_equipment.cpp`: took integration's engine-path Weaponmaster helper.

## 5. Notes for other streams

- **Trainer**: prompts changed for 2, 146, 150, 207, 208, 231, 367, 440, 481, 540, 728, 730, 732, 740 (additional-cost prompts now come before payment), every Weaponmaster unit (may and Equipment at finalization), Square Up and Curtain Call (Repeat offers), Aphelios 372, and Hide with Swift Scout when both payments are possible.
- **Observation tensor**: the four new restricted pools are not in `extractStateFeatures`, following `unit_energy`, which is not either. `kStateFeatureDim` is unchanged. This is a decision for whoever owns the feature layout.
- **CLAUDE.md** (worktree copy) updated: cost payment, restricted resources, additional costs, Equip, Weaponmaster, Repeat, the other cost hooks, the `costDueNow` rule.

## 6. New defects found, not fixed

- `Card::needsEquipTimeTarget()` is overridden by no card; the one-intent Equip branch in the generator and `payAndQueueAbility` is dead code.
- A card that pays a cost under `costDueNow` and then prompts again *before* its marker, inside a finalize pass, would still pay twice. No current card has that shape (I checked the 13 files where a prompt follows `costDueNow`); the contract in `card.h` says costs go immediately before the marker.
- `tests/cards/test_equipment.cpp` `tryEquip` calls `Card::onEquip` directly with a context that has no registry; it works only because `onEquip` needs none.
- Irelia, Graceful 462: the "[A] less" half of her discount has no engine-path test.
- `EffectExecutor::killObject` returns early on `zone == Trash` ("already dead"); a unit banished on death by Endless Riches has zone Banishment, so that guard does not cover it. A second kill is stopped by the missing location, but the comment is no longer the whole story.
