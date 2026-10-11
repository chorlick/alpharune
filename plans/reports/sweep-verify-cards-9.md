# sweep-verify-cards — ninth tranche report (9.Z2, 9.Z3)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `c875e27` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`): **`riftbound_tests` 2785 passed, 0 failed;
  `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- Warnings: 0 in card files and 0 in my test file. **17 `-Wunused-variable` / `-Wunused-parameter` warnings
  arrived with the last integration merge**, all in other test files (see section 5) — my previous clean
  build, one merge earlier, had none.
- Red check: 19 tests failed against the 6 unfixed card files. `VerifyCardsTest`: 289 tests.
- Process note: I started the final build in the same step as a merge that turned out to conflict (one plan
  line: you had ticked 9.Z1 while I ticked 9.Z2 / 9.Z3). I stopped that build before using its result,
  resolved the conflict (both ticks kept) and re-ran the clean build on the resolved tree; the numbers above
  are from that run.

## 2. Closed

**9.Z2 — FIXED, ticked.** Rune picks are made at finalization (CR 355.5.b, 383.3):
- Dark Child 315, Targon's Peak 284 (its delayed end-of-turn ability; the conquer branch got its own marker so
  the finalize pass does not schedule the delayed ability twice), Gentle Gemdragon 666, Sona 73 and Curator of
  the Sands 932 opt into `kFinalizeTrigger`: runes named before the marker, readied after it; a named rune
  that is no longer an exhausted rune on the board at resolution is skipped.
- Found on the way: **Curator of the Sands had no finalization at all** — its "may" and its exhaust cost were
  decided and paid at resolution. Both are at finalization now, cost immediately before the marker.
- Sona's "if I'm at a battlefield" is now checked as the ability goes on the chain, not again at resolution
  (the Sona example under CR 383: removed in response, the ability still resolves). This goes slightly beyond
  the item; it is one guard to revert.
- Hwei 642 — NOT A DEFECT, pinned and commented: "do the following based on the discarded card's type" is a
  reflexive trigger (CR 388.1.b, 389.1; the Vendetta FAQ on Hwei), so its choice comes after the discard, not
  at the move ability's finalization.
- `AuditFix3Test.Hwei_DiscardGearReadiesRunes` now answers its three prompts by name and fails on an
  unexpected one.

**9.Z3 — FIXED, ticked; a rules reading with play impact, so please confirm it with the user.**
- Printed wording (def text and gallery data agree): 315, 284, 666, 932, 642 say bare "runes"; Sona 73 says
  "ready up to 4 **friendly** runes".
- Rules: a type word means any such object on the board (CR 133.3.a, 355.9.a.1); a choice is narrowed only by a
  printed restriction (CR 355.9.b); readying by an effect has no controller limit (CR 416.3.b); and cards that
  mean "yours" print it (Sona "friendly runes", Ekko 110 "your runes", Sigil of the Storm "one of your runes").
  No rule limits an effect on "runes" to your own.
- So the five bare-"runes" cards now offer every exhausted rune on the board; Sona is unchanged and pinned.
- Revert, if the user reads it otherwise: one predicate per card (`obj.controller == ctx.controller` in
  `readiable`).

Existing tests changed by 9.Z3: three of my tranche 8 tests asserted the opponent's rune is NOT offered
(flipped; the Dark Child pin is renamed `…ReadiesUpToTwoRunesItsControllerPicks`), and
`AuditFix6Test.TargonsPeak_EndOfTurnIgnoresEnemyRunes` (a direct call encoding the old reading) is rewritten
on the engine path as `TargonsPeak_EndOfTurnReadiesOnlyTheRunesNamed`.

## 3. Not closed

Nothing of mine. (The two tests I had left `DISABLED_` — Mystic Vortex, Volibear — were enabled by
sweep-verify-engine and pass.)

## 4. Needs another stream / for the trainer

No `shared:` commits; changed: six card files (315, 284, 666, 932, 73, 642), my test file,
`test_audit_fixes_3.cpp` and `test_audit_fixes_6.cpp` (one test each), plans/09.

**For the trainer — this one matters:** with 9.Z3 a first-option or random agent can ready an OPPONENT'S rune
with Dark Child (every end of turn), Targon's Peak, Gentle Gemdragon, Curator and Hwei. Options are listed in
object-id order, so whether the first option is the agent's own rune depends on creation order. "None" is
still last. Decks with Dark Child will see their random-rollout baseline shift.

## 5. New defects found (none fixed)

- 17 compiler warnings in test files that came in with the last integration merge: unused variable `c` / `card` in `tests/cards/test_audit_fixes_0.cpp:335`, `_4.cpp:253,275,289`, `_5.cpp:224,240,272`, `_6.cpp:346,363,377`, `_7.cpp:490` (+2 more), `test_rules_compliance_cards2.cpp` (2), `test_vendetta_E.cpp` (1), and an unused parameter `exec` in `test_audit_fixes_2.cpp:89` — left by another stream's test conversions.
- `src/cards/card_helpers.h` `selectSeveral`: called again at resolution with options rebuilt from the current state, `options.erase(std::find(...))` is `erase(end())` when a stored pick is no longer in the list (undefined behaviour). The five cards avoid it by reading their picks back through `selectObject` with an empty offer; a finalize-safe `selectSeveral` would remove five copies of that loop and the hazard.
- Aphelios, Exalted 372 ("Ready 2 runes"): picks after its marker (at resolution) and offers only its controller's runes — the same two questions as 9.Z2 / 9.Z3.
- Acceleration Gate 937 ("Ready up to 4 units, gear, and/or runes"): not re-read for the 9.Z3 question (whose runes).
- Reflexive triggers ("do this: ..." / "then, based on ...") have no chain item in the engine: Hwei 642, Twisted Fate 200 and Deceiver 756 resolve them inline, so the opponent gets no priority between the naming and the effect.
- Sona 73: the battlefield condition is no longer re-checked at resolution (deliberate, see section 2) — flagged so it is a recorded decision rather than a silent one.
