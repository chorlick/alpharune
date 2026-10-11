# sweep-triggers — follow-up 10 (9.GG4)

## Result

9.GG4 is closed; the stream has no open items.

- Branch `fix/sweep-triggers`, head `ea7c5e9`, verified against `fix/integration` at `48e39ca` (merged in before the final run; the only conflicts were in `plans/09-found-during-fix-run.md`, resolved keeping both streams' ticks).
- Clean rebuild of `riftbound_tests`, `riftbound`, `riftbound_openspiel_tests`, `riftbound_clone_equiv_test` (lane B, `-j3`):
  - full suite: 2882 passed, 0 failed
  - openspiel tests: 25 passed
  - clone equivalence: 10/10
  - 40-game smoke: rc=0
  - fault check: MCTS batch rc=0 without the fault, rc=1 with `RIFTBOUND_STEP_FAULT_AT=40`

## Item

**Hwei 642 — dead re-check removed.**
`src/cards/units/0642_hwei_brooding_painter.cpp` no longer calls `readiable(id)` on each rune after `selectSeveral`. The comment says why none is needed: the list handed to the helper is built with the same predicate in the same entry, the helper returns only ids that are in it, and readying one rune does not change another. This matches what 9.EE3 did for 315, 284, 666, 932 and 73.

New pin: `SweepTriggersTest.Reflexive_HweiARuneReadiedOrGoneInResponseIsSkipped`. Two runes are named at finalization; in response one is readied and the other leaves the board. It asserts that nothing is asked again, the rune that left is not readied, the third rune does not take a place, and the card's own trace line says "readied 0 rune(s)". The existing Hwei tests are unchanged and green.

## Mutation

| Mutation | Red test |
|---|---|
| `card_helpers.h`: `selectSeveral` keeps a stored pick that is no longer in the list | `SweepTriggersTest.Reflexive_HweiARuneReadiedOrGoneInResponseIsSkipped` (the other 2871 pass) |

## New defects found

- The sibling pins from 9.EE3 (`DarkChild315_`, `TargonsPeak_`, `GentleGemdragon_` … `_ARuneReadiedOrGoneInResponseIsSkipped`, `tests/cards/test_review_fix_verify_cards.cpp`) stay green when `selectSeveral` stops dropping stale picks. `EffectExecutor::readyObject` refuses an off-board rune by itself, so the board state they assert does not depend on the helper. The behaviour is correct, but those tests do not pin the property the 9.EE3 comments in the five cards rely on. The Hwei pin first had the same gap and now asserts the card's own readied count; the same assertion would close it for the five. Owner: sweep-verify-cards.

## Files

- `src/cards/units/0642_hwei_brooding_painter.cpp`
- `tests/test_review_fix_sweep_triggers.cpp`
- `plans/09-found-during-fix-run.md`
