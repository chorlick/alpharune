# sweep-verify-cards — fifth tranche report

## 1. Result

- Branch `fix/sweep-verify-cards`, head `ed675ef` (merged with the current `fix/integration`, including
  sweep-verify-ledger and the first half of sweep-verify-engine; working tree clean).
- Clean build (PCH-safe launcher, lane B, `-j3`) of the three test targets, **0 warnings**:
  **`riftbound_tests` 2638 passed, 0 failed (1 disabled, see 9.O1); `riftbound_openspiel_tests` 24 passed;
  clone-equivalence 10/10.** Not run: smoke, MCTS batch, fault injection.
- `VerifyCardsTest`: 202 tests. Red checks: 27 tests failed against the 37 unfixed card files of this tranche,
  4 more against the unfixed Mystic Vortex. Pins pass on both, as they should.
- The two `-Wdangling-else` warnings in my test file are gone (fixed in `3bd0e86`; the build has none).
- Merge of sweep-verify-engine conflicted in four card files: 153, 253, 931 taken from integration (their
  version supersedes mine); The Zero Drive 412 merged by hand — their "banish this is paid by the engine at
  activation" plus my per-unit location prompt, now using `battlefieldTakesUnitsFrom`.

## 2. Closed

- **07 item 7.33 Party Favors — ticked, not carried to stage 5.** The two answers are `valChoice` 0 / 1 and
  `encodeAction`'s MakeChoice branch (`src/openspiel/action_vocab.cpp`) maps a set `chosen_value` v to
  `verbBase(MakeChoice) + kNumCardDefIds + 1 + v`: slots +1101 and +1102, distinct; `ability_index`,
  `choice_label` and `player` are not part of that encoding. (No test asserts `encodeAction` directly for this
  card; `ReviewFixC2Test.PartyFavors_TheTwoAnswersAreDistinctValues` pins the values.)
