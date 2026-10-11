# sweep-costs follow-up 3 — report

Branch `fix/sweep-costs`, head `c8020b3`, worktree clean. **Verified against integration `761afa9`** ("status: sweep-verify-cards ninth tranche verified"), which is merged in; integration had nothing newer when I committed.

## 1. Result

Clean rebuild at the merged tree, safe launcher, lane A, `-j3`. **Zero warnings in the build log.**

| Check | Result |
|---|---|
| `riftbound_tests` | 2795 run, 2795 passed |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 3 threads | rc 0, P1 139 / P2 61 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, failure named, not a draw |

## 2. Items — 9.AA1 to 9.AA5, all ticked

**9.AA1 — every finalize-pass spell declares what it can choose.**
- `Card::mayChooseAtFinalization` now returns `FinalizeChoice::Undeclared / No / Yes`; the base says Undeclared. All 34 finalize-pass spells declare it, most through a new `friendlyUnitChoice` helper in `card_helpers.h`.
- A registry test fails for any such spell left Undeclared: `EveryFinalizePassSpellDeclaresWhatItCanChoose`.
- Engine-path test on a second spell: `FinalizePassSpell_ASpellWithNoDeclaredTargetsIsOfferedThroughItsDeclaration` (Curtain Call). I first wrote one with Challenge (`..._ASecondSpellIsOfferedThroughADiscount`); the mutation showed it does not exercise the declaration, because Challenge declares its targets and is offered through them. I kept it and added the Curtain Call one.
- The 34 declarations were written from each card's printed text and the shape of its pick, not from a line-by-line read of every `onResolve`. A wrong "Yes" costs an offered play that is then undone (9.AA2); a wrong "No" costs a missed offer in the discount-only corner.
- **Bug found on the way and fixed:** `targetCostDiscount` gave the "choose me" discount for choosing the *opponent's* Irelia. It is now the controller's only. Test: `TargetDiscount_BelongsToTheControllerOfTheChosenUnit`.

**9.AA2 — an attempt that ends unpayable is undone completely.**
- When the spell is too dear on its own, `executePlaySpell` keeps a copy of the state from before the pass. If the choices do not make it payable, the state is restored whole (Deflect paid, chosen-object bookkeeping, the chain id taken) and the card goes back to its zone. Only the decision record (`action_history`, `decision_index`) is kept, so a replay asks the same questions.
- I chose rollback over "never offer": the engine cannot know which of the player's own non-object choices (a mode, a battlefield) will rule the discount out. So such a play can still be offered and end with nothing played.
- The state copy is taken only in that case (plain cost unaffordable), not on every spell play.
- Test: `FinalizePassSpell_AnAttemptThatEndsUnpayableIsUndoneCompletely`.
- The 9.T2 mutation was re-run against the rewritten Resonating Strike test: red.

**9.AA3 — grant plays carry earlier discounts.** `canPayTrashGrant` has an overload taking `unpaid_deflect`; the choice filter passes everything chosen so far plus the candidate, as it does for normal plays. The test also found that the trash generator's pair branch did not offer a pair-picking spell that a choice makes payable; fixed. Test: `TrashReplayGrant_ALaterChoiceKeepsTheDiscountOfAnEarlierOne`.

**9.AA4 — `EnginePathAgent::script` is strict.**
- An entry that matches no option fails the test; so does a prompt after the script has run out.
- Two markers say the old fallback out loud: `kFirst` (this prompt: first option) and `kRest` (this and every later prompt).
- Pinned by four `FixtureContractPickers.Script*` tests in `tests/cards/test_fixture_contract.cpp`.
- 22 tests relied on the fallback: 8 of mine and 14 in other streams' files (`test_audit_fixes_6`, `test_review_fix_c0/c1/c4/c7`, `test_review_fix_verify_cards`). All converted.
- Most conversions append `kRest` to a script that ran out. That makes the reliance visible but does not check the trailing prompts; I did not write out what each of the 14 foreign tests' later prompts should be.
- One of mine, the Curtain Call Repeat test, had been passing on the fallback with a wrong script (its entries were in the pre-9.N2 order). It now has the right one.

**9.AA5 — warnings.** The 16 unused `Card*` variables and one unused `exec` parameter left by the 9.L1 conversion are removed.

## 3. Red against mutated code

| Mutation | Tests that went red |
|---|---|
| declaration ignored in `payableThroughAFinalizeChoice` | `..._ASpellWithNoDeclaredTargetsIsOfferedThroughItsDeclaration`, `..._ADiscountFromWhatItChooses...`, `..._AnAttemptThatEndsUnpayable...` |
| state not restored on the abort | `FinalizePassSpell_AnAttemptThatEndsUnpayableIsUndoneCompletely` |
| grant filter judges one object | `TrashReplayGrant_ALaterChoiceKeepsTheDiscountOfAnEarlierOne` |
| discount for any controller | `TargetDiscount_BelongsToTheControllerOfTheChosenUnit` |
| hidden-choice check disabled (9.T2 re-run) | `HiddenPlay_AFinalizePassSpellNeedsSomethingToChooseAtItsBattlefield` |

Not mutation-run: the registry test (it fails by construction for an Undeclared spell) and the four fixture-contract tests (they assert the failure directly with `EXPECT_NONFATAL_FAILURE`).

## 4. New defects found, not fixed

- A finalize-pass spell can still be offered and end with nothing played (9.AA2's remaining case). A random agent can pick it repeatedly; each attempt costs decisions but changes no state.
- The rollback restores `GameState` only. Anything outside it that reacted to events during the pass (for example `TriggerManager` bookkeeping for a rune recycled to pay Deflect) is not rolled back. I know of no such state that matters, but did not audit it.
- The 14 foreign tests converted with `kRest` no longer hide the fallback but still do not assert their trailing prompts.
- `mayChooseAtFinalization` and the card's real pick list can drift apart; nothing ties them together except the registry test for "declared at all".
