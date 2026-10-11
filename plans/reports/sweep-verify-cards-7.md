# sweep-verify-cards — seventh tranche report (9.T3)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `6a1318f` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`), 0 warnings: **`riftbound_tests` 2679 passed, 0 failed;
  `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- Red check: the 7 fix tests failed against the 16 unfixed card files; the pins pass on both.

## 2. Closed

**9.T3 — FIXED, ticked.** The rule is explicit (CR 355.13): "any number" / "up to" objects includes zero,
and the spell can then be played without targets. All twenty spells with that wording were read.

- **Fixed (6):**
  - Fox-Fire 253 — no play gate any more; "Stop here" is the first option on the first prompt.
  - Flash 309 — "No more units" on the first prompt.
  - Singularity 105, Bellows Breath 402 — the empty target set is offered beside the others.
  - Emperor's Divide 366 — playable from hand with no friendly unit at a battlefield.
  - Disposal Order 665 — the "recycle up to 3" mode is selectable with an empty opposing trash. This one is a
    judgement: the spell was already playable; the change only makes a do-nothing mode selectable (355.13
    read as reaching mode legality). It is a one-line revert if you read it the other way.
- **NOT A DEFECT, each with a pin test and a comment at the code site (11):** Salvage 224, Piercing Light 346
  (its first unit is mandatory; only the other is "up to one"), Arise! 513, Decree of Discord 894, Guerilla
  Warfare 260, Moonfall 755, Tricksy Tentacles 616, Shadows of the Past 890, Relentless Pursuit 503, Shuriken
  Flip 927, Acceleration Gate 937.
- **Rule does not apply (3):** Reinforce 62 (a single optional pick), Bullet Time 263 ("any amount" is a cost),
  Curtain Call 743 (each mode names exactly a unit).
- 24 tests added (`VerifyCardsTest`, now 229), every prompt answered by an explicit `script({...})` or a
  hand-written picker; none uses `choosing(id)`.
- One existing test adjusted: `C3Test.BellowsBreath_TargetSetsAreUnitsAtOneLocation` skips the new
  empty-target play instead of asserting there is none.

No engine change was needed for plays from hand: the default `hasLegalTargets` and both generators already
accept an optional requirement with nothing to choose; what was missing was on the cards.

**For the trainer:** in Fox-Fire, Flash and Disposal Order the first option of a prompt is now "choose none",
so an agent that takes the first option does nothing with those spells; Singularity and Bellows Breath have
one more legal play (no target).

## 3. Not closed

Nothing of mine.

## 4. Needs another stream

No `shared:` commits; only the sixteen spell files, my test file, one line of `test_review_fix_c3.cpp` and
plans/09 changed.
- needs sweep-verify-engine (already 9.T2, restated with the exact site): `GameEngine::generateHiddenPlayActions`
  emits the no-target hidden play for any spell whose `getTargetRequirements().count == 0` without asking the
  card; for a spell that picks its targets at finalization (Fox-Fire, Moonfall, Rocket Barrage) it must ask
  the card whether a target exists at that battlefield before offering the play (CR 811.1.d).

## 5. New defects found

- None in card code beyond the above.
- Not scoped by 9.T3 and not read: the 23 non-spell card files (units, gear, legends, battlefields) whose text has "any number of" / "up to N" — the same CR 355.13 question applies to their triggered and activated abilities (can the controller choose none; is the ability offered with nothing to choose).
- Emperor's Divide 366 from facedown still needs a friendly unit at that battlefield (it has no target set there); consistent with CR 811.1.d read literally, untested because the hidden generator only serves Closed States and showdowns.
