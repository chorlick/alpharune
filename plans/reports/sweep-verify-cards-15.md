# sweep-verify-cards — fifteenth report (9.HH6)

Branch `fix/sweep-verify-cards`, head `fade61c`; verified against integration `041a416` (merged first, fast-forward). Working tree clean.

## 9.HH6 — NOT A DEFECT, pinned (tests only; `card_helpers.h` and all card files untouched)

A second test card (`SelectSeveralDupProbeCard`, id 1061, registered by the test) lists each other friendly unit once per buff, as Albus Ferros 230 does.

- X has two buffs, Y one: the list is {X, X, Y}. All three are picked at finalization. In response X loses one buff, so the list at resolution is {X, Y}.
- `selectSeveral` returns **{X, Y}** and asks nothing again. Each stored pick uses up one instance of its object in the current list: the first stored X counts, the second finds no instance left and is dropped, Y counts.
- That is right: returning X twice would let a caller spend a buff that is no longer there; returning {Y} alone would discard a pick that is still good.

Tests in `tests/cards/test_review_fix_verify_cards.cpp`:
- `VerifyCardsTest.SelectSeveral_AnObjectListedTwiceWithOneInstanceGoneCountsOnce`
- `VerifyCardsTest.SelectSeveral_AnObjectListedTwiceIsReturnedTwiceWhenBothInstancesRemain` (control: {X, X, Y} when nothing changes)

Mutant (`selectSeveral` keeps a stale pick): eight tests red — the new first test, the 9.HH5 helper test, the five rune pins and Hwei's; 2886 others green, the control among them.

## Gate (clean build, lane B, head `fade61c`)

- `riftbound_tests`: 2894 passed, 0 failed; 0 warnings
- `riftbound_openspiel_tests`: 25 passed
- clone-equivalence: 10/10
- 200-game smoke: rc=0, P1 139 / P2 61 / 0 draws (unchanged)
- MCTS batch: rc=0; with `RIFTBOUND_STEP_FAULT_AT=40`: rc=1, "injected engine fault at decision 40"

No new findings.
