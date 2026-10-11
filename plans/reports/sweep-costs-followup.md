# sweep-costs follow-up — report

Branch `fix/sweep-costs`, head `c7fddaa`, worktree clean, `fix/integration` fully merged in (nothing ahead on integration at the time of writing).

## 1. Result

Clean rebuild of the merged tree, safe launcher, lane A, `-j3`:

| Check | Result |
|---|---|
| `riftbound_tests` | 2502 run, 2502 passed |
| `riftbound_openspiel_tests` | 20 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games fiora vs yi_rebuild, seed 20261010, 3 threads | rc 0, P1 137 / P2 63 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |
| same batch with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "game 1 [seed=7]: RIFTBOUND_STEP_FAULT_AT: injected engine fault at decision 40" — a failure, not a draw |

## 2. Items

All four are ticked in `plans/09` section J. New tests are in `tests/cards/test_sweep_costs.cpp` (`SweepCostsTest.*`, now 48).

**9.J1 — dead one-intent Equip branch: removed.** `Card::needsEquipTimeTarget()` is deleted with the generator branch and the `payAndQueueAbility` branch; no card overrode it. An Equip intent always names the unit. `Card::onEquip` is no longer virtual (the engine's path is `payEquipAndAttach` with `equipTerms`). No new test: the existing Equip and Weaponmaster tests pass unchanged.

**9.J2 — cost contract enforced.** `GameEngine::runFinalizePass` throws `std::logic_error` naming the card when a run in which `costDueNow` said "due" ends with a pending choice and the state has changed since. "Changed" is a digest (`Card::costStateFingerprint`: pools, zone sizes, XP, score, and each object's zone / exhaustion / buffs / damage / Empowered / attachment), stored on the chain item as `ChainItem::cost_due_fingerprint`.
- Cards that prompt inside their cost section before paying (Garbage Grabber, Dusk Rose Lab and the like) do not trip it, because nothing has changed yet when they prompt.
- None of the real cards trips it across the full suite.
- Test: `CostContract_APromptAfterAPaidCostBeforeTheMarkerIsAnEngineError`, using a test card that violates the contract; it checks the message names the card.
- Limit: a cost whose payment changes nothing in the digest would not be caught. I know of no such cost.

**9.J3 — `runEquip` through the engine.** The helper in `tests/cards/test_equipment.cpp` (renamed from `tryEquip` by cleanup) now builds the Equip intent and executes it with `testHook_executeIntent`. Every test using it passes unchanged.

**9.J4 — the four missing tests.**
- Irelia, Graceful 462, "[A] less" half: `PlayGate_TheTargetDiscountTakesAPowerWhereThereIsNoEnergy`. It uses a test spell costing one Fury Power; the test says so and why.
- Risen Altar 950 through a real activation: `RisenAltar_EmpowerCostsOnePowerLessThroughARealActivation` (Gangplank, Naval at the Altar is offered and pays one [O]; one in base is not offered).
- `payAnyPower` after the delegation: `PayAnyPower_SpendsFloatingPowerThenRecyclesExhaustedRunesFirst` (all or nothing, floating first, exhausted rune before ready).
- Discount on a trash-replay grant cost: `TrashReplayGrant_ADiscountAppliesToTheGrantCost`.

**Defect found by the Irelia test and fixed.** `generateSpellActions` gated every spell on its undiscounted cost before looking at targets, so a spell playable only through a target's discount was never offered (my first-round Irelia test executed the intent directly and missed this). The generator now asks `canAffordChoosing` per target or target set, which also keeps out a target whose Deflect cannot be paid on top of the cost. When the plain cost is affordable and the targets add no Deflect it skips the extra check.

## 3. Red against mutated code

One mutated build (backups in the scratchpad, restored afterwards; `git status` clean), running `SweepCostsTest.*`: exactly these five failed and the other 43 passed.

| Mutation | Test that went red |
|---|---|
| contract check disabled in `runFinalizePass` | `CostContract_APromptAfterAPaidCostBeforeTheMarkerIsAnEngineError` |
| `canAffordChoosing` never tries the Power half of a target discount | `PlayGate_TheTargetDiscountTakesAPowerWhereThereIsNoEnergy` |
| `netActivationCost` skips the Power-side reduction | `RisenAltar_EmpowerCostsOnePowerLessThroughARealActivation` |
| `payAnyPower` returns true without paying | `PayAnyPower_SpendsFloatingPowerThenRecyclesExhaustedRunesFirst` |
| `trashGrantCost` uses the grant's raw cost | `TrashReplayGrant_ADiscountAppliesToTheGrantCost` |

9.J1 and 9.J3 are a deletion and a test-helper change, so there was nothing to show red.

## 4. Other changes

- `CostDueNow_IsPaidOnceWhenTheCardIsPlayedByAnEffect` now uses the non-opted test card: an effect-played spell does get a finalize pass, so the opted-in variant is the contract violation and belongs to the new test.
- `PlayGate_ADiscountFromTheChosenTargetCountsBeforePayment` also asserts the spell is offered by the generator.
- `CLAUDE.md` (worktree copy): one sentence on the enforced contract.

## 5. New defects found, not fixed

- 34 call sites in other test files (`test_audit_fixes_0/2/4/5/6/7`, `test_vendetta_E`, `test_rules_compliance_cards2`, `test_choice_helpers`) still call `Card::onEquip` directly, bypassing the generator gate and `equipTerms`. They test the printed cost correctly but not the engine path.
- `generateTrashReplayActions` and `generateHiddenPlayActions` were not checked for the same "plain cost before targets" gate that `generateSpellActions` had. The trash generator does call `canAffordChoosing` per target set; the hidden-play generator I did not read.
- The fault-injection run prints the failure but I did not check that the three games after the faulted one still ran; the gate only asks for a non-zero exit.
