# sweep-costs follow-up 5 — report

Branch `fix/sweep-costs`, head `63521e3`, worktree clean. **Verified against integration `48e39ca`** ("sweep-verify-cards twelfth report; stream finished"), merged in; nothing newer on integration when I committed.

## 1. Result

Clean rebuild of the merged tree, safe launcher, lane A, `-j3`. **Zero warnings.**

| Check | Result |
|---|---|
| `riftbound_tests` | 2889 run, 2889 passed |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games fiora vs yi_rebuild, seed 20261010, 3 threads | rc 0, P1 139 / P2 61 / 0 draws |
| MCTS batch (4 games, seed 7) | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, failure named, not a draw |

## 2. Status of the seven items

| Item | Status |
|---|---|
| 9.DD1 | done, ticked |
| 9.DD2 | done, ticked |
| 9.DD3 | done, ticked |
| 9.GG1 | done, ticked |
| 9.GG2 | done, ticked |
| 9.EE1 (engine half) | done; the check is an error |
| 9.EE2 (engine half) | **done as a check, but not an error by default** — 33 cards still trip it (section 5) |

## 3. Items

New tests are in `tests/cards/test_sweep_costs.cpp` (`SweepCostsTest.*`, now 81).

**9.DD1 — a discount-only play cannot end unpaid.**
- The play is offered only when a complete payable sequence of the spell's choices exists. `payableThroughAFinalizeChoice` first asks the declaration, then makes the choices on the side (`prePlayProbe`, bounded at 256 runs). Nothing of a probe is seen or kept: no event, no log line, no agent asked, state restored.
- In the real pass of a too-dear spell, each prompt offers only the options a payable sequence can still follow (`choosePrePlayViable`). So an offered play always completes and is paid.
- The 9.AA2 rollback, the 9.CC1 undone-attempt memory and the held play are removed; there is no undone attempt any more, so no `INTENT:` line of one either. `EventBus::hold` stays, with a new `beginQuiet / endQuiet` used by the probes.
- `executePlaySpell` is split in two (`finishPlaySpell`), because the viable pass re-runs choices from copies of the state and the references taken at the top would dangle.
- Irelia deck batch, 200 random games, seeds 1–200: **231 spells played with less Energy at hand than their printed cost, 0 plays that changed nothing** (before this change: 10 undone attempts).
- Tests: `FinalizePassSpell_AChoiceThatWouldLeaveItUnpayableIsNotOffered`, `..._IsNotOfferedWhenNoSequenceOfItsChoicesPaysForIt`, `..._ProbingAnOfferLeavesNoTrace`, `NoPlayChangesNothingOverTwoHundredRandomGames`.

Measured cost, before → after (three runs each):

| Measure | Before | After |
|---|---|---|
| search microbench, `LegalActions (live)` | 61.5–62.7 ns | 61.2–61.7 ns |
| search microbench, `legalIntents (live)` | 4.8–5.1 ns | 4.4–4.7 ns |
| microbench observation checksum | `94739f84565d4d16` | same |
| smoke, 200 games fiora vs yi_rebuild | 0.20 s | 0.20 s |
| Irelia deck batch, 200 games | 665–668 ms | 794–799 ms |

No measurable cost on decks without a choose-me discount; about 20% on the deck built to hit the case.

**9.DD2 — what the fingerprint digests.** Documented field by field at `Card::costStateFingerprint`. Added: an object's exact location, controller and stunned state; each resource pool on its own; the Champion Zone; each battlefield's controller and facedown count. Not digested, with reasons at the code: per-turn tallies, continuous-effect flags, derived values, and `card_counters` (helpers and cards write progress there during choices). With the undone-attempt memory gone, the fingerprint has one user again: the cost contract. Tests: three `CostFingerprint_Sees...` and `CostFingerprint_DoesNotMoveWithACardsOwnCounters`.

**9.DD3 — every mode reached.** The walk runs on two boards (the second adds Empowered units) and asserts every mode of the four modal spells was offered: Rocket Barrage 400, Curtain Call 743, Mesmerize 839, Sanction 822. It also asserts those four are exactly the spells among the 34 whose text says "Choose one". With the second board removed the test goes red on Sanction's second mode.

**9.GG1 — Predict keep options named.** `EffectExecutor::predict` and the look prompt, Scryer's Bloom 698 and Abandon 693 now give their keep option value 0 ("Keep it" / "Leave it"). `EnginePathAgent::isPredictRecycleOrKeep` recognises the new shape in the same commit, and `script()` matches an option named only by `chosen_card_def`. Two tests in other streams' files picked the keep option by its old shape and are corrected (`SweepTriggersTest::keepOnTop`, the Diana, Lunari verify test).

