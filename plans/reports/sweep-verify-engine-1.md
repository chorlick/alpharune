# sweep-verify-engine — interim report 1 (2026-10-11)

## 1. Result

- Branch `fix/sweep-verify-engine`, head `29c95f2`, `fix/integration` merged in at `abce3eb` (one conflict, plans/09, resolved).
- Incremental build after the merge (not a clean build; that is for the final run).
- `riftbound_tests`: 2579 passed, 0 failed. `riftbound_openspiel_tests`: 24 passed (ResumeStress included). `riftbound_clone_equiv_test`: 10/10. Smoke 200 games rc=0, 0 draws. Fault injection: rc=0 without, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`.
- Warnings: 0 in engine targets (`riftbound_tests`, `riftbound`, `riftbound_openspiel_tests`, `riftbound_clone_equiv_test`, `riftbound_search_microbench`). The full rebuild right after the merge showed only the two 9.F2 warnings, now removed.
- Still to do in this stream: 9.M1–9.M12, 9.N3 (and 9.O1's engine reader if it comes).

## 2. Items closed

All FIXED unless noted. Red-first: two mutation builds (fixes disabled by file backup), every new test listed was red, then green again.

- **9.G4** `GameEngine::askAgent` is the one place an agent is asked; the answer must be one of the options offered. **Decision: throw** — `std::invalid_argument`, as `applyChoice` throws `std::out_of_range` for a bad index. Concede is always accepted (agents answer it on an empty list or on shutdown).
- **9.G5** Same check, every intent type, every decision. Plus `payAndQueueAbility` refuses an activation whose source is not in play / not the player's / fails `canActivateAbility[At]` (this is what stops the test seam `testHook_executeIntent`, which bypasses decisions by design; said so at the hook).
  - Cost: not measurable. 200-game batch, 1 thread: 0.59 s with the check, 0.58 s without (3 runs each). `clone_advance_microbench` ApplyAction median 112–114 µs with, 111–112 µs without (replayed); 88 µs both (resumed). Run-to-run noise is larger than the difference.
- **9.I6** `resolveSpell` hands `onTrigger` / `onEquippedTrigger` a fixed empty list.
- **9.C3** A stale intent is refused. An edit through `mutableState()` while a decision is pending is noticed; the answer must then be in the rebuilt list (main phase, showdown, priority) or, for a nested decision, name no object the edit moved / removed / gave away. The nested list itself is not rebuilt (it cannot be).
- **9.A19** `AuraEffect::only_if_absent`, decided after every aura hook. Spirit's Refuge 63 fixed; batch-08 corrected (**9.G10**).
- **9.A21** `CostModifier::from_hand_only`; Jayce 406.
- **9.A13** `ActivationCost::banish_self`, paid at activation; The Zero Drive 412.
- **9.G1** `battlefieldTakesUnitsFrom` in the location helpers; masks in 704, 515, 475 and Arise! 513's private copy deleted.
- **9.G8** `Card::killAsCost` / `markCostUnpaid` / `costUnpaid`; 765 and 449 use them. `killObject` untouched.
- **9.G12** `boardObjects` + sorted `chooseableUnits`; 13 cards + 108 + 406.
- **9.A22, 9.D3, 9.C5** every 12 / 10-pick cap removed (helpers, 878, 153, 253); tests with 14 picks.
- **9.F2** all three dead code, removed (`stepFinalize` now void). **9.F3** card.h parameter names.
- **5.13, 5.14** remainder: see sub-bullets in plans/05. `contested_by` / `combat_staged` after combat **was a real defect** (`combat_staged` stayed true for the decisions asked before the caller cleared it, visible in the feature vector).
- **8.16** every bullet verified with its test; only a stale comment in Death Mark 931 was left.

## 3. Items not closed

- 9.M1–9.M12, 9.N3: not started.

## 4. Needs another stream

- `shared:` commit `7505633`: `ActivationCost::banish_self` in `src/effects/effect_types.h` (one field).
- `src/core/events.cpp`: two lines removed with `CleanupNeededEvent` (events.h is mine, the .cpp only forwarded).
- `src/engine/chain_manager.{h,cpp}` and `trigger_manager.cpp`: the 9.F2 edits (5 lines), made after the merge as instructed.
- Test files of other streams, corrected because they asserted the old behaviour (reason at each test):
  - `tests/cards/test_review_fix_verify_cards.cpp`: four `*_DoesNotOfferABattlefieldUnitsCannotBePlayedTo` (options are now numbered over what is offered: base = 0, battlefield #1 = 1); `PromisingFuture_...BanishesTheTopCard` → `..._IsRefused`; the Spirit's Refuge order test tightened to cost 1.
  - `tests/cards/test_rules_compliance_cards.cpp`: Alpha Strike large amount (24 picks, not ≤ 12); the helper's guard 16 → 1024.
  - `tests/cards/test_audit_fixes_0.cpp`: Zero Drive activation goes through the engine.
- **9.M31** (owner sweep-verify-cards) is already done here: Fox-Fire 253 and Overt Operation 153 caps removed, tests `VerifyEngineTest.FoxFire_KillsMoreThanTwelveZeroMightUnits` / `OvertOperation_AsksAboutMoreThanTwelveBuffedUnits`. Same for the Death Mark part of **9.M29**.
- **cleanup (play_server.cpp)**: `recomputeLegalForParkedHuman` still rebuilds only the main-phase list. The engine now checks a showdown or priority answer against the rebuilt list after an edit, so a human who clicks a stale option there gets the game aborted with `std::invalid_argument` instead of the option silently executed. Calling `engine_.recomputePendingLegalActions()` there (the comment at the function already asks for it) would hand the human the fresh list. About 10 lines; not done because it needs a live-server test at a showdown.
- **CLAUDE.md (ledger)**: not documented yet — `askAgent` and the answer check, `mutableState()` during a decision, `only_if_absent` / `auraKeywordIfAbsent`, `from_hand_only`, `banish_self`, `killAsCost` / `costUnpaid`, `boardObjects`, `battlefieldTakesUnitsFrom`.

## 5. New defects found

- `tests/cards/test_review_fix_verify_cards.cpp:2689, 2844`: two `-Wdangling-else` warnings (seen before the merge; check they are gone).
- `card_helpers.h` `splitDamageAmong`: targets chosen at resolution, not finalization (now 9.M4).
- Brush 561's swap-back had no test before `VerifyEngineTest.Pin_Brush_WhenScoredRestoresTheBattlefieldItReplaced`.
- `GameEngine::runFinalizePass`: if an agent's answer is refused inside it, `chain.resuming` stays set. Harmless today (the game has failed and is never continued), noted in case a caller ever catches and continues.

## New state fields

`GameObject::AuraEffect::only_if_absent`, `PlayerState::CostModifier::from_hand_only`, `ActivationCost::banish_self`. None is in the feature layout. `CleanupNeededEvent` is gone from the event bus.
