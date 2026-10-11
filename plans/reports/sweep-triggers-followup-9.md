# sweep-triggers — follow-up 9 (9.FF1)

## Result

9.FF1 is closed; the stream has no open items.

- Branch `fix/sweep-triggers`, head `2edaaf4`, verified against `fix/integration` at `e17c6c0` (merged in before the final run, no conflicts).
- Clean rebuild of `riftbound_tests`, `riftbound`, `riftbound_openspiel_tests`, `riftbound_clone_equiv_test` (lane B, `-j3`):
  - full suite: 2831 passed, 0 failed
  - openspiel tests: 25 passed
  - clone equivalence: 10/10
  - 40-game smoke: rc=0, 0 draws
  - fault check: MCTS batch rc=0 without the fault, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`
- No engine code changed in this follow-up: one card comment, tests, and the plan tick.

## Items

**1. `ChainTestFixture.DisintegrateKillsAndDraws` — renamed.**
It is now `ChainTestFixture.DisintegrateKillsItsTargetAndDrawsNothingWithoutAnEngine` (`tests/test_chain.cpp`), which is what it asserts. The draw itself is covered on the engine path by `ReflexiveCardsTest.Disintegrate`.

**2. `Card::notReflexiveBecause()` marker branch — covered.**
The per-card rule of the registry test is factored into `reflexiveDeclarationProblem(card)`. `Reflexive_EveryCardWithThePhraseDeclaresItself` applies it to every registered card (still 17 declared). The new `SweepTriggersReflexiveRule.AcceptsTheMarkerAndCatchesEachMistake` applies it to test cards:
- phrase + implemented trigger: accepted
- phrase + marker: accepted
- phrase with neither: caught
- phrase with both: caught
- "for each of the following tags" with no claim: accepted
- a claim without the phrase: caught
- "Do one of the following" and "Do the following" are recognised as phrases

**3. Dancing Grenade 582 — NOT A DEFECT, pinned.**
The text is "Its controller may play this spell again for [A]", with no "do this".
- It is not a reflexive trigger (CR 388.1). The play is an instruction of the spell's own effect, a limited play made during its resolution (CR 420.3).
- A spell goes to the trash only after its whole effect has been executed (CR 359.3.d), so the card is still on the chain when it is played again.
- The existing behaviour is therefore right: it is a replay while resolving, not a play from the trash. `ChainManager` does not file the card until the new play resolves.
- The citations are in the comment at `src/cards/spells/0582_dancing_grenade.cpp`.
- `SweepTriggersTest.ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves` used Death from Below, which no longer reaches this state. It now plays Dancing Grenade through the engine: P2 answers the "may" and picks the replay's target. It asserts 2 damage then 3 (the Bonus Damage), [A] spent, the card never in a trash while its play is on the chain, and filed exactly once in its owner's trash.

## Mutations

| Mutation | Red test |
|---|---|
| `chain_manager.cpp`: replay detection off (`replayed` never set) | `SweepTriggersTest.ReplayedSpell_StaysOnTheChainUntilTheNewPlayResolves` |
| rule helper: marker no longer accepted | `SweepTriggersReflexiveRule.AcceptsTheMarkerAndCatchesEachMistake` |

Each mutation turned exactly that one test red; both were restored afterwards.

## New defects found

- The existing `C3Test.DancingGrenade_*` tests (`tests/cards/test_review_fix_c3.cpp`) all stay green with the replay detection disabled, so they do not cover where the card is filed. The rewritten test above now does; nothing further is needed.

## Files

- `src/cards/spells/0582_dancing_grenade.cpp`
- `tests/test_chain.cpp`
- `tests/test_review_fix_sweep_triggers.cpp`
- `plans/09-found-during-fix-run.md`
