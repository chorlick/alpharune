# batch-2 — final report: combat timing, continuous effects, throughput

## 1. Result

- Branch `fix/batch-2`, master merged twice (`c9e42e6`: batch 4a, `a92407a`; `6c294fc`: master `c943f6a`,
  status only). Gate run on the head before this report's commit (`B1: the Expiration Step's body forgets the
  skip fact`, on top of the merge).
- Clean rebuild of both build directories (`--target clean`, then `all` and `deck_fuzz`), lane B, `-j3`: **0
  warnings in our code** (8 in OpenSpiel's own sources, as on master).
- **`riftbound_tests`: 3,277 passed, 0 failed.** `riftbound_openspiel_tests`: 27 passed.
  `riftbound_clone_equiv_test`: 10/10.
- Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, `build-checked`): suite 3,277 passed.
- Smoke (fiora vs yi_rebuild, 200 games, seed 20261010, 3 threads): rc 0, **P1 122 / P2 78 / 0 draws / 17
  turns / 271 decisions** (master and B1 alone: 117 / 83 / 0 / 17 / 278; section 4).
- MCTS batch, 4 games, seed 7: rc 0 without the fault; rc 1 with `RIFTBOUND_STEP_FAULT_AT=40` ("a game failed
  ... injected engine fault at decision 40", 0 draws).
- ISMCTS, each command ten times: `sims=50`, 2 games, seed 20261010: rc 0 ten times, the same two games each
  time (8-3 in 369 decisions, 8-2 in 246). `sims=20`, 8 games, seed 1: rc 0 ten times, 3-5 each time, the same
  eight games. No "did not reach the snapshotted decision". (Both differ from batch 1's games because the
  flow changed; B1 alone reproduced batch 1's games exactly, section 5.)
- Pairing run, random vs random, 7 games for every ordered pairing of the 39 lists in `decks/` (1,521
  pairings, **10,647 games**): optimised and checked builds **0 failures, 0 games without a winner**.
- Fuzz on the final tree, fresh seeds: **no new failure group** (section 6).
- Throughput: about 520 -> about 650 games/s (+25%); about 12% short of batch 1's starting point (section 5).

What was not done as asked, said plainly:

- **Full recovery of throughput** is not there: +25% of the 29% lost. Section 5 says where the rest goes.
- **Commits per mechanism**: D5, G3, D13, D72 and B4 are one commit each, made after the fact from the one
  tested patch (cumulative hunks of one file); only the head of the series was built and tested. Smoke was
  measured after each of them (section 4).
- **The 728-line WIP test file** was kept, checked against each finding, and extended (G3, G7, G8); two of its
  tests were wrong as first written and corrected before they were trusted (G8 asserted the order of units in a
  log line; the first G3 combat test ran on the attacker's turn because the fixture's `runCombat` makes the
  attacker the turn player, so it could not fail).
- **The session restarted at about 04:45.** The previous copy of me kept running until about 04:52: it
  committed my uncommitted CLAUDE.md / plans edits (`51629b2`) and merged master's status commit (`6c294fc`).
  I checked both against my edits (identical), and everything was rebuilt from clean before the gate.

## 2. Verdicts

| item | verdict | mechanism | red shown on |
|---|---|---|---|
| B1 | FIXED, partly recovered | (1) Cleanups leave the piles out once a full Cleanup found them inert; (2) a Cleanup after a quiet one with nothing announced and the same `cleanupShape()` is not run | — (perf; same games shown) |
| D5 | FIXED | a combat is a Showdown State (CR 308.1); control lost in any Open State (CR 323.5) | B1 tree (4 tests) |
| D13 | FIXED | attacker assigns, defender assigns, then all dealt in one trigger batch (CR 467.2.c.1.a) | B1 tree (2) |
| D72 | FIXED | all holds, then one chain; one batch (CR 315.2.b.2, 383.3.d) | B1 tree (2) |
| B4 | FIXED | `runChain` after establishing control (CR 468.6) and after a showdown's end, before the next showdown or combat (CR 323.11 / 323.12) | B1 tree (2) |
| G3 | FIXED (combat) / QUESTION (non-combat) | Focus to the Attacker (CR 466.2.c.1.a), or to the player who applied Contested (CR 345); judge question 8 (J2) | B1 tree (2) |
| D60, D76, D23 (B9) | FIXED by batch 1 | `runCombat` runs the chain's loop, a Cleanup included, with designations set, before the first Focus decision | pre-batch-1 tree `59b443c` |
| D42, D26, D49, D61, G8 | FIXED by batch 1 | same root: a Cleanup after every chain step and before the combat's first decision | pre-batch-1 tree |
| G7 | FIXED by batch 1 | `awakenPhase` ends in `runChain`, whose first step is a Cleanup | pre-batch-1 tree |

"Red shown": the batch-2 tests were built against the tree before the fix and failed there. For the items
batch 1 closed, I built the pre-batch-1 master (`59b443c`, extracted to scratch, deps from this tree) with only
the fixture and those 11 tests: **9 failed exactly as the findings describe** (D60 defender 6M not 4M and the
battlefield lost; D76 4M not 6M; D23 the moved Sand Soldier 2M; D42 6 not 7 at both Focus decisions; D49 1
damage, no Bonus Damage; D61 `Inferna(1M)`; D26 taxed spell offered; G8 `Mech B(4M), Mech A(2M)`; G7 6M and
[Ganking] at both Beginning-Step priority decisions); the 2 that passed are a positive control (D26, the tax
paid) and a pin in the other direction (D76, the +2 ends with the combat).

Pins without a defect (passed before and after, kept): `D5_ControlOfABattlefieldWithoutUnitsIsLostInTheOpenStateOfACombatElsewhere`
(making combats a Showdown State must not keep control of an empty battlefield elsewhere),
`D72_AHoldThatWinsTheGameEndsTheStep`, `B4_YetiBrawler_ItsConditionIsJudgedAtTheConquer_...` (Yeti's condition
is part of its trigger condition since batch 1: B4's Yeti half was already right; the Sivir, Ambitious half was
not), `G3_InACombatOnItsTurnTheTurnPlayerAttacksAndHasFocusFirst`.

Item notes:

- **D5**: also the Flow sighting (G27): `D5_AtFocusInACombat_ASpellWithoutActionIsNotOfferedForItsFlowCost`
  (Up from the Deep from the trash, with the Main-Phase offer as its control).
- **D13** does **not** change the combat-damage option enumeration (`advanceCombatDamage` is untouched); only
  when the assignments are dealt. Batch 4b's B7 is unaffected.
- **G3 / J2**: written as judge question 8 in `docs/judge-questions-2026-10.md`. The engine follows the literal
  reading: the controller of the unit that applied Contested.
- **G7 side note** (Power paid from the pool not counted for Sivir) is not G7; listed in section 11.

## 3. B1: what was done, and the two places it was wrong first

Profile (rdtsc probes around each Cleanup step, 1,000 games, removed again): the Cleanup was 34% of a game;
the aura pass 53% of that (the card hooks 18%, `refreshFlowGrants` 9%, the clear and the fold 16%), the two
checksums 14%, the StateSettled emission and its handler 6%, the other steps the rest. The WIP's skip of a
Cleanup that follows a quiet one skips only 10% of Cleanups: almost every Cleanup follows a real change (a
priority round ends in a resolution, a phase line in a phase's work). Alone it gained nothing measurable; the
gain is mechanism (1).

Changes to the WIP before committing it:
- the skip compares `cleanupShape()` too (chain size, turn player / phase / Neutral-Showdown / Open-Closed,
  scores, object count, each battlefield's controller and flags): `processFEPR` writes the Closed State
  without announcing it;
- both shortcuts only in a game the engine plays: test fixtures edit through a reference kept from
  `mutableState()`.

What the checked build found, and is fixed:
1. **Jax, Unmatched** — "Your Equipment everywhere have [Quick-Draw]" — gives an effect to cards in decks and
   trashes, which (1) assumed never happens. Cards now declare it (`Card::continuousEffectReachesPiles`); while
   one is in play every object is walked. Found by the suite's fuzz tests on the checked build.
2. **The Expiration Step's body** ends "this turn" effects by writing objects directly. In the step's second
   pass a "this turn" -5 [M] expired, Rengar, Trophy Hunter became Mighty again and Grand Duelist triggered, in
   a Cleanup (2) would have skipped. Found by the checked pairing run; the body now forgets the skip;
   `DeckFuzz.B1_TheCleanupAfterTheExpirationBodyIsNeverSkipped` replays that game (red on the checked build
   before the change). The optimised fuzz could not have shown it: identical outcomes in all games compared.

Same games, shown for B1 before any behaviour fix: smoke 117 / 83 / 0 / 278 before and after; ISMCTS both
commands identical game by game; **9,600 fuzz games identical row by row** (`deck_fuzz --rows`, new: one line
per game) across random, random-player and pass=0; 6,000 checked fuzz games without a skip or pile assertion.

## 4. Smoke and decision flow

| state | P1 / P2 | decisions per game |
|---|---|---|
| master (`ef72c8d`, batch 1) | 117 / 83 | 278 |
| + B1 (and batch 4a merged) | 117 / 83 | 278 |
| + D5 | 123 / 77 | 272 |
| + G3 | 123 / 77 | 272 |
| + D13 | 123 / 77 | 272 |
| + D72 | 122 / 78 | 271 |
| + B4 (final) | 122 / 78 | 271 |

D5 is the movement: spells with neither [Action] nor [Reaction] are no longer offered at a combat's Focus
(both lists hold such spells; the defender on the opponent's turn could play them), so there are fewer plays
and windows (-6 decisions) and fiora's side wins 6 more of 200. G3 changes nothing here: in these lists the
attacker is the turn player. D13 changes when damage is dealt, not what is decided. D72: the second hold no
longer waits for the first hold's abilities (-1). Fuzz, mean decisions per game: 264 random, 257 random-player,
292 pass=0 (batch 1: 266 / 261 / 291).

## 5. Throughput

1,000 games, 1 thread, fiora vs yi_rebuild, three alternating runs (lane A was building beside me; load about
4):

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| master before batch 2 | 517 games/s | 516 | 526 |
| B1 alone | 654 | 638 | 644 |
| final | 654 | 643 | 655 |

**+25%.** Batch 1 measured master before it at 783 against 550 after, on a quieter machine; scaled, batch 2 is
about 12% short of the pre-batch-1 rate. Where the time still goes (B1 profile, per Cleanup that runs): the
card hooks (`applyPassiveAura` of every card in play: Wuju Bladesman walks the board twice per Cleanup, for
example), `refreshFlowGrants` (rebuilds every player's trash grants each pass), the two checksums, and the
StateSettled emission. None of these is a walk the piles change; recovering more means making individual hooks
cheaper or skipping the aura pass when its inputs are unchanged, which needs a per-hook declaration of what it
reads. Fuzz throughput: about 600 games/s per thread (batch 1: about 500).

## 6. Fuzz, final tree

`deck_fuzz --decks 5000 --games-per-pair 20 --threads 3 --known docs/testing-known-failures.txt`, seeds
20261401-3 (checked: `--decks 2500 --games-per-pair 8`, seeds 20261501-3).

| build | agents | games | failures | groups | games without a winner / long chains / hung |
|---|---|---|---|---|---|
| optimised | random | 100,000 | 1 | F6 1 (Sacrifice) | 0 / 0 / 0 |
| optimised | random-player | 100,000 | 3 | F6 2 (Sacrifice), F5 1 (Kennen) | 0 / 0 / 0 |
| optimised | random-player:pass=0 | 100,000 | 2 | F5 2 (Kennen) | 0 / 1 / 0 |
| checked | random | 20,000 | 0 | — | 0 / 0 / 0 |
| checked | random-player | 20,000 | 0 | — | 0 / 0 / 0 |
| checked | random-player:pass=0 | 20,000 | 1 | F6 1 (Heedless Resurrection) | 0 / 0 / 0 |
| optimised | pairing, 39 lists x7 | 10,647 | 0 | — | 0 / 0 / 0 |
| checked | pairing, 39 lists x7 | 10,647 | 0 | — | 0 / 0 / 0 |

Against batch 1's table: F3, F4 and F7 are gone (batch 4a), F5 and F6 remain (known, other batches). **New:
none.** The one long chain (201 items, pass=0) is Imposing Challenger and Irresistible Faefolk answering each
other: batch 1's F8 loop, stopped by the Tournament Rules 505 cap after 100 accepted "may"s; the game ended
normally.

## 7. Tests changed by design

None. No existing test was changed or weakened; all 3,249 tests of master pass unchanged. Added:
`tests/test_batch2_combat_timing.cpp` (27 tests) and `DeckFuzz.B1_TheCleanupAfterTheExpirationBodyIsNeverSkipped`.

## 8. Trainer notes

- **Feature `ns_state == Showdown`** (the turn-state block) is now 1 throughout a combat, as for a non-combat
  showdown; it was 0. No layout change, no checkpoint break, but the value's meaning for combat positions
  changed.
- **Options withdrawn**: spells with neither [Action] nor [Reaction] (hand and Flow / trash replays) at a
  combat's Focus and in its chain windows, for both players.
- **Order of decisions**: in a combat the attacker has Focus first, also on the other player's turn; in a
  non-combat showdown the player whose unit applied Contested. The defender's damage assignment is asked with
  no combat damage dealt yet (and nothing triggered by it on the chain).
- **New order prompts possible**: hold abilities of several battlefields are one batch, so a player with two
  different hold abilities is asked their order (batch 1's prompt, no new action ids).
- **Timing**: a combat's conquer abilities resolve before the next staged combat begins.
- `kStateFeatureDim`, `kCardVocabSize`, the layout and the action-id scheme are unchanged.

## 9. Items closed

B1, B4, D5, D13, D23, D26, D42, D49, D60, D61, D72, D76, G3, G7, G8; B9's batch-2 part (B9 stays open for
batch 3's part). Outcomes and test names in `plans/completed/10-testing-phase.md`. Not closed: none of mine.

## 10. Needs another stream / shared files

- Shared files touched: `src/core/events.{h,cpp}` (an activity counter in every emit), `src/core/object_table.h`
  (`offPiles()`), `src/cards/card.h` (`continuousEffectReachesPiles`), `src/cards/units/0377_jax_unmatched.cpp`
  (declares it), `src/engine/game_engine.{h,cpp}` (runCombat, runShowdownLoop, combatDamageStep,
  combatResolutionStep, scoringStep, advanceMainPhase, updateBattlefieldControl, cleanup, recalculateAuras,
  doExpirationBody), `tools/deck_fuzz.cpp` (`--rows`), `tests/test_deck_fuzz.cpp`, CLAUDE.md,
  `docs/judge-questions-2026-10.md` (question 8).
- **Any card whose continuous effect reaches cards in a deck, trash or Banishment** must override
  `Card::continuousEffectReachesPiles()`; the checked build throws, naming the object, if one does not.
- **Engine code that writes game state without an event or a log line** (as the Expiration Step's body does)
  must set `quiet_cleanup_valid_ = false`, or the next Cleanup may be skipped; the checked build catches it.
- Batch 4b (B7): the D13 rework does not touch the damage-option enumeration.

## 11. New defects found (not fixed)

- **CR 323.12, combat order**: with several combats staged, "the Turn Player chooses one of those
  Battlefields. Combat begins there." The engine takes them in battlefield order (`advanceMainPhase`), no prompt.
  The same for several staged showdowns (323.11).
- **CR 468.5-468.7, end of combat**: the rules establish control (468.5), resolve what that queued (468.6) and
  only then end the combat (468.7: designations removed, end-of-combat effects, "this combat" effects expire).
  The engine clears the designations, returns to a Neutral State and emits `CombatEndedEvent` before it
  establishes control, so the conquer abilities resolve after the combat has ended (a "when I conquer after an
  attack" ability cannot read its unit's designation; it uses the recorded `last_combat_*` values instead).
- **Sivir, Mercenary (G7's side note)**: `power_spent_this_turn` is counted only where a rune is recycled;
  Power paid from the pool (a Gold's [A], floating Power) is not counted as spent.