- **9.M28** Safety Inspector 726: the 3 XP is a real play cost (`additionalPlayCost` / `payAdditionalPlayCost`).
- **9.M31** caps of Fox-Fire 253 / Overt Operation 153: done by sweep-verify-engine, verified, ticked.
- **9.M32** three tests renamed to what they assert; plan lines citing them updated (not `STATUS.md`).
- **9.M33** Bellows Breath 402 — FIXED, not blocked: CR 355.11.b / 355.11.b.1 use this very card as the example;
  the location is not fixed when the targets are chosen. The still-legal targets are grouped by current
  location; with several groups the controller picks one. (The plan item's parenthetical said the opposite.)
- **9.M34** Angle Shot 334 attaches through `ctx.executor.attachGear`.
- **9.M35** all nine cards (25, 62, 115, 198, 242, 412, 471, 553, 710) ask where the effect-played unit goes.
- **9.M36** test comments; **9.M37** Voidreaver 787 labels; **9.M38** Defender of Tomorrow 936 asks which
  printed ability while Empowered (no vocabulary change, Empower index unmoved).
- **9.N4** Corrupted Dragon 878, Blitzcrank 67, Faefolk 674 no longer offer a unit they cannot move.
- **9.N6** ten direct-call / cannot-fail tests rewritten on the engine path with accepting agents (names kept);
  the dead probe is gone from the C7 Stalking Wolf test.
- Undying Loyalty 730's own play-location mask removed (the shared helper filters now); its three tests still pass.

Existing tests changed because a fix changed a prompt: `ReviewFixC2Test.DefenderOfTomorrow_TheGearToReadyIsChosen`
(mode prompt first while Empowered), `EquippedTriggerChainTest.TheControllerChoosesWhichUnit` (Last Rites asks where).

**Prompt changes for the trainer:** 726 (yes/no during the play, not at the trigger), 936 (mode prompt while
Empowered), 402 (a group prompt when its targets ended in different places), a location prompt when a
battlefield is controlled in 25, 62, 115 (each player), 198, 242, 412 (per unit), 471, 553, 710; a Void
Hatchling look prompt before the reveals of 529, 820, 641, 121, 505, 553 and (for the opponent) 25.

## 3. Not closed, with exactly why

- **9.M29** — every card comment is corrected (931 by sweep-verify-engine; 730, 401, 772, 460, 554, 540, 544
  by this stream). Left: the comment on `grant_repeat_base_to_next_spell` in `src/core/game_state.h` (not a
  card file). needs sweep-verify-engine: "the next spell without a printed [Repeat] gets a Repeat cost equal to
  its base cost (Energy and Power)".
- **9.M30** — fixed for the six by-hand reveals and Blind Fury 25 (the opponent reveals, so the opponent's
  Hatchling looks); "look at N, you may reveal one" (381, 594, 613, 626, 741) closed NOT A DEFECT with pins and
  comments — a judgement, not a clean ruling (under the literal reading the recycled card's position at the
  bottom would be known rather than random, CR 417.5). **BLOCKED: rules question — Twisted Fate, Gambler 200**
  reveals the top card of the Rune deck. Reading A: "reveal cards from a deck" (CR 425.3) covers the Rune Deck
  (CR 103, 108.5, 417.5.a) — the controller then looks at the top rune and may recycle it, steering the payoff,
  and `lookBeforeReveal` needs a deck argument (engine). Reading B: "deck" in card text means the Main Deck
  (CR 392.3.b example; every other reveal card says "Main Deck") — nothing changes. No change made.
- **9.O1 Mystic Vortex 947** — card side fixed; one part needs the engine reader. Decisions: a combat counts
  only during its Combat Showdown Step (CR 316.8.a, 466; Damage and Resolution Steps are combat, not
  showdown); "here" says when, not where the taxed card is played, so charging a play anywhere is right. The
  card used to publish its tax only while a flag was already up, so **the tax was missing at the opening
  decisions of every showdown** (no aura pass runs between the flag being set and the first decision); it now
  publishes a standing amount and the reader gates it. needs sweep-verify-engine: `GameEngine::extraAnyPowerCost`
  must count `bf.reaction_tax_power` only while `bf.showdown_in_progress || (bf.combat_in_progress &&
  bf.combat_phase != CombatPhase::DamageStep && bf.combat_phase != CombatPhase::ResolutionStep)`; then enable
  `VerifyCardsTest.DISABLED_MysticVortex_NoTaxOnceTheCombatShowdownHasClosed` and tick.

## 4. Needs another stream

No `shared:` commits. Files touched: 37 card files (10 + 19 for the items, 8 comment-only), my test file, eight
existing test files, plan files. No keyword line in any def was changed.
- sweep-verify-engine: the two `needs` lines above (9.M29 comment, 9.O1 reader); 9.M30 reading A if the user
  chooses it.
- User: the Twisted Fate 200 rules question; the look-then-reveal judgement if they want the literal reading.

## 5. New defects found (none fixed)

- `spells/0253_fox_fire.cpp`: does not re-choose a legal subset when the chosen units' total Might no longer fits at resolution (the Fox-Fire example in CR 355.11.b.1).
- Paid-flag residue: `units/0726_safety_inspector.cpp` and `units/0481_zaun_punk.cpp` keep a "cost paid" flag on the object for their play trigger; if that trigger never resolves (countered) the flag survives, and a later replay that cannot pay would be treated as paid. Needs the engine to clear such flags on a zone change.
- `spells/0505_void_rush.cpp`: checked after the merge — its location list now goes through the filtered `playLocations`; no defect (listed so nobody re-raises it).
- `src/core/game_state.h:200`: stale comment (see 9.M29).
- One ENGINE-GAP marker is left in card code and must match an open box or be reworded: `src/cards/card_helpers.h` (split damage: CR 355.14.a-b make every unit in a split a target chosen at play; the helper chooses at resolution). Corrupted Dragon 878's marker is gone after the sweep-verify-engine merge.
- No test asserts `encodeAction` for a card's MakeChoice values (7.33 rests on reading the encoder); a slot assertion belongs in `src/openspiel/openspiel_tests.cpp`.
- `tests/cards/test_review_fix_verify_cards.cpp`: several location tests execute a play for the non-turn player through `playPayingCost` (Promising Future) — the only coverage of that path.
- The new location and look prompts change seeded game trajectories; no test asserting a seeded outcome failed, but smoke win splits will move.
