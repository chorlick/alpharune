# sweep-verify-cards — eighth tranche report (9.W1)

## 1. Result

- Branch `fix/sweep-verify-cards`, head `991453f` (merged with the current `fix/integration`; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`), 0 warnings: **`riftbound_tests` 2735 passed, 0 failed
  (2 disabled: Mystic Vortex waiting on 9.R4, Volibear waiting on the shared helper, below);
  `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- Red check: 15 tests failed against the 7 unfixed card files; pins pass on both.
- `VerifyCardsTest`: 268 tests. Every new prompt is answered by `script({...})` or a hand-written picker.

## 2. Closed (9.W1 — all 23 non-spell files read against CR 355.13)

- **Fixed (7 cards):**
  - "Ready up to N runes" chooses rune OBJECTS, so the controller picks which and may pick fewer or none:
    Sona 73, Hwei 642, Gentle Gemdragon 666 (all three used to ready the first runes in map order, unasked),
    Dark Child 315 (readied the two lowest ids unasked), Targon's Peak 284 (asked only with more than two, then
    forced two), Curator of the Sands 932 (forced a second rune; readied a lone rune unasked).
    Why it matters: a rune can be recycled for Power ready or exhausted (CR 164.2.b), so the rune you will
    recycle is better left exhausted and another readied.
    **This overturns my own batch-01 closure "Dark Child asks nothing — NOT A DEFECT"**: the plan line is marked
    superseded and the pin replaced.
  - Kinkou Monk 141 ("buff up to two other friendly units") forced one and, when one existed, a second; now
    none, one or two.
- **Not a defect, pinned (8):** Albus Ferros 230, Janna 376, Fae Dragon 423, Azir Sovereign 497, Bard 540,
  Elder Dragon 680, Corrupted Dragon 878, Forge of the Future 212.
- **Rule does not apply (7):** Lee Sin 78 and Kayle 921 (a cap on a status), Spiderling 884 (deck
  construction), Scryer's Bloom 698, Baited Hook 242 (a Might bound), and the additional play COSTS of Kraken
  Hunter 150 and Commander Ledros 231 (zero is choosable while the card is affordable without it — pinned).
- **Emperor's Divide 366 from facedown** with no friendly unit at that battlefield: NOT offered, pinned
  (`Pin_EmperorsDivide_FromFacedownNeedsAFriendlyUnitAtItsBattlefield`). Reading: CR 811.1.d ("with no valid
  targets under these restrictions" cannot be played from facedown) overrides the general 355.13 permission.

## 3. Not closed

- **9.W1 stays open for one card: Volibear, Furious 41** ("split among any number of enemy units here"). The
  first prompt of the shared `splitDamageAmong` forces a target. needs sweep-verify-engine: in
  `src/cards/card_helpers.h` `splitDamageAmong` add a trailing `bool may_choose_none = false`; in the "Who"
  block, when `targetCount() == 0 && may_choose_none`, append `valChoice(ctx.controller, 0, "No more targets")`
  AFTER the unit options; pass `true` from `units/0041_volibear_furious.cpp`; enable
  `VerifyCardsTest.DISABLED_VolibearFurious_FirstPromptOffersDamagingNoUnit`; tick. (A card-only fix would add
  a yes/no prompt and break `VerifyEngineTest.Volibear_ASplitTargetThatLeftInResponseIsDropped`.)

## 4. Needs another stream / decisions

No `shared:` commits; changed: seven card files (73, 141, 642, 666, 315, 284, 932), my test file, plans.
- sweep-verify-engine: the Volibear line above.
- User (judgements, each a one-line revert):
  - Emperor's Divide from facedown (CR 811.1.d over 355.13). Under the other reading the pin's first two
    expectations flip and the hidden-play generator needs an empty-target play (sweep-costs).
  - Curator of the Sands 932: its "you may exhaust me" is now offered with no exhausted rune (exhaust the
    legend to ready none) — same kind of judgement as Disposal Order (9.W2).

**For the trainer:** new prompts in Sona, Hwei (Gear branch), Gentle Gemdragon, Dark Child (every end of turn
with an exhausted rune), Targon's Peak (also with one or two exhausted runes) and Kinkou Monk; "None" is the
LAST option in these, so a first-option agent still readies / buffs as before.

## 5. New defects found (none fixed)

- Timing: CR 355.5.b (its Targon's Peak example) puts a triggered ability's choices at finalization; Dark Child 315, Targon's Peak 284, Gentle Gemdragon 666, Curator 932 (rune picks), Sona 73 and Hwei 642 still ask at resolution.
- "Ready up to N runes" has no "friendly" in the text of 73, 642, 666, 315, 284, 932, yet all six offer only their controller's runes; decide whether an opponent's rune may be readied.
- `src/cards/card_helpers.h` `splitDamageAmong`: no "choose none" on the first prompt (Volibear 41, above).
- `AuditFix3Test.Hwei_DiscardGearReadiesRunes` (direct call) now passes only because its picker falls back to the first option on the two new rune prompts; it should script them.
- Golden / seeded tests: none failed, but Dark Child now adds a decision at every end of turn of a deck that plays it.
