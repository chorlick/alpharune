# sweep-verify-cards — eleventh tranche report (9.EE1, 9.EE2, 9.EE3)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `7153400` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`), **0 warnings**: **`riftbound_tests` 2870 passed, 0 failed;
  `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- `VerifyCardsTest`: 339 tests (40 new).
- Red check (all changed card files put back to HEAD): 42 tests failed there, i.e. every new test and two
  existing ones whose scripts now name value 0.
- One helper-written card (Tomb-Raider Barbara 824) did not compile at first — a variable name clash; fixed
  before anything was committed. The red check had already run against HEAD cards by then, and the full
  suite was re-run on the fixed tree; the numbers above are from the later clean build.

## 2. Closed

**9.EE2, card half — ticked. All 220 cards that opt into choices at finalization were audited**; the per-file
list is in `.worktrees/reports/sweep-verify-cards-11-ee2-audit.txt`. 172 clean, **39 fixed**, none left that
asks or pays at resolution:
- (A) `return` ahead of `confirmOptional` on a condition that can change between the passes — 20 cards
  (534, 277, 770, 765, 532, 949, 774, 72, 152, 465, 671, 555, 783, 6, 37, 35, 194, 641, 955, 617) plus
  Curator 932. The return is gone; the helper stores the "no" itself when its predicate is false.
- (B) `return` on an empty list ahead of `pickTarget` — 5 (228, 165, 308, 225, 729).
- (C) `return` on the source's location ahead of the helper — 6 (41, 351, 404, 674, 828, 875); the condition
  now empties the list or the mode mask.
- (D) a trigger condition tested inside the callback — 5 (573 "during a showdown", 562 "to a battlefield",
  824 "7 or more runes", 923 and 866 `[Empowered][>]`); now `triggerCondition`, so no ability goes on the chain.
- (E) a branch that skipped a helper — 2 (936 Defender of Tomorrow: Empowered in response; 372 Aphelios buff
  mode with no unit yet).
- One test per card: the condition is false at finalization, made true at the opponent's first priority, and
  nothing is asked, paid or done afterwards.
- Behaviour notes: Dusk Rose Lab 765 — a paid cost now always draws (before, losing control of the battlefield
  between payment and resolution withheld the draw). In group D the condition is ALSO still checked inside
  `onTrigger`, because three direct-call tests (`Vendetta_Barbara_KillsEnemyGearAtSevenRunes`,
  `Vendetta_Ambessa_AttackKillsWeakerEnemyHereOnlyWhileEmpowered`, `FreshBeans_NoOpOutsideShowdown`) bypass
  `triggerCondition`; so it is still re-checked at resolution there.
- The fourteen cards sweep-triggers is converting do not opt into finalization at this head: none has the shape.

**9.EE3 — ticked.** The re-checks were dead in all five (same predicate, same entry, nothing in between
changes a rune) and are removed, each with a comment saying why none is needed.

**9.EE1 — six cards fixed, two left (box stays open).** The decline / skip / stop option now carries
`chosen_value` 0, position unchanged, in Rek'Sai 491, Void Burrower 553, Whirlwind 187, Emperor's Divide 366,
Hard Bargain 457 and Corrupted Dragon 878. The two Void Burrower tests name the value instead of `kFirst`;
Rek'Sai can now be declined by a script (new test). All 25 hand-built MakeChoice sites in `src/cards` were read.

## 3. Not closed

- **9.EE1: Scryer's Bloom 698 and Abandon 693.** Their Predict "keep it" option has neither value nor object.
  needs sweep-costs: give each `valChoice(ctx.controller, 0, "...: keep it")` AND, in the same commit, change
  `EnginePathAgent::isPredictRecycleOrKeep` in `tests/cards/card_test_fixture.h`, which recognises a Predict
  prompt by exactly that valueless, label-less keep option (otherwise the default agent starts recycling
  instead of keeping). The engine's own Predict / look options in `effect_executor.cpp` (`keep_choice`,
  `skip_it`) have the same shape and will trip the planned assert too.

## 4. Needs another stream / for the trainer

No `shared:` commits; changed: 49 card files, my test file, plans/09.
- sweep-costs: the 9.EE1 line above; and for its 9.EE2 engine check — `pickEffectPlayLocation`
  (`card_helpers.h`) returns 0 without storing anything when no battlefield is controlled, so Flame Chompers 6,
  Immortal Phoenix 37 and Soulgorger 196, which call it before their marker, ask for a location at resolution
  if a battlefield is controlled by then. Fix in the helper: store mode 0.
- **Trainer:** the action id of six answers moves from the MakeChoice "empty / skip" slot to the int-answer-0
  slot (491, 553, 187, 366, 457, 878 — no vocabulary size change). And 39 cards can no longer be made to ask
  at resolution, which removes decision points that only existed through this defect.

## 5. New defects found (none fixed)

- `card_helpers.h` `pickEffectPlayLocation`: stores nothing when there is nothing to choose (see section 4).
- Elder Dragon 680 and Imposing Challenger 667 would ask at resolution only if a battlefield is ADDED while their ability is on the chain (Baron Nashor's Pit is the only source); left, noted in the audit file.
- Veiled Temple 535: its "you may detach it" is asked at finalization when the gear is attached then; CR 383.3.a.3 puts a later "may" at resolution — a different question from 9.EE2, not decided.
- The same dead re-check after the pick remains in Aphelios 372 (line ~115) and Hwei 642 (line ~96; that file is sweep-triggers').
- Group D of 9.EE2: three direct-call tests keep the trigger condition inside `onTrigger` (named in section 2); converting them to the engine path would let the in-callback copy go.
- Card 919 builds an option with `chosen_card_def` only: not this defect (it is named by card def and encoded by it), but `EnginePathAgent::script` cannot match it.
