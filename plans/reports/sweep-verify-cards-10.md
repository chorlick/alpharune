# sweep-verify-cards — tenth tranche report (9.BB1, 9.BB2, 9.CC4)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `de58c49` (merged with the current `fix/integration`, strict
  `EnginePathAgent::script` and `mayChooseAtFinalization` included; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`), **0 warnings**: **`riftbound_tests` 2805 passed, 0 failed;
  `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- `VerifyCardsTest`: 299 tests.
- Red check (the shared helper and the card files put back to HEAD): 5 tests failed — the two helper tests
  (Fae Dragon, Albus Ferros) and three Aphelios tests. Note: the two helper tests hit undefined behaviour on
  the old code (`erase(end())`); they failed cleanly here, but that is not guaranteed on another build.
  Three further new tests (Dark Child ×2, Sona) pass on old and new alike: the old read-back loops gave the
  same result, they cover the new helper path rather than a behaviour change.

## 2. Closed

**9.BB1 `selectSeveral` — FIXED, ticked** (`src/cards/card_helpers.h`, that function only).
- A stored pick is read from its slot and never re-derived. A stored pick that is no longer in the caller's
  current list is DROPPED from the result (no `kInvalidId`, nothing asked in its place) — chosen over
  `kInvalidId` so the callers I may not edit need no change (Albus Ferros dereferences every pick). The end of
  the picks is stored too, so a longer list at resolution asks nothing new. Prompt order and option values
  while choosing are unchanged.
- The five read-back loops are gone from 315, 284, 666, 932, 73.
- Callers read: Hwei 642, Azir 497, Bard 540 unchanged; Albus Ferros 230 and Fae Dragon 423 unchanged
  normally, and a candidate that goes away between two prompts is dropped instead of undefined behaviour;
  Aphelios 372 no longer calls it.
- **A regression caught by my own pin on the way:** with the read-back loops gone, Sona 73 asked for runes at
  resolution when she was not at a battlefield (the finalize pass had returned early, and the old loop had been
  masking it). Her "if I'm at a battlefield" is now a `triggerCondition`, which is where an intervening "if"
  belongs: no ability goes on the chain otherwise, and it is not asked again at resolution.

**9.BB2 — ticked.**
- Aphelios, Exalted 372 — FIXED: the runes of "Ready 2 runes" are named at finalization and readied at
  resolution; any player's exhausted rune may be named; not "up to" — with more than two exhausted exactly two
  are named and there is no "None", with two or fewer all are readied unasked. Mode-claim logic untouched; the
  four existing Aphelios tests pass unchanged.
- Acceleration Gate 937 — NOT A DEFECT: it already offers every exhausted unit, gear and rune on the board
  whoever controls it; pinned, comment extended.

**9.CC4 — my part done** (noted under the item; the 8 in `test_sweep_costs.cpp` are sweep-costs').
All 17 `kRest` uses in `test_audit_fixes_6.cpp`, `test_review_fix_c0 / c1 / c4 / c7.cpp` and my test file are
replaced by explicit entries, each trailing prompt named in a comment. `kFirst` stays in two Void Burrower
tests only (its "skip" option carries neither value nor object, so a script cannot name it). All pass under
the strict agent; **no test changed its result**. What writing them out showed:
- The fallback had been answering prompts the tests never mentioned: Arise! readied its new token; the second
  Altar of Memories round put a different card back. No assertion depended on either.
- Rek'Sai 491's decline option has no value and no object and is listed last, so a script cannot say "banish
  none"; that test still banishes the top card.
- The Arcane Shift base-only test's old script `{1, 1, 1}` had never matched any option.
- Four tests are stricter now: an unexpected extra prompt fails the script as well as the assertion.

## 3. Not closed

Nothing of mine.

## 4. Needs another stream / for the trainer

One change outside card files, as authorised: `selectSeveral` in `card_helpers.h`. Also changed: seven card
files (315, 284, 666, 932, 73, 372, and a comment in 937), six test files, plans/09.

**For the trainer:** Aphelios's rune mode is mandatory once chosen, and with 9.Z3's reading it can ready an
opponent's runes — a first-option or random agent that takes that mode when only the opponent has exhausted
runes readies theirs. This belongs with the 9.BB4 confirmation.

## 5. New defects found (none fixed)

- `units/0491_rek_sai_swarm_queen.cpp` and `legends/0553_void_burrower.cpp`: the "banish none" / "skip" option of their hand-built prompt carries neither a `chosen_value` nor an object, so it is distinguishable only by position (last in Rek'Sai, first in Void Burrower); give it a value so agents, scripts and the action vocabulary can name it.
- `EffectExecutor` / card callbacks in general: a card that returns before its `finalizingChoices` marker in the finalize pass is entered again at resolution with `choices_finalized` set and nothing stored — any helper it then calls asks at resolution. Sona showed it; other cards with an early return before their first helper (grep for `return;` ahead of `finalizingChoices`) deserve the same look, and the engine could refuse to resolve an item whose finalize pass ended before the marker.
- The redundant `readiable(id)` re-checks after `selectSeveral` in 315, 284, 666, 932, 73 can go now that the helper drops stale picks (harmless; left in).