**9.GG2 — play location settled at finalization.** `pickEffectPlayLocation` stores mode 0 through a new `Card::settleMode` when there is nothing to choose. Tests, one per card: `EffectPlayLocation_FlameChompers / ImmortalPhoenix / SoulgorgerDoesNotAskAtResolution` (the opponent hands over a battlefield in response).

**9.EE1 — unnameable prompt option.** `GameEngine::askAgent` throws `std::logic_error` naming the resolving card when an all-MakeChoice prompt has an option with no value, object or `chosen_card_def`, or two options that are equal by those. Before 9.GG1 it tripped on prompts of 20 cards, most through the engine's own Predict keep option; after 9.GG1 and sweep-verify-cards' fixes **no card in the suite trips it**. Test: `CardContract_AnOptionWithNeitherValueNorObjectIsAnError`.

**9.EE2 — finalize pass ended before its marker.** `collectFinalizeChoices` detects it (`finalizingChoices` sets a bit; reflexive triggers are exempt). It is a `CARD-CONTRACT` warning in the log by default and a `std::logic_error` naming the card under `GameEngine::setStrictCardContracts(true)`. I did not make it an error by default because the suite would go red on the cards below. Test: `CardContract_AFinalizePassThatEndsBeforeItsMarkerIsReported`.

## 4. Red against mutated code

One mutated build; exactly these ten tests failed, the other 71 `SweepCostsTest` tests passed.

| Mutation | Tests that went red |
|---|---|
| offered on the declaration alone (no probe) | `..._IsNotOfferedWhenNoSequenceOfItsChoicesPaysForIt` |
| every option put to the player (no viability filter) | `..._AChoiceThatWouldLeaveItUnpayableIsNotOffered`, `NoPlayChangesNothingOverTwoHundredRandomGames` |
| `settleMode` does nothing | the three `EffectPlayLocation_...` |
| location digested as "on the board or not" | `CostFingerprint_SeesAUnitMoveBetweenLocations` |
| marker check off | `CardContract_AFinalizePassThatEndsBeforeItsMarker...` |
| unnamed-option check off | `CardContract_AnOptionWithNeitherValueNorObject...` |
| second board removed | `FinalizePassSpellDeclarationsMatchTheirPickLists` |

Not mutation-run: `ProbingAnOfferLeavesNoTrace` and the other three fingerprint tests.

## 5. Cards that trip the 9.EE2 check (for sweep-verify-cards)

From a full suite run at the final tree with `RIFTBOUND_REPORT_CARD_CONTRACTS=1`; the number is how often.

Blade Dancer (2335), Tideturner (371), Emperor's Dais (312), Grand Duelist (250), Sunken Temple (8), Volibear, Furious (3), Vayne, Hunter (3), Shadow Clone (3), Mistfall (2), Immortal Phoenix (2), Glasc Mixologist (2), Blood Rose (2), and once each: Windsinger, Wildclaw Shaman, Vex, Mocking, Valley of Idols, Treasure Hoard, Spirit Wheel, Solari Shrine, Ripper's Bay, Protective Sands, Piltover Enforcer, Nocturne, Horrifying, Nami, Headstrong, Monastery of Hirana, Kennen, Keeper of Balance, Flame Chompers, Dusk Rose Lab, Diana, Lunari, Curator of the Sands, Blitzcrank, Impassive, Blast Corps Cadet, Adaptatron.

- The check is "the pass ended without reaching `finalizingChoices`". Many of these are probably an early `return` on a declined "may" or on "nothing legal", which stores an answer and does not ask again at resolution. I did not read the 33 cards to sort harmless from harmful.
- The list is only what the suite exercises; a card with no test on the offending path is not in it.
- Flame Chompers and Immortal Phoenix (9.GG2) are on it, as you expected.

## 6. New defects found, not fixed

- 9.EE2 is not an error by default (above). Making it one is a one-line change once the list is empty.
- The viable pass asks each prompt after re-running the earlier choices from a copy of the state. A card whose finalize pass is not repeatable (same answers, different prompts) would make it throw `did not complete the choices it had just made`. No card in the suite does.
- The probe is bounded at 256 runs. A spell whose only payable sequence lies beyond that is not offered. None of the 34 comes near it on the test boards (the largest walk is in the tens).
- A probe and the viable pass copy the whole `GameState` per run. That is the 20% on the Irelia deck; a deck with many discount sources and too-dear spells would pay more.
- Sweep-triggers is converting fourteen more cards to reflexive triggers, several of them among the 34 spells. Their declarations and the walk test will need a look when that lands; I have not seen that code.
