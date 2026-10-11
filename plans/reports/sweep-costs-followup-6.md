# sweep-costs — follow-up 6 (plans/09 section HH: 9.HH2, 9.HH3)

Branch `fix/sweep-costs`, head `0cbed4d` (plus the commit that adds this report).
**Verified against integration `9a3e37a`** (merged before the final run; one conflict, in `plans/09`, both streams' ticks kept).

## 1. Result

| Gate (clean rebuild, `-j3`, lane A lock) | Result |
|---|---|
| Build, all four targets | OK, **0 warnings** |
| `riftbound_tests` | **2899 passed**, 0 failed |
| `riftbound_openspiel_tests` | 25 passed |
| clone-equivalence | 10/10 |
| smoke, 200 games, seed 20261010 | rc 0, P1 139 / P2 61, no engine error |
| MCTS batch | rc 0 |
| fault injection (`RIFTBOUND_STEP_FAULT_AT=40`) | rc 1, as wanted |

| Item | State |
|---|---|
| 9.HH2 | done — the check is an error with no switch; 12 cards brought under the rule (11 card files and `splitDamageAmong`) |
| 9.HH3 (a) | done |
| 9.HH3 (b) | done, with one difference from the item's wording (section 4) |
| 9.HH3 (c) | done — all four tests shown red |

## 2. 9.HH2 — the rule, and the check as an error

**Rule** (written at `Card::finalizingChoices` in `src/cards/card.cpp` and in CLAUDE.md, "Where a finalize pass may end"): a finalize pass ends

1. at the marker, or
2. at a return taken because a choice helper stored "nothing to decide" on the chain item: `confirmOptional` 0, `pickMode` -2, `pickTarget` / `pickTargetPair` / `selectObject` / `selectObjectAs` `kInvalidId`, `splitDamageAmong` with nothing to target.

Any other return before the marker is a `std::logic_error` naming the card, thrown by `GameEngine::collectFinalizeChoices`. `setStrictCardContracts`, `cardContractViolation` and `RIFTBOUND_REPORT_CARD_CONTRACTS` are removed.

**What the engine actually checks.** The helpers call `Card::nothingToDecide(item)`; the pass is accepted if it reached the marker or if any helper stored "nothing to decide" during it. That is a necessary condition, not proof that the return was the helper's: a card that has a declined "may" and then returns on a test of its own passes the check. The engine cannot see which `return` was taken.

**Cards.** Of the 33 on the follow-up 5 list, 29 trip nothing once rule 2 exists. I listed every `return` ahead of the marker in each of those 29 files and each is a helper's answer (`conf < 1`, a `kInvalidId` pick); I did not re-read the files in full. Four still tripped, and the new registry-level test found eight more that the suite never reached. All twelve were changed the same way: the card's own test now feeds the helper (an empty pick list, a false `still_legal`) and the card returns after its marker.

| Card | Returned before the marker on | Now |
|---|---|---|
| Nami, Headstrong 614 | `__nami_paid` not set | empty pick list |
| Blast Corps Cadet 336 | `__blastcorps_paid` not set | empty pick list |
| Frostcoat Cub 390 | `__frostcoat_paid` not set | empty pick list |
| Masa, Crashing Thunder 907 | `__masa_paid` not set | empty pick list |
| Gust Monk 888 | `__gust_monk_paid` not set | empty pick list |
| Akshan, Mischievous 431 | `__akshan_paid` not set | empty pick list |
| Vex, Mocking 617 | stunned unit missing | `still_legal` false |
| Stealthy Pursuer 177 | moved unit missing | `still_legal` false |
| Loyal Pup 447 | `__defend_bf` not recorded | `still_legal` false |
| Brush 561 | slot not replaced | `still_legal` false |
| Curator of the Sands 932 | less than 7 Energy spent, or subject not a unit/gear | `still_legal` false |
| Volibear, Furious 41 | (in `splitDamageAmong`) nothing to target | the helper calls `nothingToDecide` |

None of the twelve changed what a game does: each test was on something that cannot differ between finalization and resolution (a per-play flag, the event's subject, the Energy spent). They were not harmful early ends; they are brought under the one rule. The Curator hunk is at the top of `onTrigger`, away from the readied-count trace line, and merged cleanly with sweep-verify-cards' edit.

**Registry-level test.** `SweepCostsTest.CardContract_NoRegisteredCardEndsItsFinalizePassAnywhereElse` runs all 231 opted-in cards through the engine: 100 spell plays, 1592 trigger firings (each trigger type; no subject, a friendly unit, an enemy unit), 4 equipped-trigger firings, 30 activations — alone and on a full board, first option and last option. It also requires that no run throws anything else; none does.

What it does not reach:
- Paths that need a specific board (a particular counter, zone or opponent action). The suite covers some of those; nothing proves all.
- Guards that cannot be taken in a game stay ahead of the marker in many cards: `if (!ctx.state.objectExists(ctx.source)) return;` (objects are never erased) and Emperor's Dais 521's "my slot was not found". If one were ever taken it would now be an engine error.

## 3. 9.HH3 (a) — the probe's bound

`prePlaySequenceExists` throws a `std::logic_error` naming the card when it has made 256 runs, sequences remain and none is payable. `prePlayProbe` restores the state, the choice filter and the bus before passing it on. Test: `FinalizePassSpell_ReachingTheProbeBoundIsAnErrorNotAWithheldPlay` (test card with 512 sequences).

**Found while doing it, fixed:** `runScriptedPass` caught every exception of the card's finalize pass and counted the sequence as "not payable". A card breaking a contract inside the probe (now including the 9.HH2 error) was therefore silently not offered instead of reported. It now passes the exception on. Test: `FinalizePassSpell_AContractErrorInsideTheProbeIsNotAWithheldPlay`. No suite test and none of the 200 smoke games depended on the old behaviour.

## 4. 9.HH3 (b) — repeatability

The contract is stated above `GameEngine::choosePrePlayViable`, in `game_engine.h` and in CLAUDE.md. `FinalizePassSpell_APassThatIsNotRepeatableMakesTheViablePassThrow` pins it with a test card that offers a unit the first time it runs and nothing afterwards.

Difference from the item: the test card makes the viable pass throw `was offered with no payable sequence of choices`, not `did not complete the choices it had just made`. Which of the three throws fires depends on which run diverges; all three messages now say "not repeatable" and the test asserts on that, the function name and the card name. I did not build a card that hits the final-run throw specifically: it would have to count the engine's runs, and 9.HH4 (perf-engine) is expected to change that count.

## 5. Mutations (9.HH3 (c) and the new tests)

Three mutation builds, each restored with `git checkout -- src tests`.

| Mutation | Tests that went red |
|---|---|
| Fingerprint: Power summed over Domains, `spell_power + gear_power` summed, `champion_zone` dropped | `CostFingerprint_SeesEachPoolAndTheChampionZone` (all three assertions) |
| Fingerprint: battlefield controller and facedown count dropped | `CostFingerprint_SeesABattlefieldsFacedownCardsAndController` (both assertions) |
| Fingerprint: `card_counters.size()` added | `CostFingerprint_DoesNotMoveWithACardsOwnCounters` |
| `EventBus::beginQuiet` no longer holds | `FinalizePassSpell_ProbingAnOfferLeavesNoTrace`, `...ReachingTheProbeBound...` |
| Bound throw disabled | `...ReachingTheProbeBoundIsAnErrorNotAWithheldPlay` |
| `runScriptedPass` swallows again | `...AContractErrorInsideTheProbeIsNotAWithheldPlay` |
| "No viable option" throw turned into `break` | `...APassThatIsNotRepeatableMakesTheViablePassThrow` |
| End-of-pass check disabled | `CardContract_AFinalizePassThatEndsAnywhereElseIsAnError`, `...AContractErrorInsideTheProbe...` |
| `splitDamageAmong` without `nothingToDecide`; Masa's early return restored | the registry test (names Masa and Volibear), the three Volibear / split-damage pins |

`CardContract_APassMayEndAtTheMarkerOrAtADeclinedMay` has no mutation run of its own in this follow-up; before `nothingToDecide` existed the strict check tripped on the declined-"may" cards (the 33).

## 6. Things to know

- **The probe bound can now end a real game.** A registered spell on a board with more than 256 unpayable sequences ends the game as `engine_error` instead of being quietly not offered. It did not happen in the suite, the 200 smoke games or the 200-game Irelia batch (231 short plays, 0 no-op plays, unchanged).
- Test card ids used by my file are now 1048–1059.
- 9.HH4 (the probe's `GameState` copies) is untouched; the mechanism is as it was apart from the two throws.

## 7. New defects found, not fixed

None left open. The one found (section 3, the swallowed exception) is fixed and tested.

## 8. Files

- `src/cards/card.cpp`, `card.h`, `card_helpers.h` — rule comment, `nothingToDecide`, `splitDamageAmong`
- `src/engine/game_engine.cpp`, `game_engine.h` — end-of-pass error, probe bound, repeatability contract
- eleven card files (table in section 2; Volibear's change is in the helper)
- `tests/cards/test_sweep_costs.cpp` — 4 new tests, 2 rewritten, test cards 1048, 1049, 1051
- `CLAUDE.md`, `plans/09-found-during-fix-run.md` (9.HH2, 9.HH3 ticked)
