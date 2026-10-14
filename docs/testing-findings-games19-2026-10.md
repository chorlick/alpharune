# Testing phase 2026-10 — 19 paired games read against card text: findings

Stream `test-games19`, branch `test/round2`, engine `v0.1.0+2d59c14` (master BEFORE the testing-phase fix
batches: the 93 known defects of `plans/remaining/10-testing-phase.md` are expected to appear).
Nothing in the engine, the cards or the tests was changed: this document records, it does not fix.

The request: "create 19 unique games (each game uses two decks from the pool then removes those decks from the
pool and repeats until none are left in the pool) and do a card text look up. It's really the best way to check
for illegal rules."

## 1. Summary

- **Games.** The 38 lists of `decks/` were paired without replacement (seed 20261011, section 2): 19 games,
  every list used exactly once, `ismcts:sims=50` in both seats. All 19 ended with a winner at 8 points, exit
  code 0, no engine error; no replacement seed was needed.
- **Reading.** Every game was walked from the first decision to the last by its own reader (19 read-only
  readers, one game each, all at once), with the printed text of every card of both lists, the option list and
  board of every decision, and the rules. Coverage, as each reader states it, is complete: **all decisions of
  all 19 games were read** (5,763 decisions by the engine's counter; per game in section 3). The lead ran the
  games, built the reading files, looked up every quoted trace line mechanically (469 quoted lines in the
  readers' notes: 467 found as written or as the beginning of a line, 2 were not quotes — a line written with
  "..." in its middle and an empty placeholder — and are not used below), read every rule cited for a new finding
  in `rules/core-rules.md`, compared all 516 `PAY_COST` lines with the printed costs (22 differ; each is a
  printed discount, a Fizz "ignoring its Energy cost" play, or the known D18) and checked scoring, the
  winning-point rule and the Channel Phase of every turn by script (clean).
- **New findings: 26 (G1-G26).** 18 CERTAIN, 5 LIKELY, 3 QUESTION. Five of the 26 are a KNOWN root on a card
  (or cards) the known item does not name — G8, G9, G19, G20, G21 — and are numbered because each needs its
  own card fix. Ordered by root cause in section 4, each with its fix batch.
- **Known items seen again: 42 of the 93**, 881 sightings counted (section 5); 19 notes on aspects the
  known item's text does not cover.
- **Games with no new finding: 0 of 19.** G1 (the first player's first draw) is in every game; leaving G1
  aside, every game still has at least one new finding or a new card on a known root.
- **What is most wrong among the new findings**
  1. **G1: in 1v1 the player going first is made to skip the first draw** (all 19 games). CR 487.7 / 488.7 give
     1v1 only the second player's extra rune; the no-draw sentence is printed for the three multiplayer modes.
     The engine cites a rule number of an older edition, and a test pins the skip.
  2. **G2: Time Warp played on the opponent's turn does nothing**: the Additional Turn replaces its owner's
     regular turn instead of being inserted before it.
  3. **G3: the turn player always gets Focus first** in a showdown, also when the other player's unit applied
     Contested and is the attacker (4 games).
  4. **G8: Mechanized Menace's [Shield] is missing in a combat nobody acts in** (root of D42 / D60 / D76); it
     changed who conquered and, from there, the result of game 9.
  5. **G9: eight more cards put an ability on the chain while the condition printed with the trigger is
     false** (root of D51): Curator of the Sands (14 empty items in one game), Lady of Luminosity, Gutter
     Palace, Patched Porobot, Poppy, Paragon, Pyke, Dockside Butcher, Blitzcrank, Impassive, Vanguard Captain.
  6. **G14: a chosen target that left the board and came back is still acted on** (Amateur Recital moved a unit
     Temporal Breach had banished and replayed), against CR 359.3.e.4 in terms.
  7. **G11 / G12: Heart of the Tempest** does not trigger for a play from the Champion Zone or a play by an
     effect, and triggers for a Flow spell as it is put on the chain instead of when it has resolved.
  8. **G22 / G21: facedown units** are played onto Rockfall Path, and Kennen, Keeper of Balance played from
     facedown stuns a unit in the enemy base.
  9. **G18 / G19 / G20: Defender of Tomorrow** cannot choose a ready gear and can be activated with no gear at
     all; **Public Execution** is offered and paid for with no legal pair.
- **The agents.** In every game both ISMCTS seats played close to uniformly at random: turns ended with
  everything unspent, beneficial effects put on enemy units (Bounty Hunter's [Ganking] 5 of 5, Pridestalker's
  +1 [M] 6 of 8), own spells countered, a winning move not taken (game 8). No reader found a missing option
  behind it; it is recorded as an observation in section 8, with the two places that deserve a look.

## 2. The 19 games

Pairing: `scripts/testing/games19_pairing.py decks <plan.tsv>` — the sorted list names are shuffled with
`random.Random(20261011)`, neighbours are paired, a second draw per pairing decides which list is `--deck1`;
game k has engine seed `2026101100 + k`. The plan is run with `scripts/testing/run_deck_games.py` (three at a
time, under the run lock), which also writes the HTML replay and its text form.

| # | deck1 (P1) | deck2 (P2) | seed | winner | score P1-P2 | turns | decisions | engine error |
|---|---|---|---|---|---|---|---|---|
| 1 | irelia | ornn | 2026101101 | P2 | 5-8 | 17 | 289 | none |
| 2 | fiora | ivern | 2026101102 | P2 | 3-8 | 12 | 209 | none |
| 3 | pyke | kaisa | 2026101103 | P2 | 6-8 | 18 | 355 | none |
| 4 | rengar | vi | 2026101104 | P2 | 4-8 | 17 | 260 | none |
| 5 | lucian | ezreal | 2026101105 | P1 | 8-2 | 16 | 307 | none |
| 6 | kennen | diana | 2026101106 | P1 | 8-5 | 14 | 355 | none |
| 7 | yasuao | khazix | 2026101107 | P2 | 4-8 | 11 | 174 | none |
| 8 | mel | vex | 2026101108 | P1 | 8-7 | 22 | 405 | none |
| 9 | rumble | reksai | 2026101109 | P2 | 6-8 | 20 | 393 | none |
| 10 | leona | darius | 2026101110 | P2 | 2-8 | 15 | 259 | none |
| 11 | lux | draven | 2026101111 | P1 | 8-7 | 26 | 411 | none |
| 12 | sivir | viktor | 2026101112 | P1 | 8-4 | 17 | 361 | none |
| 13 | renata | poppy | 2026101113 | P2 | 3-8 | 17 | 304 | none |
| 14 | yi_rebuild | ahri | 2026101114 | P2 | 4-8 | 14 | 295 | none |
| 15 | leblanc | akali | 2026101115 | P2 | 2-8 | 14 | 206 | none |
| 16 | jayce | ambessa | 2026101116 | P2 | 2-8 | 18 | 329 | none |
| 17 | lillia | azir | 2026101117 | P1 | 8-0 | 12 | 206 | none |
| 18 | missfortune | zed | 2026101118 | P2 | 6-8 | 17 | 334 | none |
| 19 | teemo | nasus | 2026101119 | P2 | 3-8 | 15 | 311 | none |

"turns" is the engine's `turns=` of the `Game over` line; "decisions" is the last `DECISION #n` of the trace.
Games are named g01-g19 below.

One command replays a game (ISMCTS is a function of its seeds; game 7 was run again from this command and its
output is identical to the saved log line for line). Game k of the table, with `<a>` `<b>` its lists:

```
RIFTBOUND_ROOT=. ./build/riftbound --deck1 decks/<a>.txt --deck2 decks/<b>.txt \
    --agent1 ismcts:sims=50 --agent2 ismcts:sims=50 --seed <2026101100 + k> --trace --debug
```

e.g. g11: `RIFTBOUND_ROOT=. ./build/riftbound --deck1 decks/lux.txt --deck2 decks/draven.txt --agent1 ismcts:sims=50 --agent2 ismcts:sims=50 --seed 2026101111 --trace --debug`.
Add `--render-html on --show-hand --replay-dir <dir>` for the HTML replay (it does not change the game).

Numbering: `[D:027.03]` is the trace prefix a replayed game prints, and what is cited here. `DECISION #n`
inside a line and the replay's `idx=n` are the engine's decision index, which runs ahead of it; "idx=" is given
where an option list is meant.

How the reading was done: `scripts/testing/games19_cardsheet.py` writes, from `--dump-registry`, the printed
text of every card of the two lists (and of all 955 cards); `scripts/testing/games19_reading.py` turns the
replay text into one reading file per game (per decision: the board with every hand and facedown card, as a
full board at each turn and line changes in between; all legal options; the option chosen; the trace lines);
`scripts/testing/games19_check.py` prints the table above and checks quoted trace lines against the logs
(`cite`). Each reader got the same written brief, its game's files, the rules and the known-item list, and
wrote notes; the lead merged the notes, dropped nothing, and re-read each new finding's rules.

## 3. Coverage, game by game

All 19 readers state that every snapshot of their reading file and the tail of the log were read; none reports
a part left unread. "D:" is the range of trace prefixes, "idx" the engine's decision indices.

| game | read | cards never seen at work are listed in the reader's notes |
|---|---|---|
| g01 irelia / ornn | idx 3-289, D:001-267 and the log tail: all read | yes |
| g02 fiora / ivern | idx 3-209, D:001-194 and the log tail: all read | yes |
| g03 pyke / kaisa | idx 3-355, D:001-321 and the log tail: all read | yes |
| g04 rengar / vi | idx 3-262, D:001-233 and the log tail: all read | yes |
| g05 lucian / ezreal | idx 3-307, D:001-274 and the log tail: all read | yes |
| g06 kennen / diana | idx 3-355, D:001-330 and the log tail: all read | yes |
| g07 yasuao / khazix | idx 3-176, D:001-160 and the log tail: all read | yes |
| g08 mel / vex | idx 3-405, D:001-362 and the log tail: all read | yes |
| g09 rumble / reksai | idx 3-393, D:001-363 and the log tail: all read | yes |
| g10 leona / darius | idx 3-259, D:001-227 and the log tail: all read | yes |
| g11 lux / draven | idx 3-411, D:001-365 and the log tail: all read | yes |
| g12 sivir / viktor | idx 3-361, D:001-331 and the log tail: all read | yes |
| g13 renata / poppy | idx 3-304, D:001-280 and the log tail: all read | yes |
| g14 yi_rebuild / ahri | idx 3-295, D:001-268 and the log tail: all read | yes |
| g15 leblanc / akali | idx 3-206, D:001-193 and the log tail: all read | yes |
| g16 jayce / ambessa | idx 3-331, D:001-321 and the log tail: all read | yes |
| g17 lillia / azir | idx 3-208, D:001-185 and the log tail: all read | yes |
| g18 missfortune / zed | idx 3-334, D:001-289 and the log tail: all read | yes |
| g19 teemo / nasus | idx 3-311, D:001-286 and the log tail: all read | yes |

What the reading is not:

- **Not every card.** A game shows the cards that were drawn and played. Kennen, Storm of Shuriken never
  conquered in g06, so the legend's granted [Flow] (D78, the named focus of the Kennen list) was not exercised;
  only two printed-Flow plays were read there.
- **Not a seat's view.** The reading file is the omniscient record. Hidden information can only be judged from
  which lines exist (D11 again), not from who may read them.
- **The readers' depth on the known list differs.** Every reader read the 93 one-line items in full; the detail
  sections of the decks document were read for the items each reader matched against (g09's reader says so in
  terms). A known item may therefore be reported here under a new number if its detail names the case; the lead
  checked each G against both findings documents by card name and by rule (section 4, "Not the same as").
- Two readers ran one read-only `git log -1` in the worktree against their brief (g02, g13); nothing else was
  run, built or written by any reader outside its notes file.

## 4. New findings, by root cause

Fix batches are those of `plans/remaining/10-testing-phase.md`: 1 triggers and the chain, 2 combat timing and
continuous effects, 3 targets / finalization / payments, 4 cards / options / validator.

| id | root | finding | confidence | batch | games |
|---|---|---|---|---|---|
| G1 | turn structure | 1v1: the first player's first draw is skipped | CERTAIN | 4 | all 19 |
| G2 | turn structure | An Additional Turn made on the opponent's turn replaces the owner's regular turn (Time Warp) | CERTAIN | 4 | g11 |
| G3 | turn structure | The turn player always gets Focus first, not the player who applied Contested / the attacker | CERTAIN (combat), LIKELY (non-combat) | 2 | g07 g10 g14 g19 |
| G4 | turn structure | Setup: each player's battlefield is an agent decision, not a random pick (Duel) | QUESTION | 4 | all 19 |
| G5 | cleanup after a chain | A unit with lethal damage is still on the board at the Focus decision after the attack / defend triggers have resolved and the chain is empty | CERTAIN | 1 | g01 |
| G6 | cleanup after a chain | A battlefield left without units keeps its controller through a Main Phase decision when the last unit's Deathknell was queued by the cleanup that killed it | CERTAIN | 1 | g13 |
| G7 | continuous effects | Sivir, Mercenary's "this turn" condition is still applied through the next turn's Beginning Step | CERTAIN | 2 | g12 |
| G8 | continuous effects (known root D42 / D60 / D76) | Mechanized Menace: "Your Mechs have [Shield]" is missing when a combat goes straight to its damage step; outcome changed | CERTAIN | 2 | g09 |
| G9 | trigger conditions (known root D51) | Eight more cards put an ability on the chain while the condition printed with the trigger is false | CERTAIN (Vanguard Captain LIKELY) | 1 | g01 g03 g09 g10 g11 g13 g15 g16 g19 |
| G10 | trigger control | A battlefield's "At the start of ... Beginning Phase" ability is given to the turn player while the other player controls the battlefield (Frozen Fortress, Dusk Rose Lab) | LIKELY | 1 | g05 g17 |
| G11 | trigger events | Heart of the Tempest: no trigger for a play from the Champion Zone or a play by an effect from the trash | CERTAIN | 1 | g06 |
| G12 | trigger events | Heart of the Tempest triggers when the Flow spell goes on the chain, not when it has resolved | CERTAIN | 1 | g06 |
| G13 | trigger events | Tornado Warrior: "Disempower it at end of turn" is a flag flipped in the Expiration Step, not an ability of the Ending Step | LIKELY | 1 | g07 |
| G14 | targets | A chosen target that left the board and came back is still acted on (Amateur Recital + Temporal Breach) | CERTAIN | 3 | g14 |
| G15 | order of choices and costs | Activated abilities with a target or mode: the whole cost is paid before the choice (Heart of the Tempest, Defender of Tomorrow, Matriarch of War, Bounty Hunter) | CERTAIN | 3 | g06 g16 g18 |
| G16 | order of choices and costs | A play-time-target spell played by Fizz, Trickster is paid for before its target is chosen (Punch First) | CERTAIN | 3 | g12 |
| G17 | choices at resolution | Emperor's Divide: the units to move are chosen after RESOLVE; the unit named at the play only stands for the battlefield | LIKELY | 3 | g10 |
| G18 | pick list narrower than the text | Defender of Tomorrow: "Ready a gear" refuses a ready gear | CERTAIN | 4 | g16 |
| G19 | no legal choice (known root D67) | Defender of Tomorrow: activated, paid and put on the chain with nothing to choose | CERTAIN | 3 | g16 |
| G20 | no legal choice (known root D8 / F7) | Public Execution: offered, paid for and put on the chain with no legal pair | CERTAIN | 3 | g16 |
| G21 | facedown plays (known root D70) | Kennen, Keeper of Balance from facedown: the Stun is offered and taken on a unit in the enemy base | CERTAIN | 4 | g04 |
| G22 | facedown plays | Hidden units (Tornado Warrior, Tideturner) are offered and played onto Rockfall Path | CERTAIN | 4 | g08 |
| G23 | battlefield replacement | A Brush that replaced Vilemaw's Lair keeps "Units can't move from here to base" | LIKELY | 4 | g02 |
| G24 | battlefield replacement | Green Father's "may" is withheld when the battlefield is already a Brush | LIKELY | 4 | g02 |
| G25 | Astral Heron | With the first card of the turn countered, no card of that turn triggers the Heron | QUESTION | 4 | g08 |
| G26 | play watchers | Ravenbloom Student played back by Temporal Breach triggers on that same Temporal Breach | QUESTION | 1 | g14 |

### G1  1v1: the player going first is made to skip the draw of their first Draw Phase
- Confidence: CERTAIN on the text of the 2026-10-09 rules. Batch 4 (turn structure; no other batch touches it).
- Rule: CR 315.4.b: "1. The **Turn Player** draws 1." CR 487.7 (1v1 Duel) and 488.7 (1v1 Match), each in
  full: "**First Turn Process:** The player going second channels an *extra* Rune from their Rune Deck during
  their first **Channel Phase** of the game." The sentence "The player going first *does not draw a card*
  during their first **Draw Phase** of the game" is the text of 489.7 (FFA3), 490.7 (FFA4) and 491.7 (2v2) only.
- Where: the first Draw Phase of every one of the 19 games (the line occurs once in each log). Found
  independently by 12 readers (g01, g03, g04, g05, g07, g08, g10, g11, g12, g17, g18, g19).
- Trace, g01:
  `[D:004.13] ── Draw Phase ──`
  `[D:004.14] DRAW PHASE: P1 skips draw (first turn, going first — CR 482.7)`
  `[D:004.15] ── Main Phase ──`
  g03, where P2 goes first: `[D:004.14] DRAW PHASE: P2 skips draw (first turn, going first — CR 482.7)`.
  The first player's deck stays at 35 and the hand at the 4 mulligan cards into the first Main Phase decision.
- Should have happened: the first player draws 1 (hand 5, deck 34); the second player's third rune, which the
  engine gives, is the whole 1v1 adjustment. As it is, the first player is one card short for the whole game.
- Why (read to explain, not the evidence): `GameEngine` draw phase, comment "CR 482.7 / 483.7 / 484.7: the
  player going first does not draw a card" — three rule numbers, which fit the three multiplayer modes before
  the rules were renumbered (now 489.7 / 490.7 / 491.7), applied with no test of the mode.
  `tests/cards/test_burn_out_and_scoring.cpp`, `BurnOutScoringTest.FirstPlayerFirstTurn_DrawPhaseSkipped`, pins
  the skip and has to be corrected with the fix.
- What could not be determined: whether a ruling outside the repository gives 1v1 the skip. Nothing in
  `rules/patch-notes/` or `rules/tournament-rules.md` does. If the project wants it confirmed, it is judge
  question J1 (section 7).
- Not the same as: none of the 93 items is about the first turn; the decks document checks only the extra rune
  ("RUNES. Second player channels 3 ... CR 487.7").

### G2  Time Warp: an Additional Turn created on the opponent's turn replaces its owner's regular turn
- Confidence: CERTAIN. Batch 4 (turn queue; stands alone).
- Text / rule: Time Warp "Take a turn after this one. Banish this." CR 735: "These effects create a temporary
  **Additional Turn** owned by that player that is inserted into the turn queue after the current turn."
  CR 737: "When an **Additional Turn** is inserted into this queue, it does not change the **Turn Order** of the
  game. The owner of the **Additional Turn** just has the next queued turn. After that turn is completed, it will
  be removed and the queue will proceed with its previously queued turns." CR 738's second example shows an
  Additional Turn of B made during A's turn followed by B's own queued turn.
- Where: g11, Time Warp resolved at D:226 (idx=249) during P2's turn 18; turn hand-off D:256-257.
- Trace:
  `[D:226.02] RESOLVE: Time Warp (chain_item=44, ability=0, spell=1)`
  `[D:226.03] TIME WARP: queued extra turn for P1`
  `[D:256.04] ADDITIONAL_TURN: P1`
  `[D:256.05] ════ TURN 19 (P1) ════ Score: P1=4 P2=6`
  `[D:257.04] ════ TURN 20 (P2) ════ Score: P1=4 P2=6`
  `[D:278.04] ════ TURN 21 (P1) ════ Score: P1=4 P2=7`
- Should have happened: P2 (18), P1's Additional Turn (19), P1's own queued turn (20), then P2. The engine
  played P2, P1, P2, P1 — the sequence without Time Warp, so the spell (10 Energy and 4 Power) did nothing.
  The second Time Warp of the game, played on P1's own turn, came out right
  (`[D:308.04] ADDITIONAL_TURN: P1`, turn 22 P1, turn 23 P2): only the cross-turn case is wrong.
- Note: here the spell was playable on the opponent's turn only through D5 (offered at Focus in a combat). The
  case is reachable without D5 by any effect that plays the spell on an opponent's turn (CR 738's own example).
- Not the same as: D5 is the offer; this is what the resolved effect does. No known item mentions Additional Turns.

### G3  The turn player always gets Focus first — not the player who applied Contested, not the attacker
- Confidence: CERTAIN for a combat (CR 466.2.c.1.a in terms); LIKELY for a non-combat showdown (the one
  reading step is that "the player who applied Contested" is the controller of the unit that applied it).
  Batch 2.
- Rule: CR 345: "As a **Showdown** begins, the player who applied **Contested** status to the **Battlefield**
  gains **Focus.**" CR 190.3.a.1: "**Units** moving to or being played to a battlefield apply **Contested**
  status if that **Battlefield** is not already **Contested** and that **Unit**’s controller does not already
  control that battlefield." CR 466.2.c.1: "The **Attacker** is the player whose unit(s) applied the
  **Contested** status to the **Battlefield.**" 466.2.c.1.a: "If a showdown opens as part of combat, this player
  gains Focus as the showdown begins."
- Where and trace (the Focus decisions print no trace line of their own; the order is the order of the
  decisions in the replay):
  - g19, combat, turn 13 (P2's turn): P2's Charm moves P1's Teemo, Scout into Dusk Rose Lab, held by P2.
    `[D:262.04] CHARM: moved enemy Teemo, Scout to Battlefield #0`
    `[D:264.03] COMBAT: BF#0 attacker=P1 defender=P2`
    idx=286 `==> P2 (forced): Pass Focus`, then idx=287 `==> P1 (forced): Pass Focus`. The engine itself names
    P1 the attacker and asks P2 first.
  - g07, non-combat, turn 9 (P2's turn): Void Assault puts P1's Fizz alone on Seat of Power.
    `[D:107.05] SHOWDOWN: BF#0 (non-combat)` / `[D:107.06] CONQUER: P1 conquers BF#0 (score was 2)`;
    idx=120 P2 passes Focus, then idx=121 P1.
  - g10, non-combat, turn 8 (P1's turn): `[D:077.04] CHARM: moved enemy Kai'Sa, Survivor to Battlefield #0` /
    `[D:077.05] SHOWDOWN: BF#0 (non-combat)`; idx=86 P1 (forced) passes, then idx=87 P2 with
    `[1] Play Hidden Blade #213[S]` and `[2] Play Inferna #560(1M) -> Monastery of Hirana` on offer.
  - g14, non-combat, turn 3 (P1's turn): `[D:036.04] CHARM: moved enemy Mutated Mouser to Battlefield #0` /
    `[D:036.05] SHOWDOWN: BF#0 (non-combat)`; idx=37 P1 is asked first with `[1] Play Back Off #604[S]`, then
    idx=38 P2.
- Should have happened: the player whose unit applied Contested (the attacker in a combat) has Focus first.
  No outcome changed in these four (both passed each time); in general the order decides who must commit first.
- Why (source, not the evidence): `GameEngine::runShowdownLoop`, "Turn player gets focus first"; the attacker
  is taken from `contested_by`, Focus is not.
- Not the same as: D31 (Focus after an [Add]); D7 (an item pending through the showdown); the decks document's
  checked entry on Blitzcrank is about who the ATTACKER is, which is right here too.

### G4  Setup: each player's battlefield is an agent decision, not a random selection
- Confidence: QUESTION (to the project, not to a judge: which Mode of Play a single engine game is). Batch 4.
- Rule: CR 487.5 (1v1 Duel, best of 1): "Each player randomly selects one (1) of their three (3)
  **Battlefields**." CR 488.5 (1v1 Match, best of 3): "Each player selects one (1) of their three (3)
  **Battlefields**."
- Where: the first two decisions of every game. g18:
  `[D:001.00] DECISION #1 (P1 — battlefield setup): 3 options`
  `[D:001.01]   CHOSE BF #0 (Targon's Peak)`
  `[D:002.00] DECISION #2 (P2 — battlefield setup): 3 options`
- If one engine game is a Duel, there is no decision and the pick is random; if it is game one of a Match, the
  decision is right (and games two and three would exclude the battlefields already used, 488.5). The engine
  plays one game and the lists carry sideboards. Recorded so that the choice is made on purpose.

### G5  A unit with lethal damage is still on the board at the next Focus decision although the chain is empty
- Confidence: CERTAIN (the same rules as D44). Batch 1.
- Text / rule: Forgefire Cape "When I attack or defend, deal 2 to all enemy units here." CR 319.5: a Cleanup
  follows "After a **Chain Item** is removed from the **Chain** for any reason". CR 323.4: "All **Units** that
  have **Lethal Damage** marked on them are killed and placed in their owners' **Trash.**"
- Where: g01, D:246-254 (idx=266-272), turn 15, combat at Targon's Peak.
- Trace:
  `[D:246.04] COMBAT: BF#0 attacker=P2 defender=P1`
  `[D:246.05] EQUIP_TRIGGER: Forgefire Cape fires on Aphelios, Exalted (P2)`
  `[D:248.02] RESOLVE: Forgefire Cape (chain_item=59, ability=1, spell=0)`
  `[D:248.03] EFFECT: deal 2 damage to Tideturner (id=9, was 0/2M)`
  `[D:249.01] TARGET (play): Charm -> Tideturner`
  `[D:254.04] CHARM: moved enemy Tideturner to Battlefield #1`
  `[D:254.05] KILL: Tideturner (id=9, 2M, dmg=2)`
- Options: idx=266, chain empty, board `BF Targon's Peak | P1: Tideturner(2M,2dmg,rdy,Hdn,DEF)
  Adaptatron(4M,2dmg,rdy,+1buf,DEF)`: P2 has Focus with `[1] Play Charm #43[S]`; idx=267 offers
  `[0] Choose: Tideturner #199 (obj=9)@BF`. The dead Tideturner was chosen and moved to the other battlefield
  before it was killed.
- Should have happened: the cleanup after the Forgefire Cape item left the chain kills Tideturner before P2
  gets Focus; it is not a choice for Charm.
- Not the same as: D44 is "until the chain is empty: no cleanup between the items of a chain". Here the chain
  had emptied (one item, resolved) and a Focus decision was asked with the unit still there; the kill came with
  the cleanup after the NEXT chain. So the run of the attack / defend triggers at the start of a combat is not
  followed by a lethal-damage cleanup. Not determined: whether, had P2 passed, the Tideturner would have dealt
  its 2 in the damage step.

### G6  A battlefield with no units keeps its controller through a Main Phase decision
- Confidence: CERTAIN on the rule and the state shown; no option or score changed in this game. Batch 1.
- Rule: CR 323.5: "Players lose control of any controlled **Battlefields** without their **Units** occupying
  them if the turn is in an **Open State** and there is no **Showdown** or **Combat** ongoing there."
- Where: g13, D:061-064 (idx=64-66), turn 6, Sunken Temple.
- Trace:
  `[D:061.03] EFFECT: kill Renata Glasc, Mastermind (id=2)`
  `[D:061.08] KILL: Soaring Scout (id=25, 1M, dmg=3)`
  `[D:061.09] TRIGGER: Soaring Scout (P1) fires ability`
  `[D:063.02] RESOLVE: Soaring Scout (chain_item=12, ability=1, spell=0)`
  `[D:064.00] DECISION #66 (P1): 1 legal actions`
  `[D:064.02] ── Ending Step ──`
- Board: at idx=64 `BF Sunken Temple | P1: --`; at idx=66 (the Main Phase decision, chain empty, Open State)
  the header is still `BF Sunken Temple  [Ctrl:P1]`; it becomes `[Ctrl:--]` only at the Expiration Step.
  Same game, where it works: Blood Money kills the last unit there with nothing queued
  (`[D:121.03] EFFECT: kill Recruit (id=110)`) and the next decision's board already has `[Ctrl:--]`, with Card
  Sharp no longer offered `-> Sunken Temple`.
- Should have happened: the cleanup after the Deathknell resolved drops control before decision #66.
- Consequence if the option generator reads the controller (not exercised: P1 could afford nothing): a unit
  played to, or a card hidden at, an empty battlefield the player no longer holds, with no showdown.
- Why (source): `GameEngine::updateBattlefieldControl` drops control only in a Neutral Open state; the pass that
  killed the Scout had just queued its Deathknell (Closed), and the board shows no later pass did it.
- Not the same as: D44 (between the items of one chain); D10 (Equipment not recalled: the same family, another
  cleanup step). G5 and G6 may share a root: no full cleanup once a chain has emptied on these paths.

### G7  Sivir, Mercenary: the turn-scoped +2 [M] and [Ganking] are still on her in the opponent's Beginning Step
- Confidence: CERTAIN that the state is stale at those decisions; no outcome changed. Batch 2 (with D23).
- Text / rule: "If you've spent at least [A][A] this turn, I have +2 [M] and [Ganking]." CR 317.2.c: "All
  'this turn' effects expire simultaneously." CR 319.2: a Cleanup happens "After the game transitions between
  **Phases**".
- Where: g12, three turn changes (D:120-122, D:169-171, D:301-303).
- Trace (turn 9 to 10; P1 spent 3 Power on turn 9, none on turn 10):
  `[D:169.03] ── Expiration Step ──`
  `[D:169.04] ════ TURN 10 (P2) ════ Score: P1=2 P2=3`
  `[D:169.07] TRIGGER: Dusk Rose Lab (P2) fires ability`
  `[D:170.00] DECISION #180 (P2 — chain priority): 1 legal actions`
  `[D:171.00] DECISION #181 (P1 — chain priority): 1 legal actions`
  Boards: `BF Zaun Warrens | P1: Sivir, Mercenary(8M,exh,Acc,EQ:Edge o..)` through both priority decisions of
  turn 10's Beginning Step; `(6M,...)` from the Scoring Step on.
- Should have happened: 6M from the end of P1's turn. A Reaction played in those windows that reads Might
  (Gust "3 [M] or less", Blood Money "2 [M] or less") or Ganking reads the stale value.
- Not the same as: D23 / D44 are within one turn. Here the counter is reset at the turn change and nothing
  recomputes continuous effects before the next turn's first chain.
- Side note, CODE-ONLY: `power_spent_this_turn` is counted only where a rune is recycled; Power paid from the
  pool (a Gold's [A]) is not counted as spent for Sivir.

### G8  Mechanized Menace: the [Shield] it gives is missing when a combat goes straight to its damage step
- Confidence: CERTAIN. KNOWN ROOT (D42 / D60 / D76, D23): numbered because the card is in none of them, the
  doubt D42 left is settled here, and the outcome of the game turned on it. Batch 2.
- Text / rule: "Your Mechs have [Shield]. (+1 [M] while they're defenders.)" CR 814.1.c: "It is functionally
  short for "While I am a defender, I have +X [M]."" CR 814.1.b.3: "If X is omitted, it is presumed to be 1."
- Where: g09, D:125 (idx=128-131), turn 8; defenders Forecaster and Rumble, Hotheaded, both Mechs.
- Trace:
  `[D:125.04] COMBAT: BF#1 attacker=P2 defender=P1`
  `[D:125.05] DAMAGE_STEP: BF#1 ATK=[Carrion Dredger(1M), Inferna(3M), Rek'Sai, Breacher(4M)] DEF=[Forecaster(2M), Rumble, Hotheaded(4M)]`
  `[D:125.09] ASSIGN_DAMAGE: P1 assigns 6 damage:`
  `[D:125.12]   -> Rek'Sai, Breacher takes 2 (was 0/4M)`
  `[D:127.05] CONQUER: P2 conquers BF#1 (score was 0)`
- Should have happened: DEF=[Forecaster(3M), Rumble, Hotheaded(5M)]; P1 assigns 8, exactly lethal for all
  three attackers, and P2's 8 is exactly lethal for both defenders: nobody is left, no conquer, no point for P2
  at D:127, no Void Burrower trigger. P2 won this game 8-6.
- What this sighting adds: two turns later the same aura IS applied once something has resolved inside the
  combat (the Mech token is 3M at the first Focus decision and 4M after a spell went on the chain:
  `[D:189.03] EFFECT: deal 3 damage to Mech (id=109, was 1/4M)`), so the grant is stale, not missing; and the
  attackers' PRINTED [Assault] is counted in the very damage step that lacks the granted [Shield], so the lag
  belongs to keywords that arrive through the aura pass. Rumble, Hotheaded's "Your Mechs each have [Assault]"
  is the same construction and was not exercised.

### G9  Eight more cards put an ability on the chain while the condition printed with the trigger is false
- Confidence: CERTAIN; LIKELY for Vanguard Captain ([Legion]). KNOWN ROOT (D51, also D27 / D33): numbered
  because each card needs its own fix (none has a trigger condition; each tests in its resolution). Batch 1.
- Rule: CR 383.2.a.1: "Any additional conditional statement immediately after the **Condition** must be true in
  order for the **Condition** to be fulfilled. Such a conditional statement is part of the **Trigger Condition**
  and not the **Effect.**" For [Legion], CR 812.1.b.1: "**Legion** is functionally short for “If you have played
  another card this turn, this card gains [Text].”" — with Legion off the card has no play ability at all.
- In every case below the item goes on the chain, a Closed State opens, both players get a priority decision,
  and it "resolves" with nothing. None is D4: nothing was declined; the ability should not have triggered.

| card | text | where | trace |
|---|---|---|---|
| Curator of the Sands (legend) | "When you play a unit, gear, or activated ability with Energy cost [7] or more, you may exhaust me to ready up to 2 runes." | g19: 14 of its 16 items — five spells (Stupefy, Discipline, Consult the Past, En Garde, Charm), eight units costing less than [7], one [3] ability | `[D:021.06] TRIGGER: Curator of the Sands (P2) fires ability` after `[D:021.02] RESOLVE: Stupefy (chain_item=5, ability=0, spell=1)`; `[D:050.02] TRIGGER: Curator of the Sands (P2) fires ability` for a [5] unit |
| Lady of Luminosity - Starter (legend) | "When you play a spell that costs [5] or more, draw 1." | g11: 4 of 10 items (spells costing 1, 2, 3, 1) | `[D:019.06] TRIGGER: Lady of Luminosity - Starter (P1) fires ability` / `[D:021.03] LADY OF LUMINOSITY: spell cost 1 < 5 — no draw` |
| Gutter Palace | "At the start of your Beginning Phase, if you have exactly 4 cards in hand and exactly 4 units at battlefields, you win the game." | g16: every P1 turn after it was played (5 times; 0 or 1 unit at battlefields) | `[D:088.07] TRIGGER: Gutter Palace (P1) fires ability` / `[D:096.02] RESOLVE: Gutter Palace (chain_item=21, ability=1, spell=0)` |
| Patched Porobot | "When you play me, if you control 3 or more other gear, draw 1." | g01 D:243-245 (one gear), g09 D:168-170 (no gear) | g01 `[D:243.02] TRIGGER: Patched Porobot (P2) fires ability`; g09 `[D:168.02] TRIGGER: Patched Porobot (P1) fires ability` |
| Poppy, Paragon | "When you play me, if an opponent's score is within 3 points of the Victory Score, ready me and gain 3 XP." | g13 D:027-029 (opponent at 0) | `[D:027.02] TRIGGER: Poppy, Paragon (P2) fires ability` / `[D:029.02] RESOLVE: Poppy, Paragon (chain_item=5, ability=1, spell=0)` |
| Pyke, Dockside Butcher | "When you play me, if you paid the additional cost, ready me and give me +2 [M] this turn." | g03 D:043-045 (cost declined) | `[D:043.01] ADDITIONAL_COST: Pyke, Dockside Butcher declined additional cost` / `[D:043.02] TRIGGER: Pyke, Dockside Butcher (P1) fires ability` |
| Blitzcrank, Impassive | "When you play me to a battlefield, you may move an enemy unit to here." | g15 D:156-169 (played to the base) | `[D:162.02] TRIGGER: Blitzcrank, Impassive (P2) fires ability` / `[D:162.04] MAY_NOT_OFFERED: Blitzcrank: pull an enemy unit to my battlefield? (precondition failed at prompt time)` |
| Vanguard Captain | "[Legion] — When you play me, play two 1 [M] Recruit unit tokens here." | g10 D:036-041 (first card of the turn) | `[D:036.02] TRIGGER: Vanguard Captain (P2) fires ability` / `[D:041.02] RESOLVE: Vanguard Captain (chain_item=6, ability=1, spell=0)` |

- What the windows did: in g16 P1 used Gutter Palace's window in four Beginning Steps to exhaust Platewyrm Eggs;
  in g19 an empty Curator item was the only thing on the chain when P2 played Consult the Past in that Closed
  State, and a second empty item was queued on top (board idx=195: two Curator items); in g15 the item kept P2
  out of its Main Phase options until both players had passed and gave P1 a priority decision the rules do not
  give. Mischievous Marai, with Blitzcrank's wording, does it right in g15 (no `TRIGGER` line when played to the
  base): it has a trigger condition.
- Should have happened: no trigger, no chain item, no priority window.
- By code (not seen): the converse half — condition true at the trigger and false at resolution must still
  resolve — and, for Vanguard Captain, Legion becoming true while the item waits (it would then make the
  Recruits). Trifarian Gloryseeker and every other "[Legion] — When you play me" card are suspect.
- Not the same as: D51 names Kha'Zix, Mutating Horror, Evelynn, Entrancing and Tornado Warrior; D27 Pyke,
  Returned and Edge of Night; D33 Vex, Apathetic; D19 Astral Heron; D77 Yasuo. The judge question Q4 of the
  decks document (Pyke, Dockside Butcher; Noxian Emissary) asks whether such an ability triggers at all; CR
  383.2.a.1 answers it, so Pyke is listed here as a defect.

### G10  A battlefield's Beginning Phase ability is given to the turn player while the other player controls it
- Confidence: LIKELY (opponent-controlled case); the uncontrolled case of Dusk Rose Lab is judge question J5.
  Batch 1.
- Text / rule: Frozen Fortress "At the start of each player's Beginning Phase, deal 1 to each unit here."
  Dusk Rose Lab "At the start of your Beginning Phase, you may kill a unit you control here to draw 1."
  CR 190.6.a: "While a **Battlefield** is **Controlled**, its **Controller** controls its **Abilities** unless
  that ability indicates another player does. The controlling player takes responsibility for adding them to the
  **Chain** if applicable, and makes all choices required by them unless otherwise specified." CR 190.6.d:
  "“You” in a battlefield’s abilities refers to the battlefield’s **Controller** ... If the battlefield has no
  **Controller**, “you” refers to no one, and all such instructions are ignored."
- Where and trace:
  - g05, Frozen Fortress controlled by P2 in P1's turn 12, by P1 in P2's turns 13 and 15:
    `[D:167.07] TRIGGER: Frozen Fortress (P1) fires ability` / `[D:168.00] DECISION #183 (P1 — chain priority): 3 legal actions`;
    `[D:194.07] TRIGGER: Frozen Fortress (P2) fires ability`; `[D:250.07] TRIGGER: Frozen Fortress (P2) fires ability`.
    The damage is the same either way; who has priority first, and whose ability dealt it, is not.
  - g17, Dusk Rose Lab controlled by P1 in P2's Beginning Steps (turns 1, 3, 7):
    `[D:038.07] TRIGGER: Dusk Rose Lab (P2) fires ability`
    `[D:038.09] MAY_NOT_OFFERED: Dusk Rose Lab: kill a unit here to draw 1? (precondition failed at prompt time)`
    `[D:040.00] DECISION #46 (P1 — chain priority): 2 legal actions` — with `[1] Play Smoke and Mirrors #645[S] @ Sprite`
    (P1's facedown card) on offer in a window that exists only because of this item.
- Should have happened: Frozen Fortress's ability is its controller's (that player puts it on the chain and has
  priority first). Dusk Rose Lab does not trigger in the Beginning Phase of a player who does not control it:
  "your" is the controller.
- Why (source): `TriggerManager` attributes battlefield start-of-Beginning triggers to the turn player; Dusk Rose
  Lab's own resolution then tests the controller, hence `MAY_NOT_OFFERED`.
- Not the same as: D4 (an item that triggered for the right player and has nothing to do). A D4 fix that drops
  an unofferable "may" would hide the Dusk Rose Lab symptom and leave the wrong controller.

### G11  Heart of the Tempest: no trigger for a play from the Champion Zone, or from the trash by an effect
- Confidence: CERTAIN. Batch 1 (the event that feeds the trigger).
- Text / rule: "When you play a card from anywhere other than your hand, empower me." CR 420.1.a: "By default, a
  player can only **Play** cards from their hand or their Chosen Champion zone." (two zones). CR 420.3: "Game
  effects may result in cards being **Played** as part of their resolution."
- Where: g06 (a) D:134-146, turn 8, Kennen, Storm of Shuriken played from the Champion Zone; (b) D:294, turn 12,
  Tail-Cloaked Matriarch plays Ezreal, Prodigy from the trash.
- Trace:
  (a) `[D:134.03] PLAY: Kennen, Storm of Shuriken (id=2, Unit, cost=3E)`
  `[D:138.02] TRIGGER: Kennen, Storm of Shuriken (P1) fires ability`
  `[D:138.03] TRIGGER: Vex, Apathetic (P2) fires ability` — and no `TRIGGER: Heart of the Tempest` between
  D:134 and D:147.
  (b) `[D:294.03] TRIGGER: Ezreal, Prodigy (P1) fires ability`
  `[D:294.05] TAIL-CLOAKED MATRIARCH: played Ezreal, Prodigy from trash to base` — no legend trigger.
  The legend's only two triggers of the game are the two Flow plays:
  `[D:185.06] TRIGGER: Heart of the Tempest (P1) fires ability`, `[D:210.06] TRIGGER: Heart of the Tempest (P1) fires ability`.
- Should have happened: the legend is empowered after each of the two plays; in (a) P1 would have had
  `Activate Heart of the Tempest` ([Assault 2]) for the attack that followed.
- Not the same as: no known item names the legend or "not from hand".

### G12  Heart of the Tempest triggers when the Flow spell is put on the chain, not when it has resolved
- Confidence: CERTAIN on the order; no outcome changed. Batch 1.
- Rule: CR 420.4.a: "Any such triggered abilities trigger when the act of playing the card has been *completed*
  by the resolution of the card." 420.4.a.1: "If a game effect prevents the resolution of the card—for example,
  because the card was **countered**—abilities that trigger on playing cards will not trigger."
- Where: g06 D:185-201 (Up from the Deep by Flow), D:210-224 (Lightning Rush by Flow), turn 10.
- Trace:
  `[D:185.05] TRASH_REPLAY: P1 replayed card id=38 for [3]+power`
  `[D:185.06] TRIGGER: Heart of the Tempest (P1) fires ability`
  `[D:187.02] RESOLVE: Up from the Deep (chain_item=52, ability=0, spell=1)`
  `[D:201.02] RESOLVE: Heart of the Tempest (chain_item=51, ability=1, spell=0)`
  The legend's item (51) is on the chain before and under the spell (52). In the same plays Abandoned Hall
  waits for the resolution: `[D:189.05] TRIGGER: Abandoned Hall (P1) fires ability`.
- Should have happened: the ability triggers once the spell has resolved, and not at all if it is countered. As
  it is, a countered Flow spell still empowers the legend.
- Not the same as: D1 (a permanent's own play triggers queued before it enters).

### G13  Tornado Warrior: "Disempower it at end of turn" is done in the Expiration Step with no ability
- Confidence: LIKELY. Batch 1 (the D53 pattern, another step).
- Text / rule: "When you play me from face down, you may empower something here. Disempower it at end of turn."
  CR 317.1.a (Ending Step): "At the end of this turn and at the end of turn **Game Effects** take place." The
  example of CR 359.3.e.16 treats "at end of turn" as a delayed triggered ability.
- Where: g07 D:127 and D:140.
- Trace:
  `[D:127.05] TORNADO WARRIOR: empowered Kha'Zix, Mutating Horror until end of turn`
  `[D:140.02] ── Ending Step ──`
  `[D:140.03] ── Expiration Step ──`
  `[D:140.04] EFFECT: disempower Kha'Zix, Mutating Horror (id=55) -> 0`
- Should have happened: a delayed ability on the chain in the Ending Step, with priority, as the engine does
  for Targon's Peak's "at the end of this turn". By code Sanction 822 sets the same flag.

### G14  A chosen target that left the board and came back is still acted on
- Confidence: CERTAIN. Batch 3.
- Text / rule: Amateur Recital "When you hold here, you may move a unit at a battlefield to its base." Temporal
  Breach "Banish a unit, then its owner plays it to the same location, ignoring its cost." CR 124: "A **Game
  Object** that changes zones to or from a **Non-Board Zone** becomes a new object for the purposes of tracking
  that object." CR 359.3.e.4: "If a target changes **Zones** to or from a **Non-Board Zone** and then returns to
  its original zone, it is no longer a legal target, because it's not treated as the same object."
- Where: g14 D:077-091 (idx=84-98), turn 6, P2's Scoring Step.
- Trace:
  `[D:078.03] TGT_PICKED: Amateur Recital: move which unit to its base = Ravenbloom Student (id=68)`
  `[D:087.02] RESOLVE: Temporal Breach (chain_item=19, ability=0, spell=1)`
  `[D:087.03] EFFECT: banish Ravenbloom Student (id=68)`
  `[D:091.02] RESOLVE: Amateur Recital (chain_item=16, ability=1, spell=0)`
- Board: after the Breach `BF Amateur Recital | P2: Ravenbloom Stu..(2M,exh)` (played back); after the Recital
  item resolved `BF Amateur Recital  [Ctrl:--]`, `BF Amateur Recital | P2: --`, the Student in P2's base. (The
  move prints no trace line.)
- Should have happened: the Recital's target is gone; the played-back Student stays and P2 keeps the battlefield.
- Why (source): Temporal Breach plays the unit back under the same object id and a stored target is checked by
  id, so every stored choice survives a leave-and-return.
- Not the same as: the checked entry "Carrion Dredger ... replays it at the same location as a new object"
  (statuses reset: true here too); D47 / D48 (where and how the Breach plays it back).

### G15  Activated abilities with a target or a mode: the whole cost is paid before the choice
- Confidence: CERTAIN on the order; no outcome changed in these games. Batch 3 (with D34).
- Rule: CR 400: "Playing or activating **Abilities** follows the same steps of playing cards." CR 403.2: "Make
  all choices required for this ability, such as targets, modes, or other relevant decisions." — step 2; CR 404.2:
  "Apply cost increases and decreases as a result of choices made in the prior step."; paying is step 4.
- Where and trace:
  - g06, Heart of the Tempest "[Action][>] Disempower me, [E]: Give a unit [Assault 2] this turn.":
    `[D:272.03] EFFECT: disempower Heart of the Tempest (id=1) -> 0`
    `[D:273.00] DECISION #296 (P1): choose a target for Heart of the Tempest [5 options]` — the list holds Vex,
    Apathetic ([Deflect]).
  - g16, Defender of Tomorrow: `[D:128.03] PAY_ADD: P1 pays [1] + 0 power: exhausted 1, recycled 0` then
    `[D:128.05] MODE_PROMPT: Defender of Tomorrow: which ability [Ready a gear/Ready 2 gear] (agent decides among 2 modes)`;
    Matriarch of War: `[D:274.03] EFFECT: disempower Matriarch of War (id=54) -> 0` /
    `[D:274.04] PAY_ADD: P2 pays [0] + 1 power (any): exhausted 0, recycled 1` /
    `[D:275.00] DECISION #283 (P2): choose a target for Matriarch of War [9 options]`.
  - g18, Bounty Hunter "[E]: Give a unit [Ganking] this turn.", all five activations: the legend is already
    `[EXHAUSTED]` on the board the target prompt is asked on (`[D:070.00] DECISION #76 (P1): choose a target for Bounty Hunter [3 options]`).
- Should have happened: mode and target first, then the total cost with any Deflect, then payment.
- Not the same as: D34 (Decree of Insight, a spell). The decks document records the same order for Soul's
  Reflection and Voidreaver as notes without an item; this is the activated-ability path as a whole.

### G16  A play-time-target spell played by Fizz, Trickster is paid for before its target is chosen
- Confidence: CERTAIN on the order; no outcome changed. Batch 3 (the effect-play path; with D32 / D57).
- Rule: CR 355.5: "If a card requires you to specifically choose one or more **Game Objects**, that choice is
  made now." (step 2; paying is step 4).
- Where: g12 D:285-288, turn 15.
- Trace:
  `[D:285.03] PAY_COST: Punch First E=0 P=2`
  `[D:288.00] DECISION #310 (P1): choose a target for Punch First [5 options]`
  `[D:288.01] TARGET (finalize): Punch First -> Recruit`
  From hand the same card goes the right way round: `[D:107.01] TARGET (play): Punch First -> Sivir, Mercenary`
  then `[D:107.02] PAY_COST: Punch First E=1 P=2`.
- It matters whenever the cost hangs on the choice: Deflect, or Sandswept Tomb's "[A] less".
- Not the same as: D32 (Fizz replays a spell with no target at all — seen again in g03 and g08).

### G17  Emperor's Divide: the units to move are chosen after RESOLVE
- Confidence: LIKELY. Batch 3 (with D24 / D41; F4 is the same card).
- Text / rule: "Move any number of friendly units at a battlefield to their base." CR 355.5 (choices are made
  as the spell is put on the chain); CR 355.15: "These choices cannot be changed after this step unless
  instructed to by a spell or ability."
- Where: g10 D:183-189, turn 12.
- Trace:
  `[D:183.03] SPELL: Emperor's Divide (id=32) targets=[Scuttle Crab(id=19)]`
  `[D:187.02] RESOLVE: Emperor's Divide (chain_item=42, ability=0, spell=1)`
  `[D:187.03] CHOICE-REQUEST: Emperor's Divide: move a unit to base, or stop [3 options] (P1)`
  `[D:188.03] EMPEROR'S DIVIDE: moved a unit to base`
- Options: the play is offered once per unit (`@ Noxian Drummer`, `@ Scuttle Crab`), two intents that do the
  same thing; after RESOLVE P1 moved Noxian Drummer and stopped. The announced unit stayed, an unannounced one
  moved, and the opponent had passed priority knowing only "targets=[Scuttle Crab]".
- Should have happened: the set of units (zero allowed) is chosen as the spell goes on the chain.
- By code: if the one named unit leaves in response the spell does nothing although other units are there.

### G18  Defender of Tomorrow: "Ready a gear" refuses a ready gear
- Confidence: CERTAIN on target validity (CR 355.9); that readying a ready gear simply does nothing is by
  analogy with CR 416.1.c and the Stun example of 424.1.a.1. Batch 4.
- Text / rule: "[1], [E]: Ready a gear." (no "exhausted"). CR 355.9: "A target is a valid choice if it meets all
  of the following requirements ..." (on the board, meets the targeting restrictions, not the ability itself).
- Where: g16 D:015-020, turn 3; the same filter at D:129, turn 11.
- Trace:
  `[D:015.03] PAY_ADD: P1 pays [1] + 0 power: exhausted 1, recycled 0`
  `[D:015.05] TGT_NOT_OFFERED: Defender of Tomorrow: ready a gear (no legal targets at resolve time)`
  `[D:018.03] EFFECT: P1 adds 1 floating Energy (pool=1)`
  `[D:020.02] RESOLVE: Defender of Tomorrow (chain_item=3, ability=1, spell=0)`
  and `[D:129.04] TGT_PROMPT: Defender of Tomorrow: ready a gear [Platewyrm Egg/Platewyrm Egg] (agent decides among 2 targets)`
  with five gear on the board, three of them ready.
- Board at D:015: `P1 BASE | Gear:  Platewyrm Egg(,rdy)` — one gear, ready, "no legal targets".
- Should have happened: the ready Egg is chosen as the ability is finalized, P1 exhausts it in response for its
  [Add], and the ability readies it — the legend's whole use. The engine is not consistent with itself:
  Matriarch of War's "Ready a unit" offers a ready unit in the same game (idx=283).

### G19  Defender of Tomorrow is offered, paid for and put on the chain with nothing to choose
- Confidence: CERTAIN. KNOWN ROOT (D67, Swift Scout), another card. Batch 3.
- Rule: CR 403.3: "If legal options are not available for an **Activated Ability,** it is not legal to activate
  it." CR 355.8: "In order to put a spell or ability on the chain, valid choices must be made for all targets."
- Where: g16 idx=6 (turn 1: two `Activate Defender of Tomorrow #936` options with no gear in play at all; that
  the second is "Ready a gear" is inferred from the option counts of idx=21, 24 and 40) and D:015 (taken, with —
  by the engine's own list — no target: the trace under G18).
- Should have happened: not offered with no gear on the board.

### G20  Public Execution is offered, paid for and put on the chain with no legal pair
- Confidence: CERTAIN. KNOWN ROOT (D8; F7 for the half-checked pair), another card and the Flow path. Batch 3.
- Text / rule: "Choose a friendly unit. Kill an enemy unit with less Might than it." CR 355.8 (above); CR
  420.2.a: "As long as a player has the resources to pay the costs associated with the card and legal choices to
  make for their cards, they may **Play** cards."
- Where: g16 D:223-226, turn 14 (played by Flow); offered in the same state at idx=210, 216, 225, 229, 231.
- Trace:
  `[D:224.03] TGT_PAIR_PICKED_A: Public Execution = Vi, Peacekeeper (id=92)`
  `[D:224.04] TGT_PAIR_NOT_OFFERED: Public Execution (no legal second targets after A picked)`
  `[D:224.05] PAY_ADD: P2 pays [5] + 2 power (any): exhausted 5, recycled 2`
  `[D:226.02] RESOLVE: Public Execution (chain_item=52, ability=0, spell=1)`
- Board: the only enemy unit has 6 Might; P2's highest unit has 6.
- Should have happened: not offered. New against D8 / F7: whether a second target exists depends on which first
  target is picked, so the gate has to test pairs.

### G21  Kennen, Keeper of Balance played from facedown stuns a unit in the enemy base
- Confidence: CERTAIN. KNOWN ROOT (D70 Windsinger, D69 Sprite Call), another card; here the illegal option was
  taken. Batch 4.
- Text / rule: "When you play me or I attack, you may pay [2] to [Stun] a unit." CR 811.1.d.3: "If a play effect
  of a hidden permanent chooses any targets, those targets must be chosen from among options at the battlefield
  it was played to, unless the ability explicitly restricts targeting in a way that makes this impossible."
- Where: g04 D:104-108 (idx=116-118), turn 10; Kennen was hidden at Seat of Power.
- Trace:
  `[D:104.02] INTENT: PlayReaction by P2`
  `[D:105.04] TGT_PROMPT: Kennen, Keeper of Balance: stun [Rengar, Trophy Hunter/Inferna/Vi, Peacekeeper/Gem Jammer/Honest Broker/Rengar, Unseen/Kennen, Keeper of Balance] (agent decides among 7 targets)`
  `[D:106.03] TGT_PICKED: Kennen, Keeper of Balance: stun = Inferna (id=16)`
  `[D:108.03] EFFECT: stun Inferna (id=16)`
- Should have happened: only the three units at Seat of Power are offered; four of the seven options, and the
  one taken, are not legal. By code only Tornado Warrior and Evelynn, Entrancing know they were played from
  facedown: every other [Hidden] permanent's play ability with a target is suspect.

### G22  Facedown units are offered and played onto Rockfall Path
- Confidence: CERTAIN that the unit may not be played there. Whether it may be played anywhere else instead is
  open (existing judge question 3 asks it for Guards!). Batch 4 (with D47, D62).
- Text / rule: Rockfall Path "Units can't be played here." CR 811.1.d.1: "A hidden permanent must be played to
  that battlefield." CR 054.1: cards that forbid supersede cards that permit.
- Where: g08 (1) D:054 (idx=58), Tornado Warrior hidden at D:034, also offered at idx=46 and 52; (2) D:213
  (idx=234), Tideturner hidden at D:209.
- Trace:
  `[D:034.04] HIDE: Tornado Warrior at BF#0 by P2`
  `[D:054.04] INTENT: PlayActionCard by P2`
  `[D:054.05] TRIGGER: Tornado Warrior (P2) fires ability`
  `[D:209.04] HIDE: Tideturner at BF#0 by P1`
  `[D:213.02] INTENT: PlayReaction by P1`
- Board: before, `BF Rockfall Path | [HIDDEN: P2 — facedown: Tornado Warrior]`; after,
  `BF Rockfall Path | P2: Tornado Warrior(3M,exh,Hdn)  Steel Paws(0M,rdy,Dfl,DEF)`. From hand the engine never
  offers a unit `-> Rockfall Path` in this game.
- Consequence: a third body in a combat; a second unit holding Rockfall Path.
- Not the same as: D47 (Temporal Breach) and D62 (Thrill of the Hunt) are effects that play a unit; this is the
  plain play of a hidden unit. The decks document's scan ("0 `Play <unit> -> Rockfall Path` options") could not
  see it: a facedown play is rendered without a destination.

### G23  A Brush that replaced Vilemaw's Lair keeps "Units can't move from here to base"
- Confidence: LIKELY (CERTAIN that the engine withholds the move; whether the rules give it is J3). Batch 4.
- Text / rule: Brush "Bird, Cat, Dog, Poro, and Ivern units here have +1 [M]. / When you score here, you may
  replace this with the battlefield it replaced." CR 439.5: "The card or token that is **Replaced** is placed in
  Banishment." Against: CR 439.1 "... while inheriting all effects or statuses of the game object it replaced".
- Where: g02, replacement at D:122; option lists idx=131, 147, 149, 164, 165, 167, 168.
- Trace:
  `[D:122.03] BF_REPLACE: BF#1 replaced by 'Brush' token (card=109)`
  `[D:154.00] DECISION #164 (P2): 9 legal actions`
- Options, idx=164: three ready units at the Brush (Vi, Peacekeeper; Astral Heron; Trusty Ramhound) and no
  `Move ... -> Base` for any of them, while the ready unit at the other battlefield has its move to base.
- Why (source): the battlefield slot's flags (`blocks_move_to_base`, and by code `blocks_unit_play`,
  `min_turn_to_score`) are copied from the card at setup and not touched by the replacement: a Brush over
  Rockfall Path or Forgotten Monument would keep those too.
- Not the same as: F3 (the same function skipping `leftBoard` for the replaced card; checked build only).

### G24  Green Father: the "may" is withheld when the battlefield held is already a Brush
- Confidence: LIKELY, low impact (J4). Batch 4.
- Text / rule: "When you conquer or hold, you may exhaust me to replace that battlefield with a Brush battlefield
  token." CR 439.6: "If a token is **Replaced** it will stop existing once it begins its occupancy in
  **Banishment.**" (a token can be replaced).
- Where: g02 D:148, the legend ready.
- Trace:
  `[D:148.03] SCORE: P2 +1 via Hold BF#1 -> 7`
  `[D:148.07] MAY_NOT_OFFERED: Green Father: exhaust to replace that battlefield with Brush? (precondition failed at prompt time)`
- The class refuses on purpose. Nothing differs on the board except the legend's exhaustion.

### G25  Astral Heron: with the first card of the turn countered, no card of that turn triggers it
- Confidence: QUESTION (J6). Batch 4 (with D18 / D19).
- Where: g08 D:151-164, turn 13: Rebuke (first card) is countered by its controller's own Abandon; then a unit.
- Trace:
  `[D:151.03] MOVE: Astral Heron (id=84) -> BF#1`
  `[D:162.03] COUNTER: Rebuke countered by Abandon -> hand`
  `[D:164.04] PAY_COST: Pyke, Returned E=3 P=0`
  No `TRIGGER: Astral Heron` in between: the engine tests "this card is play number 1 of the turn" as the card
  resolves, and the first card never resolved.

### G26  Ravenbloom Student played back by Temporal Breach triggers on that same Temporal Breach
- Confidence: QUESTION (J7). Batch 1.
- Where: g14 D:087-089.
- Trace:
  `[D:087.03] EFFECT: banish Ravenbloom Student (id=68)`
  `[D:087.04] TRIGGER: Ravenbloom Student (P2) fires ability`
  `[D:089.03] EFFECT: give Ravenbloom Student 1 [M] this turn -> 3M`

## 5. Known items seen again
42 of the 93 known items were seen, 881 sightings in all. A sighting is counted as the reader counted it; where a reader gave no number, the places the reader cited are counted, so the totals are a lower bound for the engine-wide items (D1, D2, D4, D9, D13, D40), which several readers counted only up to the first few. No re-analysis: the item's own description stands. Not seen in any of the 19 games: every item not in the table (F1-F9, V1, A1-A2 need the fuzz or a validator call; D12 is the validator).

| id | sightings | games | where (game(sightings)) | known item |
|---|---|---|---|---|
| D1 | 109 | 18 | g01(6) g03(6) g04(12) g05(8) g06(3) g07(2) g08(11) g09(6) g10(6) g11(6) g12(5) g13(9) g14(7) g15(4) g16(7) g17(5) g18(2) g19(4) | Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Acc... |
| D2 | 75 | 12 | g01(8) g03(12) g04(1) g05(3) g06(11) g10(6) g11(5) g12(6) g13(3) g14(3) g16(16) g18(1) | Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it re... |
| D3 | 20 | 11 | g01(3) g04(1) g05(3) g06(1) g07(4) g08(1) g10(1) g11(1) g14(2) g18(2) g19(1) | Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is cho... |
| D4 | 151 | 19 | g01(13) g02(13) g03(5) g04(2) g05(2) g06(17) g07(2) g08(6) g09(2) g10(2) g11(3) g12(27) g13(6) g14(5) g15(4) g16(4) g17(13) g18(4) g19(21) | Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both player... |
| D5 | 23 | 10 | g01(2) g04(1) g05(1) g06(1) g09(4) g11(5) g12(1) g16(3) g18(3) g19(2) | Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, b... |
| D8 | 17 | 5 | g01(1) g03(4) g05(5) g06(5) g08(2) | Star-Crossed, Switcheroo, Facebreaker, Defiant Dance: offered, paid for and put on the chain without a legal choice for each target |
| D9 | 107 | 19 | g01(4) g02(2) g03(5) g04(8) g05(5) g06(3) g07(3) g08(5) g09(4) g10(6) g11(5) g12(9) g13(6) g14(4) g15(3) g16(12) g17(10) g18(7) g19(6) | Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled |
| D10 | 15 | 6 | g05(2) g10(2) g11(1) g12(4) g17(3) g18(3) | Equipment whose bearer dies is not recalled by the next cleanup: it lies unattached at the battlefield through priority windows, once through a Mai... |
| D11 | 35 | 11 | g02(3) g03(3) g04(3) g07(4) g08(5) g10(4) g11(2) g13(3) g17(3) g18(1) g19(4) | Hidden information in log lines whose audience is both players |
| D13 | 49 | 16 | g01(2) g02(2) g03(4) g04(4) g05(4) g08(3) g09(3) g10(5) g11(5) g12(3) g13(3) g14(3) g15(1) g17(1) g18(3) g19(3) | Combat damage: the attacker's damage is dealt before the defender has assigned |
| D14 | 5 | 4 | g04(1) g09(1) g11(1) g15(2) | Long Sword, Sterak's Gage ([Quick-Draw]): the attach is done inline as the gear lands, not as a triggered ability on the chain |
| D15 | 21 | 9 | g01(2) g02(2) g04(2) g05(2) g10(2) g12(1) g13(3) g17(4) g18(3) | Long Sword, Sterak's Gage ([Equip]): the Equip ability attaches at activation — no chain item, no priority |
| D18 | 9 | 2 | g02(5) g08(4) | Astral Heron: the discount is kept for a later turn |
| D19 | 3 | 1 | g08(3) | Astral Heron: the ability is put on the chain while the Heron is not at a battlefield |
| D20 | 4 | 3 | g03(1) g09(2) g11(1) | Falling Star: kills between its two instructions, so a death replaced after the first hit is died again on the second |
| D21 | 20 | 7 | g01(3) g02(2) g03(3) g07(4) g08(4) g10(1) g17(3) | Counter spells (Defy, Not So Fast, Hard Bargain, Riposte, Repulse, Rebuttal, Abandon, Wind Wall): the spell to counter is never chosen — only the t... |
| D22 | 2 | 1 | g17(2) | Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization |
| D23 | 19 | 4 | g05(2) g10(3) g12(11) g17(3) | Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token mo... |
| D25 | 9 | 4 | g04(3) g07(2) g08(1) g13(3) | Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its contro... |
| D27 | 6 | 2 | g08(3) g12(3) | Pyke, Returned / Edge of Night: the trigger's condition is tested at resolution, so the ability goes on the chain when it should not trigger |
| D28 | 8 | 3 | g04(3) g15(3) g17(2) | Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose) |
| D31 | 8 | 5 | g01(1) g03(3) g06(1) g10(2) g11(1) | Scorn of the Moon activated with Focus passes Focus to the opponent (outcome of K2) |
| D32 | 6 | 2 | g03(4) g08(2) | Fizz, Trickster: a spell whose target is taken from the play intent is replayed with no target (Stupefy, Eclipse, Smoke Screen, Turn to Dust) |
| D33 | 10 | 3 | g05(3) g06(5) g07(2) | Vex, Apathetic: the ability is put on the chain while Vex is in a base; "while I'm at a battlefield" is tested at resolution only |
| D35 | 2 | 1 | g02(2) | Grand Duelist / Fiora, Worthy: "becomes [Mighty]" is only noticed at the engine's top-level cleanups, so the triggers arrive late |
| D36 | 3 | 1 | g02(3) | Sett, Brawler + Grand Duelist / Fiora, Worthy: a unit that stops being Mighty and becomes Mighty again inside one chain triggers nothing |
| D37 | 8 | 4 | g04(1) g05(1) g14(3) g18(3) | Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state |
| D38 | 5 | 2 | g02(2) g11(3) | Divining Shells: "Kill this" is paid when the ability resolves |
| D39 | 4 | 1 | g12(4) | Sacrifice + Rift Herald: an ability triggered by paying a spell's additional cost is put UNDER that spell |
| D40 | 48 | 16 | g01(3) g02(5) g03(2) g04(4) g05(2) g06(5) g08(1) g09(2) g10(2) g11(2) g12(1) g13(5) g14(3) g17(3) g18(2) g19(6) | Grand Duelist + Fiora, Worthy (and Akshan's two play triggers): the controller is never asked in which order simultaneous triggers go on the chain |
| D41 | 3 | 1 | g05(3) | Repulse, Forge of the Future: choices the rules place at finalization are made after RESOLVE (K3 kind) |
| D44 | 17 | 4 | g11(2) g12(11) g13(3) g17(1) | A unit with lethal damage stays on the board until the chain is empty: no cleanup between the items of a chain (Wages of Pain, Deadly Flourish, Sin... |
| D45 | 4 | 2 | g05(2) g16(2) | Bellows Breath: the Repeat's choices ignore "at the same location", are forced to the first execution's count, and are not asked at all when the fi... |
| D46 | 1 | 1 | g05(1) | The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named |
| D51 | 13 | 5 | g08(4) g09(2) g11(2) g13(3) g18(2) | Kha'Zix, Mutating Horror (also Evelynn, Entrancing; Tornado Warrior): the condition printed with the trigger is tested when the ability resolves, n... |
| D53 | 16 | 6 | g05(1) g11(5) g14(3) g15(1) g17(5) g19(1) | [Temporary] (Deceiver / Mirror Image Reflections; Dusk Rose Lab): the kill is done directly at the start of the Beginning Step, never as a triggere... |
| D54 | 5 | 2 | g07(3) g08(2) | Tornado Warrior (Evelynn, Entrancing by code): the leading "you may" and the target are decided after RESOLVE |
| D58 | 4 | 1 | g09(4) | Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate |
| D66 | 1 | 1 | g12(1) | Herald of the Arcane: the Recruit always goes to the base; no location is offered |
| D67 | 4 | 1 | g19(4) | Swift Scout, Guerilla Warfare: the objects are chosen after RESOLVE; Swift Scout can be activated with nothing to choose |
| D72 | 9 | 4 | g02(2) g10(1) g13(5) g19(1) | Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold |
| D76 | 3 | 2 | g05(2) g14(1) | Wuju Bladesman - Starter: the +2 [M] is missing whenever nothing resolves between "COMBAT:" and the damage step |

Sightings with an aspect the known item's text does not cover (each from the reader's notes of the game named;
trace prefixes are of that game):

- **D2, Gold's cost.** Gold's "Kill this" is paid when the ability RESOLVES, after the Add — the D38 pattern
  (Divining Shells) on Gold: g04 D:183, g05 D:197, g11 D:262, g18 D:109, g03. Through both priority decisions
  the Gold is still on the board, exhausted, under its own chain item.
- **D2, more cards.** Fire Below the Mountain (g01, 8 activations; the opponent was offered Flash on top of
  one), Hand of Noxus (g10, 6), Platewyrm Egg (g16, 16), Blue Sentinel's delayed "[Add] [A]" (g14), Lux,
  Crownguard (g11), Daughter of the Void and Gold (g03, g12, g13).
- **D2, a changed outcome.** g06 D:049-069: Scorn of the Moon's [1] was still on the chain when Hard Bargain
  resolved against its controller's Eclipse — `HARD BARGAIN: countered Eclipse (P2 can't pay [2] to save it)` at
  D:058 — and arrived at D:069. With the Add done at once P2 held [2] and had to be asked.
- **D31, more cards.** Focus passes after an [Add] activated with Focus for Daughter of the Void (g03 D:213),
  Fire Below the Mountain (g01 D:142), Hand of Noxus (g10 D:062, a non-combat showdown: P1, who had already
  passed, got Focus back and played Hidden Blade) and Lux, Crownguard (g11 D:210).
- **D5, the Flow replay.** g06 idx=224: the Lightning Rush offered at Focus in a combat is the copy in the TRASH
  (the hand held one unit): a Flow play of a spell with neither [Action] nor [Reaction] is offered at Focus too.
- **D8, another reason for no pair.** g01 idx=284 / 289: Switcheroo offered when the only two units sharing a
  battlefield include Akali, Silent, who cannot be chosen; Defiant Dance paid for on turn 0 with no unit on the
  board (g01 D:005).
- **D10, the bearer banished.** g11 D:086-090: Spinning Axe left unattached when its bearer was banished as a
  cost, recalled four priority decisions later.
- **D18, across to the opponent's turn.** g08 D:219 and D:231: the Heron's unused discount made on one player's
  turn is spent on the other player's turn, both ways — `PAY_COST: Switcheroo E=0 P=0` in the opponent's
  showdown. g02 D:123: Ivern, Nurturer ([5] + 1 Power) for [3] two turns after the trigger.
- **D21, played out.** The option `Play Defy` names no target and can only take the top item: a player countered
  their own spell in g02 (D:169, Defy on its own Hidden Blade with the opponent's Mobilize under it), g07
  (D:104, Defy on its own Hard Bargain with the opponent's Void Assault under it), g01 and g17 (twice).
- **D23 / D44, more cards.** Sivir, Mercenary's +2 [M] arrives only when the chain is empty (g12, 11 sightings);
  Purifier's "Your Equipment each give [Assault]" is missing at the first Focus decision of a combat (g05
  idx=194: Kai'Sa 8M, 10M after something resolved).
- **D27, "Once each turn".** Pyke, Returned puts two items on the chain for two deaths of one damage step, while
  in a base: g08 D:270, g12 D:305.
- **D32, more cards.** Fizz, Trickster replays Cleave, Void Seeker and Hard Bargain with no target and no effect
  line (g03 D:154, D:232, D:317; g06 D:113).
- **D39, a base cost.** g12: Battle Mistress triggers off a rune recycled for a spell's own Power cost and is
  put under that spell (4 sightings, D:070, D:109, D:150, D:286).
- **D40, the turn player's trigger.** g12 D:305: the turn player's Deathknell is placed between the other
  player's three simultaneous triggers, not first (CR 383.3.d.1).
- **D3 / D41, another card.** Relentless Pursuit asks the destination, its "you may attach" and the Equipment
  after RESOLVE (g05 D:115, D:175, D:235), and the ability it grants asks its "may" at resolution (g05 D:119).
- **D53, more cards.** Sprite Fountain (gear) and its Sprite, Spinning Axe: [Temporary] kill done directly in
  the Beginning Step (g11, g14, g17, g19).
- **D76, a changed outcome.** g14 D:066: Pit Rookie defends alone without the +2 and dies to 3 damage; with it
  both sides survive and the attacker does not conquer.
- **D28, played.** Deathgrip with a single friendly unit was not only offered but played in g04 (D:209-213), g15
  and g17 (D:149-153): the unit is killed, nothing is pumped, a card is drawn.
- **D61, the lag seen.** g09 D:185: Inferna played with [Ambush] into her side's attack shows no attacker
  designation while the chain resolves and has it (with the Assault) after the chain is empty.
- **Judge question 1 (Targon's Peak, "ready up to 2 runes").** The opponent's runes were not only offered but
  readied: g01 D:088 and D:258-259 (P2 readied P1's runes in P2's Ending Step), g16 D:302. Own and opposing
  runes are mixed in one list in object-id order.
- **Judge question Q2 of the decks document (Rek'Sai, Breacher and tokens).** Occurred twice in g09 (D:127 Bird,
  D:137 Recruit): no Accelerate offered for a token.

**Seen again, game by game (known id, decision).**

The readers' own lists, unedited: one entry per known item with the trace prefixes (`D:`) or decision indices (`idx=`) of the game of the heading. "N1", "N2" in these lists are the reader's own numbers for what section 4 calls G1-G26.

### g01 irelia vs ornn — seen again

- D1: 6 sightings — D:018.02 (Poro Snax), D:032.02 (Ornn, Blacksmith), D:081.02 (Riven, Shattered) (first three; also D:177.02, D:243.02, D:264.02)
- D2: 8 sightings — D:011-013, D:019-021, D:039-041 (also D:082-084, D:130-132, D:142-144, D:205-207, D:224-226). New aspect: the card is Fire Below the Mountain (legend, "[E]: [Reaction] — [Add] [A]. Use only to play gear or use gear abilities"), not in D2's list; every activation is `RESOLVE: Fire Below the Mountain (chain_item=..)` after both players passed, and at idx=151 the opponent was offered `Play Flash #309[S]` on top of it.
- D3: 3 sightings — D:057.03 (Ride the Wind), D:236.03 (Charm), D:253.03 (Charm)
- D4: 13 sightings — D:053.03-055.02 (Blade Dancer declined), D:058.12-064.02 (Blade Dancer, Adaptatron, Veiled Temple, all "MAY_NOT_OFFERED", three windows each side), D:081.03-086.02 (Riven's Weaponmaster with no Equipment) (first three; also D:124.11/136.02, D:178.03-180.02, D:203.03-209.02, D:211.12-215.02 twice, D:238.03-240.02, D:243.02-245.02, D:264.04-266.02)
- D5: 2 played, 1 more offered — D:231.05 (idx=244, P2 plays Charm with Focus in the combat at Veiled Temple), idx=252 (offered), D:248.05 (idx=266, played in the combat at Targon's Peak). In the two non-combat showdowns where P2 held Charm (idx=130, idx=149) it was correctly not offered.
- D8: Defiant Dance D:005.05 — played and paid for on turn 0 with no unit on the board. Switcheroo offered at idx=284 and idx=289 (not played). New aspect at idx=284/289: the only two units sharing a battlefield are Akali, Silent and Patched Porobot at Veiled Temple, and Akali "can't be chosen by enemy spells and abilities unless I'm in combat" (Targon's Peak holds Aphelios alone), so there is no legal pair at all.
- D9: 4 sightings — D:036.03, D:217.03, D:229.04 (first three; D:171.03 paid from the pool, no rune)
- D13: 2 sightings — D:237.06-08, D:254.07-09
- D15: 2 sightings — D:171.04 (Guardian Angel), D:217.04 (Forgefire Cape)
- D21: D:054 (idx=54) — P1 holds Defy and can pay it, Ride the Wind is on the chain under the Blade Dancer ability: only Pass is offered; Defy appears at idx=56 once Ride the Wind is the top item. D:156-160 — P2's second Defy is never asked what it counters and takes the top item, P2's own first Defy (`[D:160.03] COUNTER: Defy countered by Defy -> trash`), so Flash resolves.
- D31: D:142.05-144.03 — P2 activates Fire Below the Mountain with Focus in the showdown at Veiled Temple (idx=149); the next showdown decision (idx=152) is P1's `[0] Pass Focus [1] Play Flash #309[S]`. New aspect: a second card (D31 names Scorn of the Moon only).
- D40: 3 sightings — D:058.08-10 (three conquer triggers), D:124.07-09, D:211.09-10
- Judge question 1 of docs/judge-questions-2026-10.md (Targon's Peak "ready up to 2 runes": opponent's runes; listed "ok (opponent's runes offered: judge ruling)" in the findings doc, not one of the 93): offered at idx=92, 93, 144, 145, 280, 281. New aspect: here they were not only offered but taken — P2 readied P1's Calm Rune obj=42 at D:088 (`[D:091.03] TARGON'S PEAK: readied 1 runes at end of turn`; P1's line goes from `rdy:1G, exh:2G 3P` to `rdy:2G, exh:1G 3P`), and at D:258-259 P2 chose P1's obj=47 and obj=48 (`[D:261.03] TARGON'S PEAK: readied 2 runes at end of turn`; P1 goes to `Runes:11/11+1 (rdy:6G 5P)` in P2's Ending Step). The prompt lists only exhausted runes, both players' mixed, own runes not first.

### g02 fiora vs ivern — seen again

- D1: not observable here as an order defect (6 play triggers — Shepherd's Heirloom D:027.02, Sett D:068.02, Astral Heron D:099.02, Ivern D:126.03, Divining Shells [Vision] D:174.02, Poppy D:187.02 — none with a finalize-time prompt, no Accelerate paid). D1 (R4-13), Rampage's optional additional cost asked after the Energy is paid: D:105.04-109.01 (`[D:109.00] DECISION #117 (P1): pay additional cost for Rampage? [decline|pay] [2 options]` after three `EXHAUST_RUNE`).
- D4: 13 sightings — D:020.03, D:031.03, D:046.13 (first three); by card: Green Father declined D:020.03, D:031.03, D:089.03, D:146.03, not offered D:141.09, D:148.07; Fiora, Worthy not offered D:046.13, declined D:071.03; Grand Duelist declined D:047.03, D:190.03, not offered D:082.06; Brush declined D:149.03; Azir, Sovereign with no token to move D:052.05-054.02. In the D:031 window (P2's Scoring Step, the declined Green Father the only item) P2 played Discipline (D:032.01) and P1 answered with Riposte (D:037.01) — a known consequence (D4 "a Reaction was played while the declined item was on the chain").
- D9: 2 payments with a rune picked by the engine — D:082.04 (Fiora, Worthy's [Y]), D:154.05 (Hide [A]); plus two zero-cost `PAY_ADD` lines D:081.03, D:193.03.
- D11: (a) D:154.01 `CHOSE: [P2] HideCard Hidden Blade bf=1`; (b) D:003.01, D:004.01 mulligan `CHOSE` lines naming the cards.
- D13: 2 sightings — D:054.04-054.06 (board of idx=62 shows `Fiora, Worthy(5M,4dmg,exh,DEF)` before P1 assigns), D:181.07-181.10.
- D15: 2 sightings, another card (Shepherd's Heirloom, "[Equip] — Spend 1 XP") — D:081.05, D:193.05: `ATTACH` directly after the intent, no chain item, no priority.
- D18: D:123.04 — `PAY_COST: Ivern, Nurturer E=3 P=0` on turn 8 (printed [5] + 1 Power); the discount is from the Heron trigger resolved on turn 6 (D:101.03), after which P2 played no card that turn. Ivern is also the first card of turn 8, so it both used the old discount and triggered a new one (D:126.02, resolved D:138.03, used by Affectionate Poro D:139.04 `E=1`).
- D21: 2 sightings — Riposte D:038 (only the friendly unit is asked); Defy D:165-169: two spells on the chain (`[top] Hidden Blade (P2)  [bot] Mobilize (P1)`), no choice offered, `[D:169.03] COUNTER: Hidden Blade countered by Defy -> trash` — P2's Defy countered P2's own Hidden Blade (an outcome of the missing choice; Mobilize, [2], was a legal choice too).
- D35: 2 sightings — D:044.04 -> D:046.03 (shape (a): Fiora made Mighty by Riposte inside the chain; the triggers appear only after the next item, Green Father, has resolved) and D:189.05 (after `RESOLVE: Poppy, Paragon`). Aspect of the first: the two pending items (`TRIGGER` D:046.03-046.04) are carried through `── Channel Phase ──` (D:046.05, two runes channeled), `── Draw Phase ──` (D:046.08, a card drawn) and `── Main Phase ──` (D:046.11) and finalized only at D:046.12-047 — like D35 (c), but from the opponent's Scoring Step into their Main Phase.
- D36: D:078.04-080.04 — Sett 5M -> 4M (buff spent as cost) -> 8M, `[D:080.03] EFFECT: give Sett, Brawler 4 [M] this turn -> 8M`, next line `[D:081.00] DECISION #89 (P1): 3 legal actions`, no `TRIGGER`. Fiora, Worthy was on the board (base, exhausted) and P1 had Order runes to recycle; Grand Duelist was exhausted.
- D38: D:181.03 (`EFFECT: kill Divining Shells (id=9)` at RESOLVE; activated at D:178.05).
- D40: 5 sightings — D:046.03-04, D:070.05-06, D:081.06-07 (first three; Fiora, Worthy + Grand Duelist); also Astral Heron + Ivern D:126.02-03, Green Father + Brush D:148.04-05.
- D72: D:145.08 (Hold BF#0), Green Father's item resolved D:148.02, then `[D:148.03] SCORE: P2 +1 via Hold BF#1 -> 7`.
- Reader 4's Q1 (a unit entering with 5+ [M] "becomes Mighty"): Poppy, Paragon D:187.02-189.05 (Grand Duelist triggers).
- Not met in this game (looked for): D5 (no spell without [Action]/[Reaction] affordable at a combat Focus), D6 (idx=41: Rampage in hand with 3 ready runes is not offered at chain priority — correct), D25 (the hidden card's controller had no Main Phase after the turn it was hidden).

### g03 pyke vs kaisa — seen again

- D1: 10 play triggers queued on a unit play — D:023.02, D:043.02, D:141.02 (first three; also 150.02, 208.02,
  219.02, 228.02, 267.04, 299.02, 313.02). The unit is on the board ready during its own trigger's prompts and
  exhausted after: Bewitching Spirit idx=149/150, Fizz idx=158/160, 245/247, 348/350, Tideturner idx=292/293,
  334/335. Accelerate asked after the Energy is paid: D:133.00 (Kai'Sa, Survivor, declined). Optional cost /
  Repeat asked after the base cost: D:043.00 (Pyke), D:053.00 (Bellows Breath), D:192.00 (Hard Bargain).
- D2: 12 sightings (8 Daughter of the Void, 4 Gold) — D:010.02, D:032.02, D:056.02 (first three). Opponent
  acting on top of an [Add]: D:118.01 (P2 activates Daughter over P1's Gold, chain of four at idx=126); own
  Reaction over own [Add]: D:027.03 (Stupefy over Daughter). New aspect for Gold: its "Kill this" cost is paid
  as the ability resolves, not at activation (the D38 pattern on another card) — idx=58 board `P2 BASE | Gear:
  Gold(,exh)` with `[top] Gold (P2)` on the chain; `[D:056.05] EFFECT: kill Gold (id=109)` comes after
  `[D:056.02] RESOLVE: Gold (chain_item=15, ability=1, spell=0)`. Same at D:122.05, D:126.05, D:243.05.
- D4: 2 declined Tideturner abilities stay on the chain — D:268.03 -> D:274.02 (P2 activates Daughter of the
  Void in that window, D:270.01), D:300.03 -> D:302.02.
- D8: Star-Crossed offered with no friendly unit on P1's board, in the Main Phase (idx=117) and at chain
  priority (idx=118); played: `[D:111.05] TGT_PAIR_NOT_OFFERED: Star-Crossed (no legal first targets at resolve time)`, `[D:111.06] PAY_COST: Star-Crossed E=3 P=1`, resolves empty at D:124.02. D8 (R3-7) again: Fizz replays
  Hard Bargain with no spell to counter — `[D:317.04] FIZZ: played Hard Bargain from trash (energy<=3, ignoring energy), then recycled it`, `[D:319.03] HARD BARGAIN: nothing on chain to counter — no-op` (deck 25 -> 26).
- D9: 5 sightings — D:078.03, D:192.01, D:198.03 (first three; also D:260.05 the Hide cost: a Fury rune was
  recycled for the [A] without a prompt, D:294.03).
- D11: D:260.01 `CHOSE: [P1] HideCard Tideturner bf=2` (kind a); D:003.01, D:004.01 mulligan `CHOSE` lines
  naming the cards (kind b). Audience not visible in the log.
- D13: 4 combats, the first side's damage is on the board before the other side assigns — D:196.04-196.07
  (idx=207 shows `Noxus Hopeful(4M,2dmg,exh,DEF)` at P2's assignment), D:261.06-261.09, D:274.04-274.07,
  D:289.06-289.09.
- D20: D:089.03-089.07 — Falling Star kills Lecturing Yordle between its two instructions.
- D21: D:189.03 `SPELL: Hard Bargain (id=32) targets=[]`; Repeat paid D:192.02; `[D:194.03] HARD BARGAIN: countered Void Seeker (P1 can't pay [2] to save it)` then `[D:194.05] HARD BARGAIN: nothing on chain to counter — no-op` (the Repeat's spell never chosen).
- D31: new card — Daughter of the Void (D31 names Scorn of the Moon). P2 activates it with Focus and Focus goes
  to P1: `[D:213.03] INTENT: ActivateActionAbility by P2`, `[D:215.03] DAUGHTER OF THE VOID: [E] -> [Add] [A]`,
  then idx=230 `P1 (forced): Pass Focus`, idx=231 `P2 (forced): Pass Focus`. Also D:026.05 -> idx=34 (a Stupefy
  was played as a Reaction in between, not as the Focus action).
- D32: new cards — Cleave and Void Seeker. `[D:154.03] PAY_COST: Cleave E=0 P=0`, `[D:156.02] RESOLVE: Cleave (chain_item=39, ability=0, spell=1)` with no target prompt and no effect line (four units on the board).
  `[D:232.03] PAY_COST: Void Seeker E=0 P=1`, `[D:235.02] RESOLVE: Void Seeker (chain_item=55, ability=0, spell=1)`, `[D:235.03] EFFECT: P1 draws 1 (deck=28)` — no "deal 4" although Plundering Poro and Kai'Sa,
  Survivor were at Zaun Warrens; the card was drawn anyway.
- D40: D:032.06-032.07 (Plundering Poro + Zaun Warrens, P2), D:068.04-068.05 (Traveling Merchant + Treasure
  Hunter, P1) — no ordering prompt.
- Not seen although the cards / situations were there: D5 (no non-[Action] spell offered at Focus: Falling Star
  withheld at idx=77, 78, 83), D6, D25 (the facedown Tideturner was never in its controller's own Open State
  after the turn it was hidden), D7, D44, D72 (two holds at D:293.08-293.09, no hold abilities to interleave).

### g04 rengar vs vi — seen again

- D1: 12 sightings — D:016.02-017.03 (Gem Jammer's trigger finalized with the unit `@Chain`, its only target), D:026.02-027.00 (Inferna `@Chain` in Pridestalker's list), D:090.00 (Accelerate for Rengar, Unseen asked after the unit is shown on the board; again Kai'Sa D:219.00). Also Rampage's optional additional cost asked after the base cost was paid (D1, R4-13 kind): D:200.04 `PAY_COST`, three `EXHAUST_RUNE`, then `[D:204.00] DECISION #229 (P1): pay additional cost for Rampage? [decline|pay] [2 options]`.
- D2: 1 sighting — D:181.02-183.06 (Gold: activation, a priority decision for each player, then `RESOLVE: Gold`). New aspect not in D2's text: the cost "Kill this" is paid at resolution, after the Add (`[D:183.03] EFFECT: P2 adds 1 floating [A] (pool=1)` then `[D:183.05] EFFECT: kill Gold (id=109)`); through both priority decisions the board shows `P2 BASE | Gear:  Gold(,exh)  Long Sword(,rdy,Rxn)` (idx=200). Same pattern as D38 (Divining Shells).
- D3: D:110.02-111.03 — Thrill of the Hunt: `MODE_PROMPT: Thrill: which battlefield [Seat of Power/Forge of the Fluft]` after `RESOLVE`.
- D4: 2 sightings — D:033.05-038.02 (Grim Apothecary: `MAY_NOT_OFFERED ... (precondition failed at prompt time)`, the item stays under Pridestalker through four priority decisions and "resolves"), D:190.03-192.02 (Piltover Enforcer declined, then a priority decision each, then `RESOLVE`).
- D5: 1 sighting — idx=219 (D:197.02 combat at Forge of the Fluft, P1 attacking): `[0] Pass Focus  [1] Play Rampage #870[S]`; Rampage has neither [Action] nor [Reaction]. Not taken.
- D9: 8 sightings — D:081.05 (Hide cost), D:097.04 (Deflect), D:106.04 (Kennen's [2]); also D:121.04, D:204.01, D:208.03, D:219.02, D:225.03. In all eight the runes the engine took were of a legal Domain.
- D11: (a) `[D:081.01] CHOSE: [P2] HideCard Kennen, Keeper of Balance bf=0`; (b) `[D:003.01] CHOSE: [P1] Mulligan 1 [Nidalee, Cat Form]`, `[D:004.01] CHOSE: [P2] Mulligan 1 [B.F. Sword]`.
- D13: 4 sightings (every combat in which both sides assigned): D:071.05-071.08 (idx=78 -> 79: board at P2's assignment already `Honest Broker(7M,3dmg,exh,Dkn,DEF)`), D:072.06-072.09, D:173.04-173.08, D:197.04-197.08.
- D14: D:044.01-044.02 — Long Sword `QUICK_DRAW: auto-attaching`, no chain item.
- D15: 2 sightings — D:208.02-208.04, D:225.02-225.04 (Equip: `PAY_ADD` then `ATTACH`, next line is the next Main Phase decision).
- D25: 3 sightings — idx=96, 103, 105 (D:084.00, D:091.00, D:093.00; turn 9, P2's own Main Phase, `BF Seat of Power | [HIDDEN: P2 — facedown: Kennen, Keeper of Balance]` hidden on turn 7): no "Play Kennen" among the options; it is offered at idx=116 (closed state, turn 10).
- D28: D:208.00 (idx=233) and D:209.00 (idx=234): `Play Deathgrip #484[S] @ Kennen, Keeper of Balance #922` with Kennen P2's only unit; played, D:213.03-213.06: Kennen killed, no Might given, draw 1.
- D37: idx=57 (D:054.00), turn 6: P1 has 7 ready runes, `BF Seat of Power | P2: Honest Broker(2M,exh,Dkn)  Gem Jammer(4M,exh,EQ:Long S..)` and no unit of his own there; the only Rengar option is `[8] Play Rengar, Trophy Hunter #682(6M) -> Base`. (Also idx=24, turn 4: no enemy unit at a battlefield then, so not a sighting.)
- D40: 4 sightings — D:033.02-033.03 (Grim Apothecary + Pridestalker), D:120.02-120.03 (First Mate + Pridestalker), D:163.02-163.04 (Pit Rookie + Darius, Trifarian + Pridestalker); also D:226.07-226.08 (Kai'Sa, Survivor + Seat of Power).
- Not seen in this game: D6, D7, D21, D35, D44, D72 (one battlefield held per Scoring Step throughout), D63 (the only reaction windows P1 had with Rengar in the Champion Zone and enemy units at a battlefield, D:050-052, he had no ready rune).

### g05 lucian vs ezreal — seen again

- D1: 8 sightings (play ability fired, then Accelerate asked; or Accelerate asked after the cost) — D:155.02 then D:156.00 (Thousand-Tailed Watcher), D:106.00 (Kai'Sa), D:222.00 (Legion Rearguard); also Fizz D:036.02 / D:257.02 (board shows him `rdy` during finalization), Lucian D:026.02, Ezreal D:204.02, The List D:209.02.
- D2: Gold D:195.01-197.03 — chain item, both players get priority (`[D:196.00]`, `[D:197.00]`) before `[D:197.03] EFFECT: P2 adds 1 floating [A] (pool=1)`. New aspect (the D38 pattern on Gold): the cost "Kill this" is paid as the ability resolves — the board at idx=223 and idx=224 still has `P2 BASE | Gear:  Gold(,exh)` under the chain item, and `[D:197.05] EFFECT: kill Gold (id=110)` comes after `[D:197.02] RESOLVE: Gold (chain_item=47, ability=1, spell=0)`.
- D3: 3 sightings, a card not yet on the list: Relentless Pursuit ("Move a friendly unit") asks the destination after RESOLVE — D:115.03, D:175.03, D:235.03 (`MODE_PROMPT: Relentless Pursuit: move it where? ...`). Same spell, same moment: its "You may attach up to one Equipment" (the may and the Equipment) is also asked after RESOLVE — D:116.04, D:176.04 / D:177.04, D:236.04 / D:237.04.
- D4: 2 sightings — D:026.02-028.02 (Lucian's [Weaponmaster] with no Equipment on the board: finalized, two priority passes, `RESOLVE` with nothing); D:141.02-143.02 (Vex, see D33).
- D41 (the K3 kind: a "you may" answered after RESOLVE), a card not yet on the list: the ability Relentless Pursuit grants, "When I conquer, you may move me to my base." — the may is asked at resolution after both players passed priority on the item: D:119.03 (accepted), D:188.03 (declined), D:240.03 (declined).
- D5: 1 sighting — D:066.04, idx=69: Turn to Dust (neither [Action] nor [Reaction]) offered to P2 at Focus in the combat at Forge of the Fluft (`[1] Play Turn to Dust #632[S] @ Trinity Force #437`); not taken.
- D8: D:085.02-097.03 — Star-Crossed offered at idx=91 with P1 controlling no unit at all; `[D:086.04] TGT_PAIR_NOT_OFFERED: Star-Crossed (no legal second targets after A picked)`, paid (`[D:086.05]`), and it returned only P2's own Vex (`[D:097.03] EFFECT: bounce Vex, Apathetic (id=93) to hand`). Not offered at idx=86 when P2 had no unit either.
- D8 (the Fizz side): D:258.04-263.03 — Fizz's list at idx=288 holds Gust although no unit at a battlefield has 3 [M] or less (Kai'Sa 4, both Rengars 8; Legion Rearguard is in the base); `[D:263.03] TGT_NOT_OFFERED: Gust (no legal targets at resolve time)`; Gust is recycled (deck 27 -> 28).
- D9: 5 sightings — D:051.03, D:106.02, D:123.03 (first three); also D:156.02, D:268.01. Domains came out right each time (Equip [O] took a Body rune, Equip [R] a Fury rune, Accelerate [1][R] an exhausted Fury rune, Accelerate [1][B] an exhausted Mind rune).
- D10: D:066.13 -> D:068.05 — Trinity Force is unattached when Lucian dies in combat and recalled only after the Deadly Flourish ability and two priority decisions (`[D:068.05] RECALL (CR 323.6): Trinity Force (id=14)`).
- D13: 4 sightings — D:066.07, D:186.07, D:193.07 (first three; D:271.07): the first assignment is already on the board (`6dmg`, `10dmg`) when the second player assigns.
- D15: 2 sightings — D:051.04 (Trinity Force), D:123.04 (Long Sword): attached at activation, no chain item. (The Forge of the Fluft legend ability, by contrast, is a chain item with its two choices at finalization: D:245.03-249.04.)
- D23 / D76 root (Might not recomputed between the designation and the next window): the board at the first Focus decision of the combat at Forge of the Fluft (idx=194, after `[D:178.07] COMBAT: BF#0 attacker=P1 defender=P2`) lists `Kai'Sa, Survivor(8M,rdy,Acc,ATK,EQ:Long S..+Trinit..)`: attacker with two Equipment and Purifier ("Your Equipment each give [Assault]"), so 10. After a play and a trigger resolved the board (idx=201) and `[D:186.05] DAMAGE_STEP: BF#0 ATK=[Kai'Sa, Survivor(10M)] DEF=[Thousand-Tailed Watcher(7M)]` have 10. New aspect: Purifier's Assault. Not determined: whether the damage step would have used 8 had both players passed at once (no such combat with an equipped attacker in this game).
- D33: D:141.02 — Vex, Apathetic's ability goes on the chain for a Rengar played while Vex is in P2's base (board: `P2 BASE | Units: Vex, Apathetic(4M,exh,Dfl)`), with priority passes; it does nothing (`[D:143.02]`). With Vex at Frozen Fortress it stuns: D:184.02-186.03.
- D37: idx=185 (D:170.00), turn 12, P1's Main Phase with enemy units alone at both battlefields: Rengar in hand is offered `-> Base` only; in the same turn at chain priority (idx=183, idx=189) and at Focus (idx=194) he is offered `-> Forge of the Fluft` and `-> Frozen Fortress`.
- D40: 2 sightings — D:117.08-117.09 and D:186.13-186.14 (Kai'Sa's conquer ability and the Relentless Pursuit conquer ability, same controller, no order asked; Relentless Pursuit always on top).
- D45: D:267.00-270.03 — Bellows Breath played with no target (option `[3] Play Bellows Breath #402[S]`), Repeat paid, and the repeat's choice is never asked: `[D:270.02] RESOLVE: Bellows Breath (chain_item=63, ability=0, spell=1)` twice, nothing else.
- D46: D:209.02-212.03 — The List's "As you play this, name a tag" is a triggered ability with two priority passes; the 16 tags offered at idx=239 are those of cards on the board and in the two trashes (no Trifarian / Noxus / Pirate of P1's hand).
- D53: D:077.07 — the [Temporary] kill of the Gold token (given by Turn to Dust) is done directly at the start of the Beginning Step, before the Frozen Fortress trigger; no chain item.
- D7 (related, not the same): `[D:186.15] COMBAT: BF#1 attacker=P1 defender=P2` is logged, and the board at idx=207 shows the ATK / DEF designations at Frozen Fortress, while the two conquer abilities from the combat at Forge of the Fluft are still on the chain (`[top] Relentless Pursuit (P1)  [bot] Kai'Sa, Survivor (P1)`); they are resolved (D:187-191) before the first Focus decision of the second combat, so nothing was left pending through it. CR 468.6 has the chain of "establishing control" resolved before the next step. No outcome changed; recorded only.
- Not met in this game: D6, D14 (Long Sword was played with no unit to attach to, D:092), D21, D25, D32 (as such), D35, D43 (no Accelerate while Ezreal was out), D44, D72 (the game ended on the first hold of turn 16).

### g06 kennen vs diana — seen again

- D1: 1 clear sighting — D:109-112 (Fizz, Trickster: board idx=116-117 `Fizz, Trickster(3M,rdy)` during its own play ability's prompts, `(3M,exh)` at idx=118). Same queue order for Kennen D:138 and the effect-played Ezreal D:294.
- D2: 11 sightings — Scorn of the Moon D:018-020, D:042-044, D:049-069 (7 in all: also D:118, D:169, D:234, D:327); Seal of Discord D:061-063, D:089-091, D:140-142 (4 in all: also D:193). New aspect, a changed outcome: at idx=52 P2 activates Scorn with Focus in the showdown at Abandoned Hall (`[D:049.05] INTENT: ActivateActionAbility by P2`); the [1] is still on the chain when P2 pays Eclipse and Hard Bargain from runes and when Hard Bargain resolves: `[D:058.03] HARD BARGAIN: countered Eclipse (P2 can't pay [2] to save it)` — board idx=60 `Runes:1/6+6 (rdy:1B, exh:2B 3P)`; the Energy arrives after (`[D:069.03] ACTIVATE: Scorn of the Moon adds [1] Energy, showdowns only`). With the [Add] done at once P2 would have held [2] (1 showdown Energy + 1 ready rune) and must have been asked whether to pay.
- D3: 1 sighting — D:028-029 (Ride the Wind: `MODE_PROMPT: Ride the Wind: move it to [Battlefield #0/Battlefield #1]` after RESOLVE).
- D4: 17 sightings — Abandoned Hall `MAY_NOT_OFFERED` D:015, D:030, D:075 (12 in all); `MAY_DECLINED` Abandoned Hall D:059, D:246, Shadow Order Disciple D:096, D:149, D:226. Each stays on the chain, both players get priority, then `RESOLVE`.
- D5: 1 sighting — idx=224 (combat at Abandoned Hall, P1 has Focus): `[1] Play Lightning Rush #943[S]` — new aspect: the copy offered is the one in the TRASH (hand: Traveling Merchant only), so the Flow replay of a spell with neither [Action] nor [Reaction] is offered at Focus too (CR 829.1.b.2: Flow "does not change the timing at which it can be played").
- D8: Star-Crossed D:009 — P2 has no unit; `[D:009.05] TGT_PAIR_NOT_OFFERED: Star-Crossed (no legal first targets at resolve time)` then `[D:009.06] PAY_COST: Star-Crossed E=3 P=1`, resolves doing nothing.
- D8: Switcheroo — offered at idx=48 and idx=50 (`Play Switcheroo #466[S]`) with one unit at Zaun Warrens and one in P2's base; played at idx=73 with Shadow Order Disciple at Zaun Warrens and Ravenbloom Student at Abandoned Hall: `[D:070.04] TGT_PAIR_NOT_OFFERED: Switcheroo (no legal second targets after A picked)` / `[D:070.05] PAY_COST: Switcheroo E=2 P=2`. (At idx=22, with a single unit on the board, it was not offered: the offer test counts units, not units at one battlefield.)
- D8 (via Fizz): D:110-115 — Fizz's list offers Hard Bargain with no spell on the chain; `[D:113.04] FIZZ: played Hard Bargain from trash (energy<=3, ignoring energy), then recycled it` / `[D:115.03] HARD BARGAIN: nothing on chain to counter — no-op`.
- D9: 3 sightings — D:185.04 (Flow [3]), D:210.04 (Flow [2][A]), D:288.03 (Empower [2][P]): `PAY_ADD`, no rune prompt.
- D31: D:049-069 — Scorn activated by P2 with Focus (idx=52); when its chain is done the next Focus decision is P1's (idx=73).
- D33: 5 sightings — Vex in P2's base: D:131.02, D:138.03, D:280.02 (also D:285.02, D:294.04); each a chain item with a priority window that resolves with nothing.
- D40: 5 sightings — Ravenbloom Student + Abandoned Hall (both P2) D:058.04-05, D:115.04-05, the two Shadow Order Disciples D:148.03-04 (also D:166, D:173): no order asked.
- Judge question 1 of `docs/judge-questions-2026-10.md` ("ready up to N runes" and an opponent's runes): D:239-250 — Hwei's Gear branch lists P1's eight exhausted runes among the 11 options at idx=258 and P2 readies one of them (`Choose: Order Rune #214 (obj=51)@Base`; board idx=269 `P1 ... Runes:1/8+4 (rdy:1Y, exh:5P 2Y)` on P2's turn). First sighting I know of where the opponent's rune is actually picked.
- Not met in this game: D6, D7, D14/D15, D21 (one spell on the chain each time), D25 (nothing was hidden), D29/D30 (Diana never at a showdown), D35, D44, D72, D78 (Kennen never conquered).

### g07 yasuao vs khazix — seen again

- D1: 1 clear sighting — D:073.02-074.00: Fizz's play trigger and Vex's answer are queued and Fizz's finalize prompt asked while the board still shows `P1 BASE | Units: Fizz, Trickster(3M,rdy)` (idx=86); she is `exh` only at idx=88. (Tornado Warrior's trigger at D:123.03 is queued at the play as well; the board at idx=138 already shows her exhausted.)
- D3: 4 sightings — D:015.03 (Unforgiven), D:042.03 (Ride the Wind), D:083.03 (Ride the Wind replayed by Fizz), D:106.03 (Void Assault: both destinations in one 4-way prompt at resolution)
- D4: 2 sightings — Shadow Order Disciple declined at finalization, item stays for two priority passes and a RESOLVE line: D:113.03 -> D:115.02; D:158.03 -> D:160.02
- D9: 3 sightings — D:013.03 (Unforgiven's [2]), D:019.05 (Hide), D:116.05 (Hide)
- D11: 4 lines of kinds (a) and (b) — `CHOSE: [P1] Mulligan 1 [Irelia, Fervent]` D:003.01, `CHOSE: [P2] Mulligan 2 [Punch First, Kinkou Initiate]` D:004.01, `CHOSE: [P1] HideCard Tideturner bf=0` D:019.01, `CHOSE: [P2] HideCard Tornado Warrior bf=1` D:116.01
- D21: D:097.03 `SPELL: Hard Bargain (id=34) targets=[]`, D:100.03 `SPELL: Defy (id=3) targets=[]`, D:104.03 `COUNTER: Hard Bargain countered by Defy -> trash`. New aspect (a played-out trace of the board the D21 text only describes): chain was `[top] Hard Bargain (P1) [bot] Void Assault (P2)`; P1 was offered `Play Defy #45[S]` (idx=112) with Void Assault (2E + 1 Power: a legal choice for Defy) under P1's own Hard Bargain; Defy could only take the top item, so P1's Defy countered P1's own Hard Bargain and Void Assault resolved untouched (D:106.02). The option line names no target, so the agent could not know what it was choosing.
- D25: D:036.00 (idx=40) — Tideturner facedown at Seat of Power (hidden turn 2, D:019), P1's own Main Phase in a Neutral Open State on turn 4: 10 options, no play of the facedown card. It was offered at chain priority and at Focus on P2's turn 3 (idx=28, 31, 38) and at chain priority on turn 4 (idx=45).
- D33: D:137.02 `TRIGGER: Vex, Apathetic (P2) fires ability` when P1 plays Yasuo with Vex in P2's base (board idx=152: `P2 BASE | Units: Vex, Apathetic(4M,exh,Dfl) ...`); two forced priority passes; D:139.02 `RESOLVE: Vex, Apathetic (chain_item=27, ability=1, spell=0)` with no effect (Yasuo not stunned).
- D54: D:125.02-127.03 — Tornado Warrior's "may" (D:125.03) and its target (D:126.04) asked after `RESOLVE`, after both players had passed priority on the item (D:124.01, D:125.01).

### g08 mel vs vex — seen again

- D1: 11 sightings (play trigger queued and finalized before the permanent is shown entered; Tideturner and Fizz are on the board `rdy` during their own prompts and `exh` after) — D:016.02, D:079.02, D:242.02 (first three)
- D3: 1 sighting — D:095.03 (Ride the Wind's destination asked after RESOLVE)
- D4: 6 sightings (declined "may" stays on the chain with two priority windows) — D:049.03, D:080.03, D:108.03 (first three); also D:146.03, D:214.03, D:253.03
- D8: D:229.09 — Switcheroo's first-target list `[Mel, Newly Awakened/Tideturner/Hwei, Brooding Painter]` includes Hwei, alone at Bandle Tree (no second unit there); not chosen. D:243.04 — Fizz's list `[Gust/Smoke Screen/Rebuttal/Shock Blast/Gust/Star-Crossed]` includes Rebuttal with no spell on the chain to choose; not chosen.
- D9: 5 sightings — D:029.02 (Deflect), D:034.05 (Hide), D:179.03 (Mel's Empower); also D:209.05, D:352.03
- D11: (a) D:034.01 `CHOSE: [P2] HideCard Tornado Warrior bf=0`, D:209.01 `CHOSE: [P1] HideCard Tideturner bf=0`; (b) D:003.01, D:004.01 (mulligan card names); (e) D:163.03 `PREDICT: Mutated Mouser -> bottom of deck (recycled)`
- D13: 3 sightings (the board of the defender's assignment decision already shows the attacker's damage) — D:270.05 (idx=300: `Hwei,Broo(5M,5dmg,exh,DEF)`), D:278.05 (idx=312), D:317.04 (idx=355)
- D18: D:219.04 — `PAY_COST: Mutated Mouser E=0 P=0` on turn 15 with the discount made on turn 14 (`[D:203.03]`, P1's turn, off P2's Rebuke). D:231.04 — `PAY_COST: Switcheroo E=0 P=0` on turn 16 (P1's turn) with the discount made on turn 15 (`[D:221.03]`). New aspect: the unused discount crosses from one player's turn into the other's both ways, and here it is what lets P2 play a [2]+2 Power spell for nothing in the opponent's showdown.
- D19: 3 sightings — D:118.02 (the Heron itself just played to base), D:233.06 (Heron moved back to base at D:222.03), D:299.02 (second Heron in base); each resolves with no effect line after two priority windows. At D:299 the window was used: P1 played Stupefy in it (D:301).
- D21: D:068.03 — Defy with `[top] Rebuttal (P1) [bot] Shock Blast (P1)` on the chain (idx=73): both are legal for Defy (Rebuttal [1]+1 Power, Shock Blast [3]+1 Power) and no choice was asked; `[D:072.03] COUNTER: Rebuttal countered by Defy -> trash`. Single-spell chains: Rebuttal D:064.03, Abandon D:158.03.
- D25: D:052 (idx=54) — turn 7, P2's Main Phase list of 11 options has no play of the Tornado Warrior hidden on turn 5 (it is offered at chain priority at idx=46 and idx=52). (Under N2 it should not be playable there at all.)
- D27: D:270.17, D:270.19 — Pyke, Returned is in P2's base (`P2 BASE | Units: Pyke, Returned(3M,rdy,Hdn)`) when Hwei and Fizz die at Bandle Tree; two abilities go on the chain, each gets two priority windows and resolves with nothing (D:272.02, D:274.02). New aspect: two items for two deaths of one damage step although the text is "Once each turn".
- D32: D:246.03-D:249.02 — Fizz replays Smoke Screen (`PAY_COST: Smoke Screen E=0 P=1`, a Mind rune recycled), `RESOLVE: Smoke Screen (chain_item=56, ability=0, spell=1)` with no effect line; the card is recycled (P1 deck 25 -> 26).
- D40: D:270.14-D:270.19 — Scuttle Crab's Deathknell and the two Pyke items, all P2's, queued in engine order; no ordering asked.
- D51: D:102.02, D:288.02 — Tornado Warrior played from hand puts "When you play me from face down" on the chain; resolves empty (D:104.02, D:290.02).
- D54: D:056.03-D:058.03 — Tornado Warrior from facedown: the "may" and the object are asked after `RESOLVE`.

### g09 rumble vs reksai — seen again

- D1: 6 sightings — D:040.02-041.03 (Gem Jammer's play trigger offers `Gem Jammer #330 (obj=11)@Chain` as its own target; board shows it `rdy` then `exh`), D:072.00 (Rek'Sai's Accelerate asked after the cost is paid and the unit rendered on the board), D:162.02-163.00 (Thousand-Tailed Watcher's play trigger queued before the Accelerate question). Also D:057, D:214.02-215.00, D:324-325.
- D4: 2 sightings — D:275.08-277.02 (Void Burrower, legend already exhausted: `MAY_NOT_OFFERED`, the item stays, both players pass, `RESOLVE` with nothing), D:347.03-349.02 (`MAY_DECLINED`, stays, both pass, `RESOLVE`).
- D5: 4 sightings — idx=128 inside D:125 (Void Rush offered to the attacker at Focus), D:178.04 (idx=191: Cull the Weak and Falling Star offered; Falling Star played), D:191.06 (idx=206: Cull the Weak offered and played). Also idx=345 inside D:321 (Production Surge and Falling Star offered to the attacker). In the two non-combat showdowns where a non-Action spell was in hand with runes ready (idx=275 P2 with Void Rush and 10 runes; idx=375 P1 with Production Surge) it was correctly NOT offered.
- D9: 4 sightings — D:024.01 (Repeat), D:095.05 (Hide: the engine recycled an exhausted Fury rune), D:148.03 (Empower). Also D:215.02 (Accelerate).
- D13: 3 sightings — idx=131 inside D:125 (board shows `Forecaster(2M,4dmg,exh,DEF)  Rumble,Hoth(4M,4dmg,exh,DEF)` before P1 is asked to assign), idx=182 inside D:172, idx=348 inside D:321 (`Noxus Hopeful(4M,15dmg,exh,DEF)` before P2 assigns).
- D14: D:288.01-288.02 — Cloth Armor played as a Reaction in P2's Beginning Step, `QUICK_DRAW: auto-attaching`, no chain item.
- D20: D:189.03-189.07 (Falling Star: `kill Mech` between its two instructions), D:336.03-336.06 (`kill Gem Jammer` between them). No replacement involved, outcome unchanged.
- D40: 2 sightings — D:040.02-040.03 (Gem Jammer: [Vision] and its own play trigger queued together, no order asked), D:057.02-057.03 (Bubble Bot, same).
- D51: D:168.02-170.02 — new card for this item: Patched Porobot ("When you play me, if you control 3 or more other gear, draw 1.") with no gear on P1's side: `[D:168.02] TRIGGER: Patched Porobot (P1) fires ability`, both players get a priority decision (idx=175, idx=176), `[D:170.02] RESOLVE: Patched Porobot (chain_item=41, ability=1, spell=0)` with no effect. CR 383.2.a.1: "Any additional conditional statement immediately after the Condition must be true in order for the Condition to be fulfilled. Such a conditional statement is part of the Trigger Condition and not the Effect." The class tests the gear count in `onTrigger` (`src/cards/units/0845_patched_porobot.cpp`), no `triggerCondition`. (D51 names Kha'Zix, Evelynn, Tornado Warrior; judge question Q4 covers the "if you paid the additional cost" and Noxian Emissary variants.)
- D58: D:132.03-135.03 — Rek'Sai, Breacher on the board at Void Gate (board idx=134 `BF Void Gate | P2: Rek'Sai,Brea(3M,exh,Asl,Acc)`), 9 ready runes: `[D:132.03] VOID BURROWER: banished Faithful Manufactor then play it` / `[D:132.05] PAY_COST: Faithful Manufactor E=3 P=0` / three rune picks / `[D:135.03] VOID BURROWER: recycled the rest`; no Accelerate question; the unit is on the board exhausted at idx=142. (The second Void Burrower play, D:311, is with Rek'Sai in the trash: no offer due, none made; D59 not triggered — that Manufactor, id=70, died at D:172.12 and was not played again.)
- D61: related, not a full sighting — Inferna played with [Ambush] at chain priority into her side's attack (`[D:185.03] PLAY: Inferna (id=78, Unit, cost=2E)`): board at idx=201-204 `Inferna(1M,exh,Asl2)` with no ATK flag while the chain resolves; at idx=205, after the chain is empty, `Inferna(3M,exh,Asl2,ATK)`. No damage step followed (the only defender had died), so nothing was miscounted here. New aspect: it shows the designation (and with it the Assault) arriving only with the cleanup after the chain, which fits D61's missing Assault being the same lag as N1 / D76.
- Q2 (judge question already asked, Rek'Sai + tokens): the situation occurred twice with Rek'Sai on the board and no Accelerate offered — `[D:127.03] TOKEN: created Bird (1M, id=110) for P2` (Carrion Dredger's Deathknell), `[D:137.03] TOKEN: created Recruit (1M, id=111) for P2` (Faithful Manufactor).
- Not seen in this game: D2 (no [Add]), D3, D6 (no non-Reaction spell appeared in a chain-priority list: idx=198 with Cull the Weak in hand offers only Pass and Inferna), D7, D15, D21, D25 (the one facedown card was hidden on turn 7 and played at the first window of turn 8; CR 811.1.b "Beginning on the next turn"), D35, D44, D47/D48 (Temporal Breach replayed a Forecaster at Void Gate: no Accelerate, not Rockfall Path), D72.

### g10 leona vs darius — seen again

- D1: 6 sightings — D:036.02, D:096.02, D:159.02 (first three; also Scuttle Crab D:178.02, Faithful Manufactor played by Baited Hook D:137.06, and Kai'Sa's Accelerate asked after the cost is paid D:019.00)
- D2: 6 sightings (Hand of Noxus as a chain item with priority for both) — D:010-011, D:038-039, D:063-064 (first three; also D:098-099, D:132-133, D:198-199)
- D3: 1 sighting — D:076.03 (Charm: "move it to [Battlefield #0/Battlefield #1]" asked after RESOLVE)
- D4: 2 sightings — D:077.11 -> D:082.02 (Monastery of Hirana, no buff; P2 plays Inferna in that window at D:078), D:096.04 -> D:101.02 (Spectral Matron, empty trash; P2 activates the legend in that window)
- D9: 6 sightings — D:030.03, D:130.03, D:155.05 (first three; also D:190.03, D:192.03, D:215.05)
- D15: 2 sightings — D:030.04 (Guardian Angel), D:190.04 (Eye of the Herald)
- D21: 1 sighting — D:163.03 `targets=[]`, D:167.03 (Defy takes the top item, Hidden Blade)
- D40: 2 sightings — D:077.08-077.09 (Kai'Sa + Monastery of Hirana, P2), D:171.03-171.04 (Apprentice Smith + Noxian Drummer, P1)
- D72: 1 sighting — D:227.08-227.09 (two holds scored one after the other; no hold ability involved, so nothing visible follows from it)
- D10: D:212.11 -> D:214.07 — Eye of the Herald unattached when Scuttle Crab dies, two chain-priority decisions and the Deathknell resolution later it is recalled (`RECALL (CR 323.6)`), after the conquer line.
- D11: D:003.01, D:004.01 (kind b: `CHOSE: [P1] Mulligan 2 [Vi, Peacekeeper, Vi, Peacekeeper]`); D:155.01, D:215.01 (kind a: `CHOSE: [P2] HideCard Hidden Blade bf=0` / `bf=1`).
- D13: D:149.04-149.07 (idx=165), D:151.04-151.07 (idx=171), D:175.06-175.09 (idx=199), D:212.05-212.08 (idx=240), D:224.05-224.08 (idx=256) — in all five combats the second assigner's board already carries the first side's damage.
- D23: idx=123 (D:113), idx=192-193 (D:172-173), idx=247-248 (D:219-220) — units that have just moved into Trifarian War Camp are shown without the +1 [M] at the chain-priority decisions that follow (`Noxian Drummer(3M,exh)`, `Apprentice Smith(2M,exh)`, `Scuttle Crab(0M,exh,Dkn)`); the +1 is there by the DAMAGE_STEP lines. No Might-reading effect was played in those windows.
- D31: D:062.05 -> D:064.03 (idx=67 -> idx=70) — new aspect: another card and a non-combat showdown. P2, with Focus in the showdown at Trifarian War Camp, activates Hand of Noxus (`[D:064.03] HAND OF NOXUS: no Legion — no effect`); the next decision is P1's Focus (`--- P1 Decision #70 [MainPhase] SHOWDOWN ---`, `[0] Pass Focus [1] Play Hidden Blade #213[S]`), and P1 uses it (`[D:064.05] SPELL: Hidden Blade (id=8) targets=[]`, killing Leona). CR 346.1 / 430.2.a: Focus does not pass from an [Add] ability. P1 had already passed (idx=66), so with Focus kept by P2 the showdown would have gone on from P2's decision, not P1's.

### g11 lux vs draven — seen again

- D1: 6 sightings — D:143.02/144.00 (Thousand-Tailed Watcher: trigger queued, then the Accelerate prompt), D:182.02 (Evelynn + Star Spring finalized with the permanent still the bottom chain item, SNAP 331), D:271.02 (Tideturner, SNAP 464)
- D2: 5 sightings — D:210.05-212.03 (Lux, Crownguard, a card not in the item's list), D:258.02-264.04 (Gold; P1 answers with its own Gold on top, D:260.01), D:312.03-314.05 (Jhin, Murderous Artist "When I move, [Add] [1][A]" as a triggered chain item with both priorities)
- D3: D:051.03-052.03 — Ride the Wind, destination asked at resolution
- D4: 3 sightings — D:052.10-054.02 (Treasure Hoard, unpayable: `MAY_NOT_OFFERED`), D:106.03-108.02, D:303.03-305.02 (declined)
- D5: 7 offers, 4 plays — D:119.05 (Up from the Deep from the trash for Flow, attacker), D:212.04 (Time Warp by the defender on the opponent's turn), D:281.04 (Time Warp, attacker); also Sprite Burst D:330.06 (defender, opponent's turn; its two tokens were put at the fought battlefield and counted as defenders, D:345.05) and offers of Falling Star / Progress Day at idx=130, 134, 231, 311, 350. In the three non-combat showdowns with a hand to compare (idx=79, 84, 219) no such spell was offered.
- D9: 5 sightings — D:119.07 (Flow), D:144.02 (Accelerate), D:202.04 (Treasure Hoard)
- D10: D:086.04-090.08 — Spinning Axe unattached when its bearer (Ravenbloom Prefect) was banished as a cost, recalled only after four priority decisions. New aspect: the bearer left by banishment, not death.
- D11: D:003.01, D:004.01 — `CHOSE: ... Mulligan` lines name the cards set aside
- D13: 5 combats — D:121.06-121.09, D:233.05-233.08, D:302.07-302.13, D:345.06-345.11, D:346.06-346.10
- D14: D:080.01 — `QUICK_DRAW: auto-attaching Spinning Axe to Ravenbloom Prefect`, no chain item
- D20: D:130.03-130.07 — Falling Star kills the first unit before dealing the second 3 (no replacement involved here)
- D31: D:210.05 (idx=231) — Lux, Crownguard's [Add] activated with Focus in a combat: next decision is `P2 (forced): Pass Focus` (idx=234), then P1 again (idx=235). New aspect: another card than Scorn of the Moon.
- D38: D:262.05, D:264.05, D:300.05 — Gold: "Kill this, [E]:" — exhausted at activation (SNAP 453 `Gold(,rdy)  Gold(,exh)`), killed only when the ability resolves (`[D:262.05] EFFECT: kill Gold (id=118)`). New aspect: another card than Divining Shells (moot once D2 is fixed).
- D40: 2 sightings — D:182.02-182.03 (Evelynn + Star Spring), D:201.05-201.06 (Plundering Poro + Treasure Hoard)
- D44: D:085.03 -> D:090.05 — Tentacle (1 [M], 3 damage from Wages of Pain) stays through the resolution of two more chain items
- D51: D:182.02 / D:188.02 — Evelynn, Entrancing played from hand puts "When you play me from face down" on the chain; it resolves with nothing
- D53: D:022.07 and D:114.07 (Spinning Axe), D:027.07 / D:027.09 (Sprite, Sprite Fountain), D:131.07, D:347.07 (Sprite) — killed directly in the Beginning Step

### g12 sivir vs viktor — seen again

- D1: 5 sightings — Accelerate asked after the cost is paid and after Battle Mistress's "may" D:030-032,
  D:089-091; play trigger queued with the unit still shown on the chain / ready D:267.03 (Ezreal), D:281.03
  (Fizz); Repeat asked after the base cost D:041.00 (the "further evidence" kind).
- D2: 6 sightings (every Gold) — D:057-059, D:079-081, D:142-144 (first three); at D:081 and D:083 P2 is
  offered two Vi, Peacekeeper plays on top of the Gold item.
- D4: 27 sightings — Dusk Rose Lab with no unit there 16 (D:004.11, D:007.09, D:013.09 ...), Battle Mistress
  exhausted 8 (D:110.04, D:110.06, D:117.09 ...), declined 3 (D:103.03 Shadow Order Disciple, D:152.03,
  D:153.03 Battle Mistress).
- D5: 1 sighting — idx=153 (after `[D:147.04] COMBAT: BF#0 attacker=P1 defender=P2`): `[1] Play Rampage
  #870[S]` at Focus (Rampage has neither [Action] nor [Reaction]); not chosen. In the two non-combat showdowns
  where P2 held Singularity and could pay (idx=232) it was correctly not offered.
- D9: 9 sightings — D:019.03, D:117.06, D:135.03 (first three; Herald, Hide x4, Equip).
- D10: D:141.05 -> D:146.06 (Edge of Night lies unattached at Dusk Rose Lab through 5 priority decisions);
  D:323.04 -> D:331.06 (8 decisions).
- D13: 3 sightings — D:159.04-.06 (board at idx=169 shows `Vi, Peacekeeper(5M,6dmg,...)` before P2 assigns),
  D:193.06-.08, D:305.06-.08.
- D15: D:165.05 — `ATTACH: Edge of Night (id=26) -> Sivir, Mercenary` at activation, no chain item.
- D23 / D44 (one root; counted under D23): Sivir, Mercenary's +2 [M] arrives only when the chain is empty —
  new card for the item. D:109.01 / D:110.01 (2nd and 3rd Power of turn 7): board 4M at D:111-112, 9M after
  `[D:112.03] EFFECT: give Sivir, Mercenary 5 [M] this turn -> 9M` through D:113-116 (11M due), 11M at idx=122.
  D:150.01 / D:151.01 (turn 9, Sivir attacking): 4M through the priority decisions D:152-D:159 of the combat
  showdown (6M due), 6M at `[D:159.03] DAMAGE_STEP` (correct there). D:274.06 (turn 15): 6M through D:275-276,
  8M and Ganking (`Move Sivir, Mercenary #464 -> Dusk Rose Lab`) at idx=299.
- D27: D:305.13 and D:305.17 — two Pyke, Returned triggers for the two deaths of one damage step; item 83
  makes the Gold (D:307.03), item 80 resolves empty (D:313.02).
- D39: 4 sightings, new aspect: the trigger comes from the BASE Power cost (a rune recycled -> Battle
  Mistress), so every spell with a Power cost in this list shows it — D:070.02 (Star-Crossed resolves D:073.02,
  Battle Mistress D:075.02), D:109.02 / D:110.02 (Punch First D:112.02 first), D:150.02 / D:151.02, and for the
  spell Fizz plays D:286.02 / D:287.02 (chain at idx=311: `[top] Punch First ... [bot] Battle Mistress`).
  No outcome changed. (For a unit the unit leaving the chain first is right, CR 359.2.)
- D40: D:305.13-.17 — P1 has three simultaneous triggers (Pyke, Battle Mistress, Pyke) and is not asked;
  new aspect: the turn player's (P2) Honest Broker Deathknell is put between them (chain at idx=336 `[top] Pyke,
  Returned (P1) [ ] Honest Broker (P2) [ ] Battle Mistress (P1) [bot] Pyke, Returned (P1)`), not first as
  CR 383.3.d.1 has it (turn player orders theirs first).
- D66: D:137.03 — Recruit to base, no prompt, while P2 controls Zaun Warrens.

### g13 renata vs poppy — seen again

- D1: 7 play triggers of permanents — D:014.02 (Shepherd's Heirloom), D:027.02 (Poppy), D:263.02 (Jayce: SNAP 415 shows `Jayce,Man (4M,rdy)` while its trigger is being finalized, SNAP 417 `(4M,exh)`); also D:186.02, D:227.02, D:241.02, D:254.02. The "K4 kind" part of D1 (optional additional cost asked after the base cost is paid, CR 355.1.a): Rampage `[D:081.04] PAY_COST: Rampage E=3 P=0` ... `[D:085.00] DECISION #87 (P2): pay additional cost for Rampage? [decline|pay] [2 options]`.
- D2: 3 Gold activations as chain items with priority for both players — D:069.02-071.04, D:162.02-165.04, D:163.01-167.04 (second Gold activated in response to the first).
- D4: 2 declined "may" triggers kept on the chain and resolved — Chem-Baroness D:042.03 -> priority D:043.00 / D:044.00 -> `RESOLVE` D:044.02 (P1 is offered a facedown Wages of Pain in that window, idx=45); Irresistible Faefolk D:178.03 -> D:179.00 / D:180.00 -> `RESOLVE` D:180.02.
- D9: `PAY_ADD` with the engine choosing the rune — D:039.05 (Hide, recycled 1), D:094.04 and D:181.04 (Sunken Temple, exhausted 1); the three Equip lines pay [0] (D:030.03, D:147.03, D:244.03).
- D11: (a) `[D:039.01] CHOSE: [P1] HideCard Wages of Pain bf=1`; (b) `[D:003.01] CHOSE: [P1] Mulligan 2 [Corina Veraza, Watchful Sentry]`, `[D:004.01] CHOSE: [P2] Mulligan 1 [Demacian Diplomat]`.
- D13: 3 combats, the defender's assignment prompt shows the attacker's damage already marked — D:116.05-.07 (SNAP 209 `Poppy, Paragon(7M,8dmg,...)` before P2 assigns), D:197.05-.07, D:273.06-.08.
- D15: 3 Equips of Shepherd's Heirloom with no chain item — D:030.02-.05, D:147.02-.05, D:244.02-.05.
- D25: facedown Wages of Pain not offered at P1's own Neutral Open decisions idx=47 (D:045), idx=53 (D:051), idx=54 (D:052), while it is offered at the chain-priority decisions of the same turn (idx=45, 51, 59).
- D40: 5 sets of simultaneous triggers of one player queued with no ordering decision — D:142.06-.07 (Master Yi + Targon's Peak), D:171.09-.10, D:198.09-.10 (Keeper of the Hammer + Master Yi); also D:210.06-.07, D:274.09-.11 (three items).
- D44: D:059.03-061.08 — Soaring Scout (1M) carries 3 damage from Wages of Pain through two priority decisions (D:060.00, D:061.00) while Hidden Blade is still on the chain; it dies only at `[D:061.08]`.
- D51: D:027.02 / D:029.02 — new aspect: card not in D51's list. Poppy, Paragon "When you play me, if an opponent's score is within 3 points of the Victory Score, ready me and gain 3 XP." is put on the chain at opponent score 0 (`[D:027.02] TRIGGER: Poppy, Paragon (P2) fires ability`), both players get priority (D:028.00, D:029.00) and it resolves with nothing (`[D:029.02] RESOLVE: Poppy, Paragon (chain_item=5, ability=1, spell=0)`). CR 383.2.a.1: the "if" right after the "When" clause is part of the trigger condition. (Section 6 records the no-effect outcome at score 4 as correct; the chain item is the D51 part.)
- D72: 2 Scoring Steps with two holds, each hold's triggers resolved before the next hold is scored — turn 15 `[D:198.08] SCORE: P2 +1 via Hold BF#0 -> 5` ... `[D:210.05] SCORE: P2 +1 via Hold BF#1 -> 6`; turn 17 `[D:274.08]` ... `[D:280.05] SCORE: P2 +1 via Hold BF#1 -> 8`. In turn 15 P1 played Consult the Past between the two holds (D:200.01).
- Not seen in this game: D3, D5 (every spell offered at Focus was [Action] or [Reaction]; Cull the Weak / Sabotage were unaffordable at each Focus), D6, D7, D10 (Heirloom recalled in the same cleanup, D:116.15-.19), D14, D21, D35, D37.

### g14 yi_rebuild vs ahri — seen again

- D1: 7 sightings (play triggers queued/finalized before the permanent is in) — D:008 (Pit Rookie), D:059 (Janna), D:153 (Sprite Fountain); also D:146, D:219, D:237, D:247
- D2: 4 [Add] chain items, all Blue Sentinel's delayed "at the start of your next Main Phase, [Add] [A]" — D:129.11-133, D:202.11-212. New card for the item; and at D:203 P2 itself played Stupefy with the two [Add] items on the chain (both players got priority each time)
- D3: 2 sightings (Charm destination after RESOLVE) — D:035.03-036.04, D:116.03-117.04
- D4: 5 sightings — D:008.04 (targetless Pit Rookie item stayed through D:021; En Garde and Discipline were played in its windows), D:036.10 (Emperor's Dais unpayable), D:119.03 (declined Amateur Recital); also D:194.03, D:231.03
- D9: 4 sightings — D:067.05 (Hide), D:142.04 (Emperor's Dais [1]), D:181.04; also D:184.05
- D13: 3 sightings (attacker's damage marked on the board before the defender assigns) — D:179.06-09 (board at idx=196 already shows `Sprite(3M,8dmg..)`), D:252.05-07 (idx=275), D:257.06-09 (idx=284)
- D40: 3 sightings — D:118.09-10 (Blue Sentinel x2 + Amateur Recital x2 put on the chain in a fixed order), D:161.04-05 (two Nine-Tailed Fox items), D:191.07-08
- D37: D:069 (idx=76: Rengar offered only `-> Base` with an enemy Ravenbloom Student alone at Amateur Recital); D:217 (idx=236: `-> Base`, `-> Emperor's Dais`, not Amateur Recital with enemy Sprite + Ravenbloom there). As a Reaction at D:221 (idx=240) both `-> Emperor's Dais` and `-> Amateur Recital` are offered.
- D53: D:185.07, D:266.07 (Sprite Fountain gear), D:230.07 (Sprite token) — [Temporary] kill done directly in the Beginning Step, no chain item. New cards for the item: Sprite Fountain and its Sprite.
- D76: D:066.05 — `DAMAGE_STEP: BF#1 ATK=[Ravenbloom Student(3M)] DEF=[Pit Rookie(2M,STUNNED)]`: Pit Rookie defends alone, nothing resolved between `COMBAT:` and the damage step, no +2. New aspect: it decided the combat — at 4 [M] the 3 damage is not lethal (D:066.08 kills it at 2M), both sides survive, the attacker is recalled and P2 does not conquer (D:066.09-10, P2's 2nd point; P2 then holds there on turns 6-14).
- Not seen although possible: D5 (idx=37: only Back Off offered at Focus with Rampage and Charm affordable — a non-combat showdown; idx=193 combat: P1 held only Punch First/Back Off), D6 (idx=176, 183, 240: no non-Reaction spell offered at chain priority), D25 (both hidden cards were played before their controller's next Main Phase), D7, D14/D15, D21, D44, D72.

### g15 leblanc vs akali — seen again

- D1: 2 play triggers in the game (Thousand-Tailed Watcher D:136.02, Blitzcrank D:162.02); the order "queued
  before the unit has entered" cannot be told from this trace, and no Accelerate prompt occurred (after both
  Accelerate units' base costs no ready rune was left: Kai'Sa D:082-086, Thousand-Tailed Watcher D:128-136).
- D4: 4 sightings — D:031.03-033.02 (Deceiver declined, priority round, `RESOLVE`), D:144.03-146.02 (Akali,
  Deadly Weapon declined, same), D:162.04-169.02 (Blitzcrank `MAY_NOT_OFFERED`, see N1; a card was played in the
  window), D:059.05-061.02 (Deceiver's reflexive "It becomes a copy of another unit there" with no other unit
  there: chain item 14 gets a priority round and resolves doing only "Give it [Temporary]").
- D9: 3 sightings — D:034.05, D:036.05, D:062.05 (the [A] of a Hide; the engine picked the rune: an Order, a
  Fury, an Order), e.g. `[D:034.05] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1`
- D13: D:077.04-077.07 — the attacker's 2 on Soaring Scout is marked (board of idx=88 `Soaring Scout(1M,2dmg,...)`)
  before the defender is asked to assign.
- D14: 2 sightings — D:072.01 (Long Sword played at Focus in the combat, attached inline to Mischievous Marai),
  D:167.01 (Long Sword at chain priority, attached inline to Akali).
- D28: Deathgrip with a single friendly unit — offered idx=22 (D:020, two copies, both `@ LeBlanc, Fragmented`,
  LeBlanc the only unit) and offered and PLAYED idx=48, LeBlanc the only friendly unit:
  `[D:044.03] SPELL: Deathgrip (id=16) targets=[LeBlanc, Fragmented(id=2)]`
  It then mistargeted (D:057.03) and drew 1 (D:057.04).
- D53: D:118.07 — the [Temporary] Mischievous Marai copy is killed directly in the Beginning Step, no chain item.
- Not seen although the situations arose: D5 (two combat/showdown Focus rounds, D:068-077 and D:146: no spell
  without [Action]/[Reaction] was affordable for the seat with Focus in the combat; in the non-combat showdown at
  idx=158 Shuriken Flip was correctly withheld), D6 (chain-priority lists idx=48, 51, 54, 105, 176 hold only
  Reactions / facedown cards; Cull the Weak and Shuriken Flip, affordable, are not in them), D25 (no own Main
  Phase decision with a playable facedown card), D10 (Long Sword recalled at once when its bearer died, D:179.05-.07).

### g16 jayce vs ambessa — seen again

- D1: 7 sightings — D:085, D:152, D:172 (first three); also D:192, D:255, D:300 (Jayce's first-gear trigger finalized with Dazzling Aurora still shown on the chain: `[top] Dazzling Aurora (P1)  [bot] Jayce, Brilliant Inventor (P1)` at idx=309), D:314
- D2: 16 sightings (every Platewyrm Egg [Add]: a chain item, both players pass priority before the Energy arrives) — D:016-018, D:040-045, D:070-072 (first three)
- D4: 4 sightings — D:057-059 (Irresistible Faefolk, no enemy unit), D:085-087 (Profiteer, nothing empowered), D:173-178 (Jayce declined; P1 activated an Egg on top of the declined item); also D:255-257 (Profiteer)
- D5: 3 sightings — idx=216 and idx=225 (D:213.04 / D:220.04: Public Execution, neither [Action] nor [Reaction], offered to P2 at Focus in the combat via Flow), idx=217 (D:213.05-220.04: Sabotage offered to and played by P1, the defender, on P2's turn at Focus)
- D9: 12 sightings — D:015.03, D:027.01, D:037.04 (first three)
- D45: D:027-029 — Bellows Breath played with nothing chosen and the Repeat paid: `RESOLVE` twice, no choice asked for the second execution; the Repeat is asked after the base cost is paid (D:024.04 -> D:027.00), as the item's "also seen" says
- No sighting of D3, D6 (non-Reaction spells were NOT offered at chain priority in this game: e.g. idx=196, 209, 215 with Sabotage in P1's hand), D7, D14/D15, D21, D25, D35, D40, D44, D72.

### g17 lillia vs azir — seen again

- D1: 5 sightings — Accelerate asked after the base cost is paid and with the unit rendered on the board `(…,rdy,Acc)`: D:106.00 (Azir, idx=122), D:115.00 (Lillia, idx=131), D:126.00 (Lillia, declined, idx=142; shown `rdy` at idx=142 and `exh` at idx=143); Sprite Fountain's play trigger D:010.02, D:050.02.
- D4: 13 sightings — every Beginning Step's Dusk Rose Lab item (`MAY_NOT_OFFERED` then priority to both players then `RESOLVE`): D:004.09, D:014.07, D:024.09 (first three); also D:038.07, D:044.11, D:063.07, D:082.11, D:098.07, D:108.09, D:116.07, D:119.07, D:139.07, D:165.09. New aspect: see N2 (controller of the item).
- D9: 10 sightings — D:029.03, D:037.05, D:073.04 (first three); also D:106.02, D:107.03, D:115.02, D:138.03, D:147.03, D:158.03, D:164.03. In every one the rune that left (read from the boards before/after) was of the right Domain: Mind for Lillia's Accelerate, Order for Azir's Accelerate, Guards!'s [C] and Eye of the Herald's Equip, Calm for the Brutalizer Equips.
- D10: D:153.04-155.06 — Azir killed by Deathgrip, `EFFECT: unattach Brutalizer (id=70)` / `EFFECT: unattach Eye of the Herald (id=73)`, two chain-priority decisions (D:154, D:155) and a RESOLVE later `RECALL (CR 323.6)` for both.
- D11: D:003.01 `CHOSE: [P1] Mulligan 2 [Sprite Burst, Brutalizer]`, D:004.01 `CHOSE: [P2] Mulligan 1 [Guards!]` (kind b); D:037.01 `CHOSE: [P1] HideCard Smoke and Mirrors bf=0` (kind a).
- D13: D:081.06-081.09 — the attacker's 2 are on the Sprite (`Sprite(3M,2dmg,exh,Tmp,DEF)` at idx=94) before the defender assigns.
- D15: 4 sightings — D:107.04, D:138.04, D:147.04 (first three); D:164.04.
- D21: 3 sightings — `[D:056.03] SPELL: Defy (id=57) targets=[]`, `[D:089.03] SPELL: Defy (id=7) targets=[]`, `[D:179.03] SPELL: Defy (id=6) targets=[]` (one spell below each time, so no choice was lost here).
- D22: D:077.03 and D:162.03 — Weaponmaster's "may" asked after `RESOLVE: Sand Soldier`.
- D23: 3 sightings of the stale Brutalizer "+2 if attached this turn" in the next turn's Awaken / Beginning Step, through that step's two chain-priority decisions — Azir 7M at SNAP 186-189 (idx=125-126), 5M from SNAP 190; Lillia 6M at SNAP 243-246 (idx=158-159), 4M from SNAP 247; Sand Soldier 5M at SNAP 277-280 (idx=184-185), 3M from SNAP 281. Whether only the rendered value is stale I could not determine from the trace.
- D28: Deathgrip offered with Azir as P2's only unit at idx=160, 165, 166 (Main Phase) and idx=167 (chain priority), played there: `[D:149.03] SPELL: Deathgrip (id=91) targets=[Azir, Sovereign(id=55)]` / `[D:153.03] EFFECT: kill Azir, Sovereign (id=55)` / `[D:153.06] EFFECT: P2 draws 1 (deck=29)`.
- D40: 3 sightings — P1's Sprite Fountain Deathknell and Dusk Rose Lab item queued together without an order prompt (D:024.08-024.09, D:082.10-082.11); P2's Weaponmaster item and Guards!'s reflexive item (D:072.05-072.07).
- D44: D:153.03-155.03 — no cleanup between the Deathgrip item and the Eye of the Herald item (the unattached gear waits; same lines as D10).
- D53: 5 Beginning Steps with a direct Temporary kill and no chain item — D:024.07 (Sprite Fountain), D:044.07-044.10 (two Sprites), D:082.07-082.09 (Sprite, Sprite Fountain), D:108.07, D:165.07. The Dusk Rose Lab pairing the item describes happened twice with Lillia's Sprites: turn 4 (Dusk Rose Lab `[Ctrl:P1]`, Sprite id=110 there: `[D:044.09] KILL: Sprite (id=110, 3M, dmg=0)` then `[D:044.13] MAY_NOT_OFFERED: Dusk Rose Lab: kill a unit here to draw 1? (precondition failed at prompt time)`) and turn 8 (Sprite id=114: D:108.07 then D:108.11) — P1 was never able to kill the dying Sprite for a card.

Not met in this game: D2, D3 (Charm never played), D5 and D6 (the one combat showdown had two forced passes; no non-Reaction spell was offered at chain priority), D7, D14, D25 (the only facedown card was gone before P1's next Open State), D35, D72 (one hold only).

### g18 missfortune vs zed — seen again

- D1: 2 sightings of a play trigger queued and finalized before the unit is on the board — D:031.02 (Tideturner), D:277.02 (First Mate; its target is asked at idx=319 with First Mate rendered ready, exhausted at idx=320). Plus the D1 (R4-13) aspect: Rampage's optional additional cost asked after the Energy is paid — `PAY_COST` D:236.04, question D:240.00.
- D2: 1 sighting — Gold, D:107.01 -> D:109.02 (two priority decisions before the [A] arrives). New aspect, not in D2's text: Gold's "Kill this" is also paid at resolution, the D38 pattern on another card — the board at idx=116-117 still has `P1 BASE | Gear:  Gold(,exh)` with `[top] Gold (P1)` on the chain, and `[D:109.05] EFFECT: kill Gold (id=109)` comes after `[D:109.02] RESOLVE: Gold (chain_item=20, ability=1, spell=0)`.
- D3: 2 sightings — D:057.03, D:064.03 (Ride the Wind, destination after RESOLVE).
- D4: 4 sightings — D:031.04 -> D:033.02 (Tideturner, MAY_NOT_OFFERED), D:127.03 -> D:132.02 (Last Rites declined), D:213.03 -> D:218.02 (Last Rites declined); also D:241.03 -> D:248.02 (Spirit Wheel declined; P1 played Nidalee with [Ambush] in that window, D:242.02).
- D5: 4 decisions in combat showdowns offering a spell with neither [Action] nor [Reaction] — idx=132 (D:123.04, Death Mark x2 by Flow), idx=172 (D:157.03: Death Mark played by Flow by the DEFENDER on the opponent's turn), idx=267 (D:234.03: Onslaught and Rampage offered, Rampage played); also idx=137. In the one non-combat showdown where such a spell was in hand and payable it was correctly withheld (idx=81: Death Mark, 7 ready runes).
- D9: 7 `PAY_ADD` lines — D:083.03, D:116.02, D:133.04 (first three); the others D:157.05, D:185.03, D:240.01, D:270.03. At D:116.02 (Perfect Execution's Flow [3][R] with 4 Fury and 3 Chaos ready) the engine exhausted the three Chaos runes and recycled a READY Fury rune, giving up that rune's Energy.
- D10: D:250.19 (unattach Last Rites) -> D:253.06 (recall): four decisions in between (two chain-priority, the Zaun Warrens discard). At D:162.09 -> D:162.11, with no trigger pending, the recall is immediate.
- D11: D:003.01 `CHOSE: [P1] Mulligan 1 [Irresistible Faefolk]` (kind (b)).
- D13: 7 damage steps — D:126.07, D:162.04, D:190.07 (first three); the second player is asked with the first assignment already marked (e.g. idx=139 `Rengar,Trop(6M,7dmg,rdy,Asl3,DEF)`).
- D15: 3 sightings — D:085.03, D:187.03, D:272.03 (Last Rites attached at activation, no chain item).
- D37: 3 sightings — idx=106 (D:098), idx=153 (D:140), idx=315 (D:274): Rengar in hand and payable, a battlefield holding only enemy units, no `Play Rengar -> <that battlefield>` in the Neutral Open list. The same battlefields ARE offered at chain priority (idx=108, idx=320).
- D40: 2 sightings — D:126.14-126.15 and D:212.13-212.14 (Last Rites' conquer trigger and Zaun Warrens, same controller, no ordering asked).
- D51: 2 sightings of Kha'Zix's ability put on the chain with the condition false and resolving to nothing — D:155.03 -> D:157.02 (two defenders), D:232.05 -> D:234.02 (two defenders). The three with a lone enemy resolved in full (D:169.03-.05, D:190.03-.05, D:269.03-.05).
- Targon's Peak offering the opponent's runes (section 5 table: "judge ruling"): idx=102, D:093.06 — options [0]-[3] are P1's Body Runes 45, 46, 47 and Chaos Rune 50.
- Not seen although the cards were present: D75 (no opponent banished a zed card), D78 (Kennen only burned), D81 (Minefield not chosen), D63, D68, D6 (at idx=263, 265, 275 P1 held Onslaught and Rampage with the runes for them and neither was listed at chain priority), D7, D25, D44, D72.

### g19 teemo vs nasus — seen again

- D1: 9 play triggers queued and finalized as the cost is paid; visible twice — D:119.07 (Blitzcrank's "may" asked with
  the board showing it `rdy`, `exh` one decision later), D:210.02-212.00 (Curator's "may" finalized, then
  `[D:212.00] DECISION #231 (P2): pay [1][Calm] for Accelerate on Tasty Faefolk? [decline|pay] [2 options]` — a "yes" to Curator would have readied the runes
  that pay the Accelerate), D:033.02.
- D3: D:261.03-262.03 — Charm's destination (`MODE_PROMPT: Charm: move it to [Battlefield #0/Battlefield #1]`) asked
  after RESOLVE.
- D4: 21 sightings (the 14 Curator items of N2 not counted) — Dusk Rose Lab in every Beginning Step of the game, 16
  times (13 `MAY_NOT_OFFERED`, e.g. D:004.11, D:011.09, D:028.09; 3 `MAY_DECLINED`: D:110.03, D:153.03, D:284.03);
  Tideturner D:058.04; Blitzcrank D:120.03 (declined), D:166.07 (not offered); Curator declined D:211.03, D:251.03.
  A Reaction played in such a window: D:088.01, P1's Consult the Past in the Beginning Step of turn 6 with only the
  not-offered Dusk Rose Lab item on the chain (before that turn's Channel Phase).
- D5: 3 Focus decisions in combat — idx=109 (Temporal Breach offered, not taken); idx=148 (Temporal Breach and 16
  Singularity intents offered; Singularity played, D:135.05, kills Nasus, Guardian of Knowledge at the other
  battlefield); idx=160 (Temporal Breach played, D:145.05). Consistent with the known description: at the five
  non-combat showdowns no such spell was offered (e.g. idx=143, P1 with Temporal Breach payable: forced Pass Focus).
- D9: 6 sightings — D:037.03, D:100.01, D:105.03 (first three); also D:125.03, D:250.03 (all 7 Calm runes taken for
  the Empower, leaving the 5 Mind), D:268.03.
- D11: D:099.01 `CHOSE: [P1] HideCard Teemo, Strategist bf=1` (a); D:003.01 / D:004.01 mulligan `CHOSE` lines (b);
  D:100.00 `DECISION #106 (P1): Hide cost for Teemo, Strategist [2 options]` (c).
- D13: 3 sightings — D:102.05, D:151.06, D:264.06 (the defender is asked with the attacker's damage already marked:
  boards at idx=112, 170, 289).
- D40: 6 sightings — D:072.02-072.03, D:082.02-082.03, D:119.02-119.03 (first three); also D:166.02, D:191.05-191.06
  (Blitzcrank's hold ability and The Academy's), D:278.04-278.05. Order never asked.
- D53: D:225.07 — `KILL: Sprite (id=109, 3M, dmg=0)` straight after `── Beginning Step ──`, no chain item.
- D67: target after RESOLVE at D:039.03, D:107.03, D:270.03. Activation offered with nothing to choose (no Teemo unit
  on P1's board, the chosen champion in the trash since D:108.06) at idx=147, 202, 208, 247; not taken there.
- D72: D:155.04-157.05 — Hold BF#0 scored, Blitzcrank's hold ability given priority and resolved, then Hold BF#1.
- Not met in this game: D2, D6, D7, D14, D15, D21, D25 (the one facedown card was hidden and lost in the same turn),
  D35, D44, D48 (neither unit Temporal Breach re-played has Accelerate), D65 (no -[M] after Mel's Empower), D68,
  D69-D71.

## 6. Checked and found correct

Surprising but correct interactions, each with the rule or text, as the readers recorded them. `D:` and `idx=`
refer to the game of the heading.

### g01 irelia vs ornn

- Second player's first turn channels 3 runes — CR 487.7 / 488.7 — D:010.09-11. Rune deck with one rune left: one channeled — CR 315.3.b.1 — D:261.10.
- Each player chooses one of three battlefields — CR 488.5 (Match; the engine's default format is BestOf3) — D:001-002. Not determined: whether P2's choice is made without knowledge of P1's ("placed simultaneously").
- Mulligan: set aside, draw, recycle; deck back to 35 — CR 117 — D:003-004.
- Costs: every PAY_COST against the sheet and the runes moved (Defiant Dance 1E + chaos; Poro Snax 1E + calm; Ornn 5E + calm; Ride the Wind 2E + chaos; Eclipse 3E; Riven 3E + calm; Defy 1E + calm (x3); Rebuke 2E + 2 chaos; Aphelios 4E + calm; Akali 4E + calm; Forgefire Cape 4E + calm + mind; Charm 1E + calm (x2); Discipline 2E; Flash 2E; Tideturner 2E; Irelia 5E; Adaptatron 4E; Guardian Angel 2E; Hourglass 2E; Patched Porobot 2E) — all right.
- Fire Below the Mountain's [A] "Use only to play gear or use gear abilities": pays Guardian Angel's Equip [G] with no rune (`[D:171.03] PAY_ADD: P2 pays [0] + 1 power: exhausted 0, recycled 0`; CR 135.2.e.5.b: an added [A] pays Power of any Domain); is NOT used for the Deflect of Riven's ability with the [A] in the pool (D:226.03 then `[D:229.04] PAY_ADD: P2 pays [0] + 1 power (any): exhausted 0, recycled 1`); makes Poro Snax (calm Power) a legal offer at idx=279 with only Mind runes in play, while Guardian Angel (2E, one ready rune) is withheld; is gone at the next turn.
- Option lists: all 60 Main Phase / Focus / priority lists compared with hand, ready runes, board. Nothing unaffordable offered, nothing affordable missing, apart from the known items above. Points checked on purpose: spells with no legal target withheld (Rebuke / Ride the Wind / Charm / Eclipse on an empty board, turns 1-5); units playable only to Base or a controlled battlefield; no battlefield-to-battlefield Standard Move (no Ganking); Hide offered only with a controlled battlefield (idx=67, 143, 184, 228) and never without (idx=96, 119, 284); an attached Equipment's Equip not offered (CR 718.2); Equip not offered in showdowns; Akali, Silent not among Discipline's targets out of combat (idx=210) and not among Charm's (enemy spell — n/a, own unit).
- Rebuke offers only units at a battlefield (idx=106); Ornn, a champion, goes to its owner's hand — D:108.03.
- Discipline countered: no +2, no draw, both cards to trash — D:100.03. Defy on Eclipse / Flash / Defy is within "no more than [4] and no more than [A]".
- Blade Dancer triggers on Ride the Wind -> Adaptatron (D:052.02) and Discipline -> Tideturner (D:202.02), not on Discipline -> enemy Ornn (D:093.01) nor on Rebuke.
- Irelia, Fervent: +1 [M] when readied in the Awaken Phase after being exhausted (D:172.06, 5M that turn, 4M next turn), no trigger at the turn-14 Awaken when she was already ready, none when the opponent's Riven chose her (D:229.03); Riven's controller paid her Deflect — D:229.05.
- Riven, Shattered: 2 damage for one Equipment (D:231.04). Akali, Silent: +2 [M] on a move to a battlefield, this turn only (6M in the combat, 4M on turn 16). Aphelios, Exalted: one mode on attach, rune channeled exhausted (D:220.04). Adaptatron: gear chosen at finalization, killed, +1 buff (D:134.03-04). Eclipse: -4 this turn (3M -> 0M, 3M next turn), Predict private. Poro Snax: "Kill this" paid at activation (D:036.05), draw at resolution.
- Combat arithmetic: Veiled Temple 6 + 4 = 10 against Irelia 4 (exhausted defender still deals damage); the defender's 4 may go wholly to Akali (not lethal) or wholly to Riven (lethal), no split — idx=254. Guardian Angel: killed instead, Riven healed, exhausted, recalled, 3M in base (D:237.10-14). Targon's Peak: 7 against Adaptatron 4 (+1 buff counted). Attackers stay, defenders dead: Conquer, one point per battlefield (D:237.17, D:254.13). Damage is gone at end of turn (Aphelios 4 dmg, turn 16 board clean).
- Scores: Conquer D:058, 067, 124, 144, 211, 237, 254; Hold D:109.08, 174.06, 181.08, 197.08, 267.08-09. Final 5-8; the eighth point by Hold — CR 466.1.b not in play.
- Control of a battlefield is lost when its controller's last unit leaves in an Open State — CR 323.5 — idx=68, 115, 193.
- Standard Move of two units as one move (D:183, D:222-223), both exhausted.
- Ornn, Blacksmith's "may" is asked at resolution (it depends on the look). Not verifiable from the trace: after `[D:035.03] MAY_DECLINED` no line shows the look or "recycle the rest"; the class does recycle the four cards on a decline, but this game never reaches the bottom of P2's deck.

### g02 fiora vs ivern

- Second player's first Channel Phase gives 3 runes, the first player's 2 and no draw — D:004.11-004.14, D:008.09-008.11.
- Mulligan: set aside, draw, recycle; deck 35 after it for both — D:003, D:004. Final deck counts 27 / 29 match the draws (P1 6 turn draws + 2 by Sacrifice; P2 5 turn draws + 1 by Ivern; Predict put Elder Dragon back).
- Rune arithmetic for every payment: number of `EXHAUST_RUNE` = E of each `PAY_COST`; Power recycled in a Domain of the card (Riposte 2 Body D:041-042, Sett 1 Body D:068.01, Vi 1 Order D:136.01, Defy 1 Calm D:167.01); rune counts on the board after each recycle and each "channel 1 rune exhausted" (Grand Duelist D:074.04, Sacrifice D:118.07, Mobilize D:171.04 taking the last rune, 12/12+0).
- Trusty Ramhound has +1 [M] in the base with Azir (base counts as "here") and loses it alone at the battlefield — idx=19 `Trusty Ramhound(3M,rdy)` -> idx=20 `(2M,exh)`; 3M again with the second Ramhound (idx=66); CR 464.2.b treats "here" as possibly a Base.
- Vilemaw's Lair: no move to base offered for the ready Ramhounds there — idx=56, 100, 110.
- Riposte: counters Discipline, +[M] equal to its Energy cost (2) -> Fiora 5M this turn; back to 3M next turn — D:044.03-044.04, SNAP 107.
- Combat D:052-054: Azir 4 vs Fiora 5 (Riposte bonus, exhausted defender still deals damage); Azir dies, Fiora keeps 4 damage until end of turn. Combat D:178-181: Sett 4 vs Affectionate Poro 3 + Ivern 6 (Divining Shells +2 put on the enemy Ivern by P1) = 9; damage options are Poro 3 + Ivern 1, or Ivern 4; Sett and Poro die, defender keeps Risen Altar. Affectionate Poro's "combat ends" draw does not happen (it was dealt damage and died).
- Fiora, Worthy: "pay [Y]" not offered with only Body runes in play (D:046.13); offered with an Order rune in play, an exhausted Order rune recycled, Fiora readied (D:082.03-086.03; board 5 runes -> 4, Order 2 -> 1).
- Grand Duelist: legend exhausted when the may is accepted, rune channeled exhausted at resolution (D:072.03-074.04); not offered while exhausted (D:082.06); readied in P1's next Awaken Phase.
- Shepherd's Heirloom: 1 XP on play (D:029.03), Equip offered only with XP (idx=34, 70, 85-89; not at idx=111 with 0 XP), +2 [M], moves with Fiora, unattached to the base when Fiora dies there (D:115.05); re-equipped to Poppy for 1 of her 3 XP (D:193).
- Sacrifice: offered only with a Mighty friendly unit (not at idx=26-70; offered idx=111, 118, 120, 122), unit killed as the cost before `PAY_COST`, draw 2 + channel 1 exhausted — D:114-118.
- Rampage: friendly and enemy unit chosen before the cost; enemy unit in the base is a legal choice (idx=113); without the additional cost Fiora 5 and Ramhound 3 deal damage to each other — D:111.03-111.05. Defy is offered against it (cost [3], no Power) — idx=119.
- Sett, Brawler: buffed on play (5M), buff spent as the cost at activation, +4 this turn, 4M next turn — D:070.03, D:078.04, D:080.03, SNAP 142.
- Battlefield control: Risen Altar becomes uncontrolled when Sett and Fiora both move to base (one Standard Move, second unit added by prompt) — D:113, CR 190.4.c / 323.5; P2's later move there opens a non-combat showdown and conquers (D:141).
- Standard Moves from a battlefield go to base only (no battlefield-to-battlefield without Ganking) — idx=70, 111.
- Green Father: exhausted at finalization when accepted, Lair goes to P2's Banishment, units stay, Brush gives Astral Heron (Bird) and Trusty Ramhound (Dog) +1 [M] at once (8M / 4M), Vi none — D:120-122, SNAP 195, 212; not offered while exhausted (D:141.09). Brush's own "when you score here" fires on the next hold there (D:148.05), not on the hold that created it.
- Astral Heron: triggers on its own play to a battlefield as the first card of the turn (D:099.02; accepted by earlier readers, decks document line 560); on turn 8 the new discount is not applied to Vi, played while the trigger was still on the chain (`[D:130.04] PAY_COST: Vi, Peacekeeper E=5 P=1`), and goes to the next card after it resolved (Affectionate Poro E=1).
- Ivern, Nurturer: looks at 3, the choice lists the three units and "None", Vi drawn (not a Bird/Cat/Dog/Poro, so no buff), deck 31 -> 30 — D:128-129.
- Vi, Peacekeeper: [Ambush] play offered at chain priority to the Brush (P2 units there) only, enters exhausted — idx=138, SNAP 212.
- Hidden Blade: Hide offered at both battlefields P2 controls for [A] (one rune recycled), not playable the turn it is hidden, offered next turn at chain priority for no cost with choices limited to units at its battlefield (all P2's own) — D:154, idx=174; CR 811.1.b, 811.1.d.2, 811.6.
- Poppy, Paragon: P2 at 7 of 8 -> readied, 3 XP — D:189.03-189.04.
- Mobilize, Divining Shells [Vision] (private peek, recycle), Discipline's target list (any unit) — D:171, D:176-177, idx=37.
- Winning point by Hold at 8 ends the game at once, before the second hold — D:194.08.

### g03 pyke vs kaisa

- Second player's first Channel Phase: 3 runes — CR 487.7 — D:011.09-011.11. Rune Deck with 1 rune: channels 1
  — CR 315.3.b.1 — D:265.09.
- Re-taking a battlefield already scored this turn is not a Conquer: P2 held Zaun Warrens (D:201.08), walked
  off it, moved Plundering Poro back in; control re-established (idx=232 `[Ctrl:P2]`), no `CONQUER`, no point,
  no Plundering Poro / Zaun Warrens trigger — CR 348.2.a.1, 472, 473.2.c — D:211-216.
- Winning point by Conquer refused with three battlefields on the board (Baron Pit counted), card drawn instead
  — CR 473.1.b.1 — `[D:284.04] SCORE_BLOCKED: P2 winning point blocked (not all BFs scored), draw 1 instead`.
  The 8th point by Hold wins — D:321.08.
- Control is lost when the last unit leaves (Void Gate after Falling Star killed Treasure Hunter idx=97; Zaun
  Warrens idx=225; Baron Pit idx=355) — CR 323.5.
- Baron Nashor: 10 Energy + 3 Chaos Power (three Chaos runes recycled, exhausted ones allowed), Baron Pit
  created, he enters there exhausted, the battlefield is Contested by a unit "being played to" it, showdown,
  Conquer +1 — CR 190.3.a.1, 440.4.b — D:246-259. "Other friendly units have +2 [M]": Fizz 5M, Tideturner 4M
  (damage step totals 12+4=16 at D:274.06, D:289.08; Fizz survives 4 damage at 5M, D:261.09). "Can't be chosen
  by enemy spells": Stupefy's five options at idx=315 leave Baron out; his own controller's Cleave and
  Bloodharbor Ripper list him (idx=328). "Units can move here from anywhere": Fizz at Zaun Warrens is offered
  `-> Baron Pit` and not `-> Void Gate` (idx=328).
- Combat Cleanup heals: Noxus Hopeful (2 damage, idx=207 -> 208), Tideturner (idx=302 -> 303) — CR 468.1.a.1,
  143.3.b.2. End-of-turn heal: Pyke idx=65 -> SNAP 106, Noxus Hopeful SNAP 166 -> 170.
- [Legion]: Noxus Hopeful offered with 2 ready runes after two other cards that turn (idx=68) and not offered
  with 1 ready rune (idx=26); costs 4 when played first (D:094.04).
- Void Gate Bonus Damage: Hextech Ray 3+1 (D:077.03-077.04), Falling Star's second hit 3+1 on a unit there and
  3 on a unit in a base (D:089.03-089.06).
- Daughter of the Void's Power pays a spell's Power and nothing else: Rocket Barrage `E=4 P=1` with four
  `EXHAUST_RUNE` and no recycle (D:174.04-178.01, runes stay 7 in play); Ferrous Forerunner's Power is paid by
  a recycle with that Power still floating (D:282.01).
- Bellows Breath's repeated option lines ("@ Treasure Hunter" x4, "@ Pyke" x2 at idx=53) are the subsets of the
  three units in P1's base rendered by their first unit (4+2+1); the chosen one was `tgt=[Pyke, Dockside
  Butcher]` alone (D:050.01) and dealt 1 to 1 unit. Not a duplicate-option defect.
- Hidden: Tideturner hidden at a controlled battlefield for [A] on P1's turn (D:260), not at Zaun Warrens (not
  controlled); played from facedown for 0 at Focus in the combat there on the opponent's next turn (D:267.03),
  joins as a defender (DAMAGE_STEP lists it, D:274.03).
- Hextech Ray / Void Seeker / Gust offer only units at battlefields (idx=53, 78, 191, 253); Gust only 3 [M] or
  less (Kai'Sa 4M left out, idx=253); Rocket Barrage only units in a base and only the damage mode with no gear
  on the board (D:172.05, D:173.04).
- Hard Bargain's "unless its controller pays [2]": controller had 1 ready rune, not asked, countered (D:194.03).
- Stupefy / Smoke Screen floors: Treasure Hunter 1M stays 1 (D:030.03), Noxus Hopeful 4 -> 1 (D:164.03), Fizz
  5 -> 4 (D:288.03); all gone next turn.
- Fizz, Trickster: only spells with Energy <= 3 listed; Power still charged (Void Seeker `E=0 P=1`, D:232.03);
  the spell is recycled, not trashed (deck +1 each time).
- Battlefield setup is an agent decision (D:001, D:002). CR 488.5 (Match) says "selects", CR 487.5 (Duel) says
  "randomly selects"; which mode the engine plays is not stated in the log, so not graded.

### g04 rengar vs vi

- Winning point by Conquer: P2 at 6, turn 17: `[D:232.08] SCORE: P2 +1 via Hold BF#0 -> 7`, then Rumble moves to the empty, uncontrolled Forge of the Fluft, `[D:233.06] SCORE: P2 +1 via Conquer -> 8`. Both battlefields were scored that turn (one held, one conquered) — CR 473.1.b.1 "If the player has Scored every Battlefield this turn, that player Gains the Final Point". Score ledger recomputed: P2 conquer T5, holds T7/T9/T11, conquer T13, conquer T15, hold + conquer T17 = 8; P1 conquer T10, hold T12, conquer T12, hold T14 = 4.
- Control lapses when the last unit leaves or dies: Seat of Power `[Ctrl:P1]` -> `[Ctrl:--]` after P1 moves Inferna and Darius to base (D:195.01-195.02, idx=217), Forge of the Fluft `[Ctrl:P2]` -> `[Ctrl:--]` after Rampage kills Vi (D:206.06); no hold for either player afterwards and Seat of Power stops being a play location (idx=217 lists `-> Base` only) — CR 323.5, 468.5.b.
- Units moving or being played onto an empty uncontrolled battlefield open a non-combat showdown and conquer when both pass: D:046.03, D:116.05 (Rengar put there by Thrill of the Hunt; staged only after the chain emptied), D:226.04, D:233.04.
- Damage healed at the end of each combat (Honest Broker `3dmg` at idx=79, none at idx=80; second combat `was 0/7M`) — CR 468.1.a.1 "Heal all Units".
- Stunned defender deals nothing: `[D:189.05] DAMAGE_STEP: BF#1 ATK=[Vi, Peacekeeper(5M), Gem Jammer(4M)] DEF=[Pit Rookie(2M,STUNNED)]`, only P2 assigns (9) — CR 424.1.b. Excess 7 >= 3, so Piltover Enforcer triggers on that conquer (D:189.11) and does not on the two conquers without combat (D:050, D:226).
- Combat arithmetic, every combat: Inferna 1+2 Assault = 3 as attacker (D:071.04); defenders 7+5 = 12 (Punch First on Honest Broker 2+5, Gem Jammer 2+2 Long Sword+1 Pridestalker); Darius 5+2 = 7 vs 4+2 = 6 with Rengar, Unseen's Assault 2 not counted on defence (D:173.03-173.08); Rengar 6 vs Vi 5 + Gem Jammer 4 = 9 (D:197.03). Assignment options always lethal-first with the remainder on the next unit.
- Darius, Trifarian: no trigger on his own play (first card of turn 12), trigger on Pit Rookie (second card): `[D:169.04] DARIUS TRIFARIAN: second card played, +2M and ready`; 7M that turn, 5M at the next turn's board.
- Rampage with the additional cost: First Mate 3+2 = 5, Vi 5; each deals 5 to the other, both die (D:206.03-206.07). Body Power for [O]: runes 8 -> 7 with a Body rune gone (idx=229 -> 230).
- Seat of Power: draw 0 with no other battlefield controlled (D:052.03, D:228.03), draw 1 when P1 also controlled Forge of the Fluft (D:177.03-177.05).
- Sacrifice: only the two units with 5 Might offered (Brynhir, Vi; not Gem Jammer 4M with the sword, not Rengar, Unseen 4M); kill, then [1]; draw 2 and one rune channeled exhausted (D:143.05-147.08); rune deck 3 -> 2.
- Thrill of the Hunt replay triggers Pridestalker ("When you play a unit") and the unit enters exhausted (D:111.04-111.08, idx=124 `Rengar,Trop(6M,exh)`); the banished-and-replayed Rengar is a new object, the +1 [M] goes on after.
- Deflect: P1 pays [A] each time it chooses Rengar, Unseen with an ability (Pridestalker D:097.05, First Mate D:121.05), paid as the target is chosen — CR 809.1.c.
- First Mate / Pit Rookie lists leave out the unit itself ("another"): D:120.05 (7 units, no First Mate), D:163.06 (4 friendly units, no Pit Rookie). Pridestalker and First Mate may choose enemy units (text says "a unit" / "another unit").
- Kennen from facedown: hidden turn 7 for [A] (D:081.05), not playable that turn, played turn 10 as a Reaction for [0] to the battlefield he was hidden at — CR 811.1.b, 811.1.d.1, 811.6. (His target list is N1.)
- Long Sword's Equip is not offered while it is attached (turn 13 idx=199/202/214 on Gem Jammer; turn 17 idx=260 on Kai'Sa) and is offered once it lies unattached in base (idx=233, 245, 250) — CR 718.2 "While in this state, the card's printed Rules Text is Inactive". Recalled when its bearer died at a battlefield (D:197.11-197.13, CR 323.6); stayed in base when Kennen died in base (D:213.04).
- Forge of the Fluft: while P2 controlled it with an Equipment on the board the legend ability is offered (`[11] Activate Piltover Enforcer #783`, idx=214); not offered to P1 on turns 10-12 (no Equipment), nor to P2 on turn 15 (battlefield lost).
- Units at a battlefield are offered `-> Base` only; units in base both battlefields; exhausted units none. Units and gear playable only to base and controlled battlefields (e.g. idx=199: all four units `-> Base` only with no battlefield controlled). Inferna's Ambush play offered at Focus in her own side's combat (idx=69) and not in Open main-phase lists.
- Power always taken from the card's Domain: Sabotage/Rengar/Punch First Body, Darius/Long Sword/Rengar, Unseen Fury, Vi Order (recycle prompts list only that Domain's runes, ready or exhausted); Thrill of the Hunt (Fury/Body signature) lists both (idx=114).
- Turn structure: P2 channels 3 on its first turn (CR 487.7; P1's missing first draw is N2); 2 runes per Channel Phase after; P2's rune deck empty on turn 15 (nothing channeled, D:207.08-207.09) and refilled by recycles afterwards; "this turn" Might gone at each next board (Honest Broker 3M -> 2M, Gem Jammer 5M -> 4M, Darius 7M -> 5M); Gem Jammer's Ganking gone at turn end. Mulligan and Sabotage deck counts (P2 35 -> 36 with Deathgrip on the bottom).
- Brynhir Thundersong resolves through priority and P1 is offered nothing but passes for the rest of turn 11.

### g05 lucian vs ezreal

- Second player's first Channel Phase: 3 runes — CR 487.7 — D:015.06-08. Every other Channel Phase 2; P2's rune deck reaches 12/12+0 on turn 15 (D:252.08-09).
- Mulligan: set aside, draw, then recycle; deck counts 39 -> 35 both sides — D:003-004.
- Sabotage: reveals the whole hand, offers only the two non-unit cards (Stupefy, Deadly Flourish), recycles the chosen one (P2 deck 35 -> 36) — D:011.03-012.03.
- Costs, every play recomputed: Energy = runes exhausted, Power recycled from a rune of the card's Domain (Sabotage Body D:009.01; Fizz / Ezreal / Star-Crossed Chaos D:036.01, D:204.01, D:090.01; Long Sword Fury D:095.01; Rengar Body D:141.01, D:184.01; Watcher and Bellows Breath Mind D:155.01, D:266.01; Relentless Pursuit Fury then Body then Body — it has both Domains). Long Sword is absent from P1's lists while P1 has no Fury rune in play (idx=7, 23) and from priority lists while P1 lacks 2 ready runes.
- Battlefield control is lost when the last unit leaves or dies in an open state — CR 190.4.c / 323.5 / 468.5.b — D:066 (both units die, Forge goes to `[Ctrl:--]`), D:117 (Kai'Sa moved away by Relentless Pursuit), D:192.03; hence no hold for P1 on turns 6, 8, 10.
- Lucian "The first time I conquer each turn, ready me" — D:052.07-054.03; then only `Move ... -> Base` is offered (no battlefield-to-battlefield Standard Move without Ganking, CR 144.4.c).
- Combat arithmetic: Fizz 3 vs Lucian 3+2 (defender, so no Assault) with 3 damage already marked: both die (D:066.05-11); Kai'Sa 4+2+2+2 Assault = 10 vs Watcher 7 (D:186.05-10; Assault values sum, CR 807.2); stunned Rengar assigns nothing, takes 4, both sides survive, attacker recalled, everything healed (CR 468.1.a.1-2; D:191.06-09); Rengar 6 vs Vex 4 (D:193.05-12); Ezreal 3 vs Kai'Sa 4 (1 marked) + Rengar 6+2: P1 assigns 12, Kai'Sa dies to exactly lethal 3 (D:271.05-11), P1 keeps the battlefield.
- Deadly Flourish: 3 to an enemy unit, Gold token exhausted when it dies later that turn in combat — D:065.03-068.04.
- Turn to Dust on the player's own Gold: [Temporary], killed at P2's next Beginning Step — D:073.03, D:077.07.
- Long Sword ([Quick-Draw], so [Reaction], CR 819.1.b) offered and played at chain priority on the opponent's turn with no unit to attach to — D:075 (offered), D:092.
- Accelerate: Kai'Sa [1][R] paid with an Energy and a Fury rune and she enters ready and moves the same turn (D:106-107); Watcher [1][B] (D:156); Legion Rearguard declined (D:222), and not asked on the second Rearguard when P1 had no ready rune left (D:242-244).
- Thousand-Tailed Watcher: enemy units -3 [M] this turn (6 -> 3 twice), gone next turn — D:158.03-04, board of turn 12.
- Wages of Pain as an [Action] at Focus of a non-combat showdown, on the only unit at a battlefield (P2's own Watcher); Gold exhausted — D:160.03-165.06. Not offered (play or hide) earlier that turn: no unit at a battlefield and no controlled battlefield.
- Relentless Pursuit: a move by an effect goes battlefield to battlefield (Forge -> Frozen Fortress, D:117.04) and does not exhaust (Kai'Sa arrives `rdy` at D:178, then makes a Standard Move to base at D:192.03); the granted conquer ability fires only for conquers after it resolved; an Equipment attached to another unit may be chosen and comes off that unit (CR 435.1.f / 435.4: D:238.05-06, Trinity Force from Kai'Sa at Frozen Fortress to Rengar at Forge); not offered while P1 has no unit (idx=83, 107).
- Rengar by [Ambush] as a [Reaction] to battlefields with enemy units only: in the Beginning Step with the Frozen Fortress ability on the chain (idx=183), on his own spell (idx=189), and at Focus of a combat at the other battlefield (idx=194, D:178.08); the second combat is then fought with him as attacker (D:186.15). Matches the decks document's checked list.
- Vex, Apathetic at a battlefield: stuns the played Rengar; he deals no combat damage; the recall to base is not a move ("They can't move it this turn") — D:186.03, D:191.06-09.
- Frozen Fortress: 1 to each unit there in each player's Beginning Step, before scoring — D:169.03, D:199.03, D:215.03 (then `[D:215.06] SCORE: P1 +1 via Hold BF#1 -> 6`), D:252.03-04, D:274.03.
- Rune Pool: the [A] from Gold added in P2's Beginning Step (D:197.03) is not there for Ezreal's Power in the Main Phase (P2 is asked to recycle a rune, D:204.00) — CR 316.3.
- Ezreal, Prodigy: discard 1 then draw 2 (D:207.03-06); "Optional additional costs you pay cost [1] or [A] less" applied to Bellows Breath's Repeat [1][B]: choice offered, `[D:268.01] PAY_ADD: P2 pays [1] + 0 power: exhausted 1, recycled 0`.
- Fizz, Trickster: list = spells in trash with Energy cost <= 3 (Deadly Flourish, 4, left out) — D:258.04; Acceptable Losses replayed for nothing and recycled — D:040.03-042.02.
- Gust from hand: only legal unit (Legion Rearguard, 2 [M], at a battlefield) returned to hand — D:229-233. Not offered in windows where no unit at a battlefield had 3 [M] or less (D:215).
- Noxus Hopeful [Legion]: with 2 ready runes it is offered after another card was played that turn (idx=271), while First Mate and Skyfall of Areion (3) are not.
- Forge of the Fluft: the legend ability is offered only while P1 controls the battlefield (idx=118, 215, 271, 274; absent at idx=133, 243-251), exhausts Purifier, chooses Equipment and unit at finalization, may take an attached Equipment — D:245.02-249.04. Not offered to P2 while P2 controlled Forge (no Equipment).
- Units to a controlled battlefield (CR 355.2.a): `-> Frozen Fortress` from idx=243, `-> Forge of the Fluft` added after the conquer (idx=271).
- Scoring: conquers D:052.06, D:107.06, D:117.07, D:165.08, D:166.06, D:186.12, D:193.12, D:238.10; holds D:215.06, D:274.06. One score per battlefield per turn held throughout (the recall at D:191.09 gave no score; the later win there did). Final point by Hold (7 -> 8), which needs nothing else — D:274.06.

### g06 kennen vs diana

- Printed Flow, offer: Lightning Rush ([Flow] [2][A]) is offered from the trash exactly when [2] + one more rune or floating Power is there — idx=92 (4 ready), idx=95 (2 ready of 5), idx=98 (2 ready + 1 floating), not at idx=101 / idx=153 / idx=165 (0 ready), idx=333 (1 ready, second copy burned at D:300.04). Up from the Deep ([Flow] [3]) first offered idx=199. Neither is offered at a chain-priority decision or on P2's turn (no [Reaction]) — CR 829.1.b.2. Spells without Flow in the trash (Ride the Wind, Switcheroo, Gust) are never offered.
- Printed Flow, cost: `[D:185.04] PAY_ADD: P1 pays [3] + 0 power: exhausted 3, recycled 0` (runes 8 ready -> 5); `[D:210.04] PAY_ADD: P1 pays [2] + 1 power (any): exhausted 2, recycled 0` — the [A] is taken from the floating Chaos Power of Seal of Discord (D:195.03), runes 3 ready -> 1, none recycled — CR 829.1.c.1.
- Printed Flow, after the play: both spells go to Banishment, not the trash (board idx=204 `P1 | Banish: Up from the Deep`, idx=232 `P1 | Banish: Up from the Deep  Lightning Rush`), and are not offered again — CR 829.1.b.
- Flow play and "plays a spell" watchers: Abandoned Hall triggers after the Flow spell resolved (D:189.05, D:213.07) — CR 420.4.a.
- Lightning Rush: three looked at, one drawn, two to trash, from hand (D:083, deck 30 -> 27) and by Flow (D:213, deck 22 -> 19); the option list has `Draw none`.
- Up from the Deep: two 1 [M] Tentacles, location asked per token (Base / Zaun Warrens, the controlled battlefield), both enter exhausted; each is a played unit for Vex at Abandoned Hall (`[D:197.03] EFFECT: stun Tentacle (id=110)`, `[D:199.03] EFFECT: stun Tentacle (id=109)`) — CR 350.2. The legend does not trigger for the tokens (CR 185 "Tokens are not cards").
- Burn as a cost at finalization: Shadow Order Disciple's "you may [Burn 1] to give me +1 [M]" burns when the may is accepted (D:030.04, D:150.04, D:300.04), +1 at resolution, gone next turn. Kennen's Burn 2 at D:146.03-04, deck 26 -> 24.
- Tail-Cloaked Matriarch: Empower `[2][P]` = 2 exhausted + 1 Chaos rune recycled (D:288.03); the trash list at idx=315 is Tornado Warrior, Ezreal, Prodigy, two Shadow Order Disciples, Kennen — the two Matriarchs ([4]) and the two Rhasa (printed [10]; its own discount is not its Base Cost, CR 356.1.c) are left out; Ezreal enters the base exhausted, cost ignored. Not offered again while Empowered (idx=322).
- Ezreal, Prodigy with an empty hand: "discard 1, then draw 2" draws 2 (D:298.03-05).
- Empower on an Empowered legend does nothing (`[D:224.02] RESOLVE: Heart of the Tempest (chain_item=59, ability=1, spell=0)` with no effect line after it); the status lasts over turns until spent (Empowered at D:201 turn 10, used at D:272 turn 12).
- Hwei, Brooding Painter: "Then, do the following ..." as a separate chain item (`[D:239.04] TRIGGER: Hwei, Brooding Painter (P2) — reflexive trigger goes on the chain`) — CR 388.1.b ("Do the following").
- Scorn of the Moon's showdown-only Energy: unusable in the Main Phase (idx=44, idx=192: nothing playable offered with it), used in the combat showdown of turn 11 — Rebuke `E=2 P=2` exhausts one rune only (D:252.01).
- Hard Bargain's Repeat [2] is not offered when it cannot be paid (D:054-057: one ready rune left).
- Gust lists only units at a battlefield with 3 [M] or less (idx=235, idx=325: Tail-Cloaked Matriarch 4M and base units absent); Rebuke lists every unit at a battlefield, none in a base (idx=270). A Gusted token ceases to exist (D:220.03, D:305.03).
- Combat arithmetic: D:154 (2+3 = 5 on Fizz 3M; 3 back), D:183 (4+4 = 8; Ravenbloom Student at 4M this turn takes 2 and lives), D:209 (Kennen 4 split 2/2 kills Ravenbloom Student; 4+2 = 6 kills Kennen), D:258 (Hwei 4M after Stupefy kills the Matriarch; 1+4+1 = 6 back, the Tentacle's stun of turn 10 gone). Kennen dies to the trash, not the Champion Zone.
- Control of Abandoned Hall drops when Rebuke returns the last unit there (board idx=278 `[Ctrl:--]`), no hold for P2 on turn 13.
- Channel: 2 per turn, 3 for P2's first turn (D:008.09-11); P1 skips the first draw.
- Vex stuns only while at a battlefield (turn 10: three stuns; turns 8 and 12 from the base: none — the spurious chain items are D33).

### g07 yasuao vs khazix

- Second player channels 3 on the first turn, 2 otherwise — CR 487.7, 315.3.b — D:005.09-11; every later Channel Phase 2 runes.
- Unforgiven moves an exhausted unit (Poro had just been played) from base to an open battlefield; non-combat showdown, conquer, +1 — the ability moves, it is not a Standard Move (CR 451) — D:013-016. Cost [2] + exhaust the legend: 2 runes exhausted, legend `[EXHAUSTED]` at idx=14.
- Seat of Power "draw 1 for each other battlefield you ... control": 0 for P1 at D:018.03 and D:109.03 (controls no other), 1 for P2 at D:067.03 (controls Sandswept Tomb).
- Hide for [A]: one rune recycled, exhausted or not, only at a battlefield the player controls, on the own turn in an Open State — CR 811.1.b — D:019, D:116; not offered at turn 2 idx=10/13 (no battlefield controlled); at idx=124 only Sandswept Tomb is offered (Seat of Power just lost).
- A facedown card is playable only from the next player's turn on (CR 811.6): Tideturner hidden D:019 is first offered at idx=28 (P2's turn 3); Tornado Warrior hidden D:116 is not offered again on turn 9 and is played on P1's turn 10 (idx=137), for no cost, to its battlefield, exhausted.
- The facedown Tideturner goes to the trash when its controller's last unit leaves the battlefield — CR 107.3.d, 323.5, 323.6.a — `[D:043.05] HIDDEN: Tideturner removed from facedown (control lost)`; P1's trash at idx=48: `Ride the Wind  Tideturner`.
- Control is lost when the last unit leaves in an Open State (CR 323.5): Seat of Power D:043 (Ride the Wind to base), D:130 (Fizz' Standard Move), Sandswept Tomb D:143 (three units moved to base); after the combat with no survivor Sandswept Tomb is uncontrolled (CR 468.5.b) — D:044.
- Combat D:044: Lonely Poro 2 vs Traveling Merchant 2 (exhausted defenders still fight), both assigned lethal, both die, nobody conquers, no XP for Voidreaver ("When you win a combat"). Poro's [Deathknell]: no other friendly unit there -> draw 1 (D:046.03). Not the D80 case.
- Traveling Merchant: the discard is chosen as the ability resolves (an effect, not a target), then draw 1 — D:026-027; the ability resolves before the showdown at the battlefield it moved to opens (CR 344: a showdown begins in a cleanup in a Neutral Open State).
- Non-combat showdown, P2 with Focus: Punch First ([Action]) offered, Up from the Deep (no keyword, 3E affordable: 3 ready runes) not offered — CR 343.1.a — idx=30.
- Punch First 1E + 2 Body Power: one rune exhausted, two Body runes recycled (one of them exhausted) — D:030-032; +5 -> 7M, gone next turn (board turn 4: 2M).
- Shadow Order Disciple: the "may" and the Burn (a cost in the leading "you may ... to ...", CR 383.3.b) at finalization, top card of the Main Deck to the trash (deck 31 -> 30), +1 at resolution — D:053-056.
- Void Assault is not offered while the opponent has no unit on the board (turn 5 idx=56, turn 7 idx=69: P1 had none) and is offered once Fizz is there (idx=98) — a legal choice for every target (the D8 rule).
- Fizz' trigger and Vex' trigger on the same play: the turn player's (Fizz) goes on the chain first, Vex' on top and resolves first — CR 383.3.d.1 — chain at idx=88 `[top] Vex, Apathetic (P2)  [bot] Fizz, Trickster (P1)`.
- Vex, Apathetic at a battlefield: stuns the unit just played and "They can't move it this turn" — D:077.03. Ride the Wind (replayed by Fizz) on that unit: the move is refused, the unit is still readied — CR 359.3.e.14.b, example "Ride the Wind ... the move instruction will be negated on resolution ... but the ready instruction will still execute" — `[D:084.04] MOVE_BLOCKED: Fizz, Trickster can't be moved this turn`, board idx=97 `Fizz, Trickster(3M,rdy)` in base. (The next line `[D:084.05] RIDE THE WIND: moved + readied Fizz, Trickster` is wrong as a log text only.) No Standard Move is offered for the ready Fizz at idx=97. `Activate Unforgiven #256 @ Fizz, Trickster` is offered there although the move would be refused: not graded — I found no rule that forbids activating an ability whose only effect will be negated.
- Fizz, Trickster: Ride the Wind (Energy 2 <= 3) from the trash for its Power only (`[D:079.03] PAY_COST: Ride the Wind E=0 P=1`, one Chaos rune recycled), its unit chosen as it is put on the chain (`[D:081.01] TARGET (finalize): Ride the Wind -> Fizz, Trickster`: not the D32 case), then recycled: deck 30 -> 31 after it resolved, trash without it (idx=97). Not determined: the spell to play is picked in the trigger's finalize pass (D:074.04), before the opponent's priority; the text has no "choose", so by my reading that pick belongs to the resolution — I did not find a rule that says so in terms.
- Battlefield-to-battlefield moves by an effect without [Ganking] (Ride the Wind's `Base/Battlefield #1` for Poro at BF#0, Void Assault moving Vex BF#0 -> BF#1): CR 451.1 — only the Standard Move is limited (CR 144.4).
- Void Assault hands the opponent a conquer on the caster's own turn: Vex leaves Seat of Power, Fizz arrives alone, P2 loses control in the cleanup, showdown, P1 conquers (+1, 2 -> 3) and its Seat of Power trigger draws 0 — CR 452, 323.5, 344.2 — D:107-109. (Who gets Focus first there: N2.)
- Hard Bargain's [Repeat] [2] not offered with 1 ready rune left after its cost — D:097-099.
- Discipline may choose an enemy unit ("a unit"); +2 this turn (Zed 5 -> 7, back to 5 on turn 11), draw 1 for the caster — D:119, D:129.
- Tornado Warrior from facedown: "something here" = the three P2 units at Sandswept Tomb, itself included (idx=141).
- Star-Crossed: one friendly and one enemy unit chosen before the cost is paid, both to their owners' hands; Zed and Fizz keep nothing (hand costs printed) — D:147-155. No Sandswept Tomb discount: none of the chosen units was there (3E + 1 Chaos paid). Same for Void Assault at D:092 (Vex was at Seat of Power) and Punch First at D:029 (unit in base).
- Standard Move of several units as one action, same destination, all exhausted, no extra cost — CR 144.3 — D:110-112, D:141-143, D:156-157; only units without a blocking status and from a legal origin are offered.
- Scoring: holds only for battlefields controlled at the Scoring Step (D:035.08, D:058.08, D:085.08-09, D:117.08, D:140.09); every score change recomputed: P1 1 (conquer T2), 2 (hold T4), 3 (conquer on P2's T9), 4 (hold T10); P2 1, 2 (conquers T3, T5), 3 (hold T7), 4 (conquer T7), 5, 6 (holds T9), 7 (hold T11), 8 (conquer T11).
- Winning point by Conquer: P2 had scored Sandswept Tomb (Hold, D:140.09) and conquered Seat of Power in the same turn — both battlefields scored that turn — D:160.04-05. That P2 had left Sandswept Tomb before the conquer does not undo the score.
- Deck and hand counts: P1 39 -> 35 (opening, mulligan 1) -> 29 at the end with one recycle (Ride the Wind) and 7 draws; P2 39 -> 35 -> 26 with 6 turn draws, Merchant 1, Seat of Power 1, Burn 1. Mulligan: set aside, draw, recycle — the replacement cannot be the card set aside (P1 drew the other Irelia, id=39).
- "This turn" effects and damage gone at the Expiration Step; runes and units ready only in their controller's Awaken Phase (P1's runes spent on P2's turn 9 stay exhausted until turn 10).

### g08 mel vs vex

- Ravenbloom Student gets +1 once, not twice, when P1 plays Shock Blast and then Rebuttal: Rebuttal was countered, and a play trigger triggers on the card's resolution (CR 420.4.a, 420.4.a.1) — D:064-D:076 (`[D:074.05]` after `[D:074.02] RESOLVE: Shock Blast`). Same timing at D:304.06 (Stupefy) and D:333.05 (Dredge Up).
- Retaking a battlefield already scored this turn gives control but no Conquer and no point: P2 held Bandle Tree (D:107.08), left it (control lost in the Open State, CR 323.5), moved three units back: showdown, no CONQUER line, `[Ctrl:P2]` at idx=149 — CR 472 "A player may only **Score**, from either method, once per **Battlefield** per turn." Same for P1 at Rockfall Path on turn 14 (hold D:171.08, emptied by Star-Crossed D:190.03, Mel back D:204.03-D:205.00).
- Control lapses when the last unit leaves in an Open State (CR 323.5) — idx=144 (Steel Paws to base), idx=208 (Star-Crossed), idx=244 (Heron to base); play locations and Hide options follow at once (idx=244: Base only, no Hide).
- Winning point by Conquer: `[D:328.05] SCORE_BLOCKED: P1 winning point blocked (not all BFs scored), draw 1 instead` and `[D:348.05]` for P2 — each side conquered one battlefield at 7 points without having scored the other that turn; one card drawn (CR 473.1.b.1). The game ends on a Hold: `[D:362.08] SCORE: P1 +1 via Hold BF#0 -> 8`.
- Soul's Reflection + Mel: `[D:208.03] EFFECT: give Astral Heron -3 [M] this turn -> 4M` — -2 printed, -1 from Mel being Empowered (empowered D:181.03) on a unit the ability chooses. P2's Switcheroo giving Mel -2 (D:233.03) gets no extra (not Mel's controller's spell). Only units at a battlefield were offered (idx=227: Mel, Astral Heron). Cost (disempower, exhaust) before the target prompt: already in the decks document's checked list.
- Soul's Reflection triggers on Mel's Empower (D:181.04) and not on P2's empowers (Tornado Warrior D:058.04, Kharox D:354.03); it is offered only while Empowered (idx=208-226) and "Activate Mel" disappears once Mel is Empowered (idx=208 on).
- Shock Blast paid in full, [3]+1 Power, at D:059.02: the Empowered object was P2's Tornado Warrior, not something P1 controls.
- Tideturner's swap list holds only units at another location: `[D:141.04] TGT_PROMPT: Tideturner: swap with which unit [Mel, Newly Awakened]` with the other Tideturner in the same base; the swap puts the new Tideturner at Rockfall Path as a move, not a play (D:144.03).
- Astral Heron on the opponent's turn: P2's Rebuke in P1's showdown is P2's first card "each turn" with the Heron at Bandle Tree — trigger and discount are right (D:201.04-D:203.03); only its survival into turn 15 is wrong (D18). Same-turn uses are right: `[D:279.04] PAY_COST: Astral Heron E=5 P=0`, `[D:349.04] PAY_COST: Vex, Apathetic E=2 P=0`.
- Gust: only units at a battlefield with 3 [M] or less are offered (idx=30: the two Steel Paws; idx=135: Steel Paws, Ravenbloom Student — Mel 4 [M] and base units absent); Deflect paid by the opponent as the target is chosen (`[D:029.03] DEFLECT: P1 pays 1 [A] to choose Steel Paws`), not for P2's own Rebuke / Discipline choices.
- Defy on Rebuttal ([1]+1 Power: within [4] and [A]); Abandon returns the countered Rebuke to hand and Predicts (D:162.03-D:163.03); Rebuke is replayed at full cost next turn (D:195.02).
- Combat: 7 vs 0 (D:076), 0 vs 2 (D:150; the 0-Might attacker assigns nothing), 10 vs 12 with Discipline's +2 on Fizz (D:270.04; both option lists respect lethal-before-next), 4 vs 4 with Mutated Mouser attacking at 1 [M] (no Shield), [Tank] first and Pyke [Backline] last (D:278.07-D:278.09), 3 vs 3 (D:317). Pyke does not trigger on Mel dying in the same damage step as Pyke (D:278.10-D:278.12).
- Stupefy "to a minimum of 1": 2 -> 1 (D:304.03); Smoke Screen on a 1-Might unit: stays 1 (D:044.03).
- Kharox: Empower [6] + 2 Chaos Power (`[D:352.03]`, Chaos runes 5 -> 3), Burn 3 from P1's deck (21 -> 18), unit list = the five units in P1's trash, played exhausted to P2's base under P2's control (idx=405).
- Hwei: draw, discard a Unit, +3 [M] this turn (5 -> 8, back to 5 next turn); the trigger and its reflexive part resolve before the showdown the move caused (D:224.04-D:229.05) — already in the checked list.
- Dredge Up's Flow replay is offered from the trash with [2] available (idx=376 `[2] Play Dredge Up #836[S]`).
- Rune Deck empty: no rune channeled on turns 19 and 21 (P2 12/12+0), two channeled for P1 on turn 20 up to 12/12+0.
- Unit play locations: Base plus a controlled battlefield, never Rockfall Path (idx=107, 121, 261, 392); Hide offered only at a controlled battlefield without a facedown card of that player.

### g09 rumble vs reksai

- Second player's first turn channels 3 runes, first player's first turn skips the draw — D:009.06-08, D:006.09.
- Eclipse (-4, no minimum printed) takes a 1 [M] Carrion Dredger to 0 [M]; it does not die and is 1 [M] again next turn — D:034.03; board turn 4.
- Thousand-Tailed Watcher "-3 [M] ... to a minimum of 1 [M]": 3 -> 1, 2 -> 1, 1 -> 1 on every enemy unit, base and battlefield — D:165.03-07, D:217.03.
- Production Surge's "[2] less if you control a Mech": `PAY_COST ... E=2 P=1` with Forecaster on the board (D:104.04); with no Mech it is offered at 4+1 only (idx=223 offered with 10 runes, idx=235 not offered with 2 ready, idx=364 offered with 4 ready + a recycle). The Mech token enters the base exhausted and gets Forecaster's granted [Vision] (`[D:109.04] TRIGGER: Mech [Vision] (P1)`, CR 350.2); the spell's "Draw 1" happens before that Vision resolves.
- Forecaster's Vision grant: fires for Forecaster itself, Gem Jammer, Bubble Bot, Rumble, the Mech token; not for Patched Porobot or the second Gem Jammer played after Forecaster had died (D:168.02, D:324.02 single trigger), and not for Thousand-Tailed Watcher (no Mech tag).
- Power is always paid in the card's Domain: recycle lists hold only Mind runes for Production Surge / Watcher / Singularity (idx=109, 168, 266), only Fury for Falling Star (idx=196, 240, 360), only Order for Hidden Blade / Cull the Weak / Imperial Decree (idx=87, 209, 323), both Domains for Void Rush (idx=147, 286). Exhausted runes may be recycled.
- Hide: offered only at the battlefield P1 controls (`Hide Temporal Breach #853[S] @ Void Gate`, idx=93), costs one Power of any Domain (D:095.05), not offered to P2 for Hidden Blade while P2 controls no battlefield (idx=70). Played from facedown for 0 on the next turn as a Reaction in the opponent's Beginning Step, targets restricted to units at Void Gate (idx=117: Forecaster, Rumble; not the base units) — CR 811.1.b, 811.1.d.2, 811.6.
- Temporal Breach: Forecaster banished and played back to Void Gate exhausted, its play ability ([Vision]) triggers again — D:117.03-04, board idx=120.
- Hidden Blade from hand: only units at a battlefield are offered (idx=84), "Its controller draws 2" goes to P1 — D:087.04-07.
- Blood Rush / Cleave may choose an enemy unit ("a unit"); "this turn" Assault is gone next turn (boards turn 3, turn 7).
- Noxus Hopeful [Legion]: `PLAY: ... cost=4E` / `PAY_COST: Noxus Hopeful E=2 P=0` after Void Rush was played that turn — D:271.03-04.
- Noxian Emissary: Empower costs [1] + Order Power (D:148.03 recycled an Order rune), not offered again while Empowered (idx=189); dying Empowered gives two Recruits in the base, exhausted — D:191.03-05.
- Inferna's [Ambush]: offered as a Reaction only to the battlefield where P2 has units (idx=191, idx=198), never to the base in those windows; she enters exhausted.
- Void Burrower: exhausted as the "may" is accepted at finalization, not offered while exhausted (second conquer of turn 14), ready again after the Awaken Phase; the banished card's cost is paid; play locations = base + battlefields controlled at that moment (battlefield 1 at D:132, battlefield 0 only at D:311 after P2 had left Void Gate). Void Rush: both times declined, both revealed cards drawn (D:144.04, D:270.04).
- Singularity "each of up to two units": 22 play options at idx=259 = 1 (no unit) + 6 (one) + 15 (pairs) for the 6 units on the board.
- Cull the Weak: each player picks one of their own units, anywhere (idx=212, idx=213); a token chosen ceases to exist.
- Frozen Fortress: triggers in both players' Beginning Steps before scoring, hits both sides' units; P2's units killed by it on P1's turn (D:205.03-07, D:283.03-08) leave the battlefield uncontrolled and no hold is scored for it.
- Combat arithmetic recomputed for all four damage steps: D:125 (ATK 1+3+4 = 8 correct; DEF wrong, see N1), D:172 (3 vs 1+1+1), D:321 (7+7 = 14 vs 4; Cloth Armor's Shield 2 rightly not counted on an attacker). Combat damage is gone after the combat (Rek'Sai idx=134, Watcher idx=349). Both sides wiped at D:172 -> no conquer, battlefield uncontrolled.
- Scores: P1 conquer 1 (D:047), holds (D:053, D:090), conquer (D:173), conquer (D:321), hold (D:352) = 6; P2 conquers D:127, D:198, D:257, D:275, hold D:290, conquers D:306, D:346, hold D:363 = 8. Never two scores of one battlefield in a turn; the 8th point is a Hold, so no every-battlefield condition applies (CR 466.1.b).
- Rune and deck counts: every channel is 2 (3 at D:009), rune-deck counts follow the recycles; hand / deck counts follow every draw, Predict recycle, Void Burrower / Void Rush reveal.

### g10 leona vs darius

- Inferna played with [Ambush] at chain priority enters the board at once, no chain item and no priority window for the play itself — CR 359.2 ("A Permanent leaves the Chain and becomes a Game Object"), CR 822 — D:078.02-081.00. Location offered: only Monastery of Hirana, where P2 has Kai'Sa (idx=87, idx=88).
- Hidden Blade on a unit whose death Guardian Angel replaces: the controller still draws 2 — CR 359.3.e.14.b, the rule's own Hidden Blade example ("it will execute even if the kill action of the earlier linked action is negated") — D:049.03-049.12. Guardian Angel: killed, Leona healed / exhausted / recalled, 5M -> 4M.
- Charm (no [Action]) is not offered at Focus in the two non-combat showdowns although P1 could pay it (idx=66, idx=70: 2 Calm runes in play) — CR 343.1.a. Back Off / Hidden Blade ([Action]) are offered there.
- P2 conquers Monastery of Hirana and scores on P1's turn after Charm put Kai'Sa there — CR 348.2.a / 348.2.a.1 — D:077.06-077.07.
- The non-combat showdown at D:043-049 goes on (P2 pass, P1 pass) after Leona, the only unit, was recalled, and Contested is removed afterwards — CR 347.2, CR 323.10 — D:049.13.
- [Hidden]: Hide offered only at battlefields P2 controls with no facedown card there, on P2's turn, for one Power of any Domain (`PAY_ADD: P2 pays [0] + 1 power (any)`); not playable the turn it is hidden (idx=176: End Turn only); played free on the next turn at chain priority; targets limited to units at that battlefield, both players' (idx=181: the two Ferrous Forerunners at Monastery; idx=248: Recruit, Faithful Manufactor, Noxian Drummer, Scuttle Crab at War Camp) — CR 811.1.b — D:155, D:161, D:215, D:220.
- Defy on a Hidden Blade played from facedown: printed cost [2] + 1 Power is within "[4] and [A]" — D:163-167.
- Noxian Drummer's move trigger resolves with no Recruit after Hidden Blade killed the Drummer in response (`[D:224.02] RESOLVE: Noxian Drummer (chain_item=47, ability=1, spell=0)`, no TOKEN line): "here" is a referent checked on execution, and "A unit that is no longer on the board has no location" — CR 359.3.f.1, 359.3.f.2 (Yasuo, Remorseful example), 359.3.e.12. See the judge question below for the other reading.
- Baited Hook: "[1][C]" paid as 1 Energy + 1 Order Power (`PAY_ADD: P2 pays [1] + 1 power: exhausted 1, recycled 1`; on the board one rune is exhausted and the Order runes in play drop by one each time: idx=142 -> idx=143 5Y -> 4Y, idx=216 -> idx=217 5Y -> 4Y); the unit to kill named at activation; after Inferna (1 [M]) only Faithful Manufactor (2 [M]) is offered, after Spectral Matron (4 [M]) all five units of the top 5 (Might 2-4); the unit is played to Base or the controlled battlefield, enters exhausted, Manufactor's play trigger makes a Recruit in the base; deck 28 -> 27 and 25 -> 24 — D:130-139, D:192-196. The top-5 contents are not in the trace, so that no eligible unit was left out the first time could not be checked.
- Hand of Noxus [Legion]: off with no card played that turn (D:011.03, D:064.03), on after a card played from hand (D:039.03, D:099.03, D:133.03) and after a card played by Baited Hook's effect (Noxus Hopeful, D:199.03). Activating it with Legion off is allowed and only exhausts it.
- Grand Strategem [6] + 3 Order: 5 runes exhausted + 1 floating Energy from the legend, 3 Order runes recycled (one ready, two exhausted); all five friendly units get +5 this turn and are back to printed Might + War Camp on turn 14 — D:200-210, SNAP 354.
- Ferrous Forerunner #2 [6] + 1 Fury paid with 5 runes + 1 floating Energy — D:140-146.
- Combat arithmetic, all five combats (War Camp +1 on both sides): 4 vs 2+4 (D:149), 5 vs 2 (D:151), 2+4+3 vs 5 (D:175), 7+8 vs 1 (D:212), 1 vs 2+3 (D:224). Exhausted units defend and deal damage. Damage options respect lethal-before-next (idx=164, idx=199). Kai'Sa's 2 damage is gone after the combat (idx=172).
- Scores: P2 conquer BF#0 (T8), hold (T9), hold (T11), conquer BF#1 (T11), hold BF#0 (T13), conquer BF#1 again (T13, after P1 retook it on T12), hold + hold (T15) = 8; P1 conquer BF#1 (T10), conquer BF#1 (T12) = 2. The winning point is a Hold.
- Channel: 2 per turn, 3 for P2's first (D:008.09-008.11); recycled runes come back later (P1 Order 46 / Calm 49 re-channeled at D:156.09-156.10).
- Standard Moves with several units as one move (D:110, D:171, D:212, D:218); no battlefield-to-battlefield move is offered (idx=100: Kai'Sa / Inferna at Monastery only -> Base).
- Forgotten Signpost's ability is never offered: at no decision did P1 have a ready unit to exhaust and a different unit at another location (both units in base at idx=177, idx=190, idx=245; every other unit exhausted otherwise).
- Apprentice Smith: `[D:175.03] APPRENTICE SMITH: revealed non-gear Emperor's Divide -> recycled`, deck count unchanged (23). The next card P1 draws is `[D:180.04]   DREW: Emperor's Divide (id=32)`. The list has two copies and the reveal line prints no id, so whether the recycled copy went to the bottom could not be determined from the trace (the executor's `recycleCards` puts cards on the bottom).
- Mulligan: set aside 2, draw 2, recycle (deck 39 -> 35 both players) — D:003, D:004.

### g11 lux vs draven

- Lady of Luminosity draws AFTER the spell has resolved (Sprite Burst D:099.02-101.05, Time Warp D:226, Drag Under D:322-324), not while it waits on the chain — CR 420.4.a "trigger when the act of playing the card has been *completed* by the resolution of the card".
- Glorious Executioner's "When you win a combat" goes on the chain and resolves BEFORE the conquer of that combat (D:233.10-235.05, then `[D:235.06] CONQUER`) — CR 468.3 (determine result), 468.4 (resolve the chain from it), then 468.5 (establish control).
- Switcheroo ([2] + 2 Chaos Power) paid with one rune exhausted and one Chaos rune recycled (D:326.04-328.01): the rest was the floating [1] + [A] of Jhin, Murderous Artist's move (D:314.03-314.04) — CR 163.2.b "Some Power is Universal and can be used to pay for costs of any Domain".
- Time Warp paid with 8 runes for [10] at D:213-220: 2 Energy came from Lux, Crownguard ("Use only to play spells"); 4 Mind runes recycled.
- Thousand-Tailed Watcher's Accelerate [1][B]: one Order rune exhausted, one Mind rune recycled (board before/after idx=159/160).
- Noxus Hopeful paid [2] (`PAY_COST ... E=2`) after Evelynn was played that turn — [Legion] — D:191.04.
- Draven, Showboat's Might follows the score at every board: 4 (1 pt), 5, 6, 7, 8 (5 pts, in the combat D:233.04), 10 on replay at 7 pts (SNAP 605).
- Combat sums: D:121.05 1 vs 9 (Sprite's Cleave Assault 3 not counted as a defender); D:233.04 16 vs 2; D:302.06 8 vs 5, Evelynn ([Backline]) assigned last and takes the excess; D:345.05 8 vs 11 with two stunned Sprites dealing 0; D:346.05 4 vs 4, stun still on. Damage healed after each combat.
- [Temporary]: Sprites of P1 survive P2's turn and die at the start of P1's Beginning Step, before the hold is counted (Star Spring lost at D:131.07, only Treasure Hoard scored D:131.10; D:347.07). Spinning Axe dies only when unattached.
- Ravenbloom Prefect triggers on a Gold TOKEN played by the opponent's Wages of Pain (CR 350.2), is banished as the cost at finalization, and the Gold is banished at resolution — D:085.06-090.04.
- Vex, Apathetic triggers once per Sprite token played at her battlefield and stuns each — D:338.04, D:339.04, D:343.03, D:345.03.
- Star Spring: one trigger for the first non-token unit played there that turn; the moved Tentacle stays ready and makes a Standard Move later (D:182.03-186.03, D:190.03).
- Tideturner's swap offers only units at another location (Noxus Hopeful, Evelynn at Treasure Hoard; not the three in its own base) — idx=302.
- Treasure Hoard's "pay [1]" is paid at finalization and the Gold arrives exhausted; Plundering Poro's Gold too (D:202.04, D:204.03, D:206.03).
- Up from the Deep played from hand, then from the trash for Flow [3] the same turn and banished (D:356-364); the first copy banished at D:121 as well.
- Star-Crossed returns the champion Draven to its owner's hand, replayed later for its full cost (D:245.03, D:349.04).
- Scores: every conquer / hold recomputed, one per battlefield per turn; the 8th point by Hold (D:365.08) — CR 466.1.b puts no condition on a Hold.
- Channel: 3 for the second player's first turn, 2 after, 1 then 0 when the Rune Deck runs out (D:178.09, D:199.08), recycled runes channeled again later.
- Option lists: each Main Phase / Focus / priority list compared with hand, ready runes and board — no unaffordable play offered, none missing except through the known items above (examples: Wages of Pain absent with no unit at a battlefield idx=75; Ride the Wind absent with no friendly unit idx=41; Progress Day absent at 5 ready runes idx=368; Stupefy @ Vex offered with 2 runes, 1 for the Deflect, idx=253).

### g12 sivir vs viktor

- A unit played with [Ambush] / from facedown is on the board at once, with no priority window of its own —
  CR 359.2 "A **Permanent** leaves the **Chain** and becomes a **Game Object**" — Vi D:046.06-D:052.01,
  D:093.03, D:126.03 (each to Zaun Warrens where P2 had a unit; in a showdown at Focus and twice at chain
  priority; enters exhausted); Pyke D:305.04; Edge of Night D:122.02.
- Second player channels 3 on its first turn, 2 otherwise (CR 487.7) — D:009.05-.07; Rune Deck counts follow
  every recycle (e.g. P1 `2/2+10` at SNAP 251).
- Rune Pool empties at the start of the Main Phase (CR 316.3): the [A] of each Gold P1 activated in a
  Beginning Step is gone at the Main Phase board (`Pool:E0/P1` -> `Pool:E0/P0`, SNAP 106 -> 107 and five more).
- Costs: every `PAY_COST` equals the printed cost and the runes moved match (Sivir 4+1 Chaos, Vi 5+1 Order,
  Singularity 6 + 2 Mind, Punch First 1 + 2 Body, Star-Crossed 3 + 1 Chaos, Hidden Blade 2 + 1 Order, Keeper of
  Law 5 + 1 with no battlefield of exactly two units, Bellows Breath 1 + 1 Mind); Hide = 1 Power of any Domain;
  Equip of Edge of Night = 1 Chaos rune (D:165.04, the exhausted Chaos rune left); facedown plays free.
- Sprite Fountain (Mind Power) not offered on turn 14 with six Order runes and no Mind rune (idx=269); offered
  on turn 16 after two Mind runes were channeled (idx=328).
- Rampage not offered with no enemy unit on the board (idx=257, 266) and offered again when P2 has units
  (idx=285); Punch First not offered with no unit on the board (idx=10).
- Hide: only on own turn at a top-level decision, only at a controlled battlefield, not at a battlefield that
  already holds a facedown card (idx=299, 319: no Hide @ Zaun Warrens after Tornado Warrior was hidden there),
  never played the turn it was hidden; a facedown card goes to the trash when control is lost — CR 811.1.b,
  323.6.a — `[D:297.04] HIDDEN: Tornado Warrior removed from facedown (control lost)`.
- Hidden Blade from facedown offered only at its battlefield's unit (own Recruit) and its controller draws 2 —
  CR 811.1.d.2 — D:232-234; from hand only units at battlefields (idx=273).
- Edge of Night from facedown: target limited to the unit "here", +2 [M] (D:122-125).
- Sivir's Ganking: `Move ... -> Dusk Rose Lab` from Zaun Warrens offered only once 2 Power were spent that turn
  (not at idx=296 after 1 Power, offered at idx=299 / 319); without it a unit at a battlefield is offered only
  `-> Base`.
- Battle Mistress: one trigger per rune recycled (two for Punch First), exhaust paid at finalization, Gold
  enters exhausted; "one or more enemy units die" fires once for two deaths in one resolution (Singularity
  D:182.05-.08) and once for a damage step with two deaths (D:305.14).
- Combat arithmetic: D:159.03 6 v 5 (Vi dies, Sivir survives 5 on 6, damage gone after combat — CR 143.3.b.2);
  D:193.05 1 v 9; D:305.05 3 v 9 with [Backline] Pyke last (all 3 forced onto Sivir). Exhausted defenders deal
  damage. Grim Resolve +3 and 2 XP on the win as defender (D:199.03).
- Scores: all 12 `SCORE:` lines recomputed (P1 8, P2 4); every hold is a battlefield the turn player controlled at its Scoring
  Step; Dusk Rose Lab's kill happens before scoring and cost P1 the hold on turn 9 (control lost, SNAP 223) but
  not on turn 17 (Pyke stayed). The 8th point is a Hold (CR 466.1.b) — D:331.09.
- Bellows Breath / Singularity option lists are exactly the sets "up to three at one location" / "up to two"
  (7 options at idx=38, 11 per copy at idx=182); Bellows Breath with zero targets is legal (CR 355.13, D:239.06).
- Fizz: the six trash spells offered all have Energy cost <= 3; Punch First cost 0 Energy + 2 Body; recycled
  after resolving (deck 21 -> 22 at SNAP 436).
- Zaun Warrens: discard chosen at resolution ("discard 1" does not choose, CR 355.18) — D:055, D:164.
- Standard Move asks for further units of the same origin (D:139.00, D:215.00, D:305.00).

### g13 renata vs poppy

- Targon's Peak offers and readies an opponent's rune: idx=163 lists P1's exhausted Mind / Order runes (obj 44, 45, 46, 48, 52) among 12; P2 picks its own Body Rune 97 and P1's Mind Rune 46; SNAP 259 shows P1 `rdy:3B` (was 2B) — judge question 1 of `docs/judge-questions-2026-10.md` / section 5 table "ok (opponent's runes offered: judge ruling)"; runes named when the delayed ability is finalized in the Ending Step (CR 355.5.b) — D:148.03-152.03.
- Cull the Weak with no unit on the board: played, paid ([2] + Order rune recycled), resolves with nothing — "Each player kills one of their units" chooses nothing at play — D:005-010.
- First turns: P1 skips its draw (`[D:004.14]`, CR 482.7), P2 channels 3 on its first turn (D:011.09-.11); mulligan 0-2 cards, 11 options; decks 35 after mulligans.
- Standard Move to an empty uncontrolled battlefield opens a non-combat showdown, both players get Focus, then Conquer +1 — D:038.03-.06, D:093.03-.06, D:133.03 / D:142.04, D:180.03-.05. P1 may play an [Action] spell (Imperial Decree) in P2's showdown at a battlefield where P1 has no unit — CR 348 "all players pass Focus" — idx=145.
- Hide: [A] paid by recycling one rune with all runes exhausted (D:039.05), only battlefields P1 controls offered, not offered the turn before control; played from facedown for [0] on a later turn, targets only units at that battlefield (idx=45 Scout only while Renata is in base; idx=59 both) — CR 811.1.
- Hidden Blade from hand on own units: "Kill a unit at a battlefield. Its controller draws 2." — P1 draws 2 (D:061.04-.07).
- Soaring Scout's Deathknell channels the rune exhausted (D:063.04); Machine Evangel's three Recruits enter the base exhausted (SNAP 186).
- Gold: played exhausted by Wages of Pain / Blood Money / Card Sharp, readies in Awaken, [A] paid Machine Evangel's Order Power (`[D:072.04] PAY_COST: Machine Evangel E=5 P=1`, 5 exhausts, no recycle); token ceases to exist.
- Rampage / Challenge: both units chosen before the cost (CR 355), units in a base are legal ("a friendly unit and an enemy unit"), not offered with no enemy unit on the board (idx=67, 70); damage 2 <-> 4 and 7 <-> 4 simultaneous; Poppy's 4 damage and Evangel's 2 stay until end of turn / next damage.
- Sunken Temple: fires for Poppy 7M (D:093.07) and Rengar 6M (D:180.06), not for conquers with a 1M Scout / Recruit (D:038.05, D:116.16 — the 6M Recruit died in that combat); [1] paid at finalization, draw at resolution.
- Combat arithmetic: 6+1+1 = 8 vs Poppy 5+2 = 7 (D:116.04); 1+1 vs 6+4+1+2 = 13 (D:197.04); 1 vs 5+6+4 = 15 (D:273.05). Blood Money's list drops the Recruit that Punch First raised to 6M (idx=123: two Recruits, was three at idx=115). Survivors are healed when the combat ends (CR 468.1.a.1: Master Yi's 2 and 1 damage gone at SNAP 333 / 429). Poppy's Heirloom unattached and recalled (CR 323.6), offered again for Equip only once XP > 0.
- Blood Money on a friendly Recruit: two Gold, exhausted (D:121.03-.07).
- XP ledger: +1 Heirloom, -1 Equip, +2 Yi conquer, -1 Equip, +2 +1 (hold), +1 Diplomat, +2 +1 +2 +1 (two holds, each Yi for its own battlefield), -3 legend, +1 Heirloom, -1 Equip, +2 +2 +1 = 13 at `[D:280.03]`; every `now N` matches. Keeper of the Hammer's hold trigger once per hold.
- Master Yi, Tempered [Level 6] Ganking: battlefield-to-battlefield moves absent at 4 and 5 XP (idx=190, 210), offered at 11 / 9 XP (idx=235, 257) and used (D:237.03).
- Vi, Peacekeeper [Ambush]: offered as a Reaction at P2's own chain priority only to the two battlefields with P2 units, not to Base (idx=236); enters exhausted (SNAP 366); Kinkou Initiate (no Ambush) not offered there.
- Channel with one rune left in the Rune Deck: P2 channels 1 (D:214.06, `12/12+0`).
- Kinkou Initiate: "draw 1 if your other units have total Might 5 or more" tested at resolution (it is in the effect, not after the "When" clause): 5+6+4+11+1+2 = 29 (D:243.05).
- Card Sharp: P1 yes, P2 no -> one Gold for P1 (D:257.03-258.04); choices made as it resolves (an instruction, not a cost or target).
- Jayce: kills the chosen gear at resolution, the next gear from hand pays `E=0 P=1` (`[D:271.04] PAY_COST: Vanguard Armory E=0 P=1`, one Order rune recycled).
- Consult the Past ([Reaction]) played by the non-turn player in the Scoring Step on top of two hold triggers; resolves first (D:200.01-206.06).
- Win by Hold at 7 -> 8 with the second hold of the Scoring Step (D:280.05); no winning-point restriction applies to holds.

### g14 yi_rebuild vs ahri

- An enemy unit moved by Charm to an empty battlefield conquers it for its controller on the Charm player's turn (P2 +1) — CR 190.3.a.1, 344.2, 471.1 — D:036.04-07.
- Rampage without the additional cost: 2 to Mutated Mouser, 1 to Pit Rookie, Mouser dies; the [O] prompt comes after targets and base cost — card text — D:039-047.
- En Garde +1 and +1 ("only unit you control there", in base) on top of the opponent's Discipline: 2 -> 4 -> 6 — D:017.03, D:019.03-05.
- Janna, Savior as a Reaction on her controller's own chain; her [C] Power paid with a Calm rune (D:059.01) and later with floating [A] (D:143.04: 3 runes, no recycle) — CR 135.2.e.5.b ("[A] can be spent to pay a Power cost of any Domain"); same for Sprite Fountain's Mind Power at D:151.04.
- Back Off from hand: stun + draw (D:063.03-04); from facedown: stun, no draw, no cost, target limited to its battlefield (D:187-189, idx=204 one option `@ Sand Soldier`), played on the turn after hiding in the opponent's Beginning Step with a trigger on the chain — CR 811.1.b / .d.
- Temporal Breach from facedown: offered only `@ Ravenbloom Student` (the one unit at its battlefield), no cost line, unit comes back exhausted at 2M — D:085-087.
- Conquer with no point and no CONQUER line when the battlefield was already scored that turn by Hold (Amateur Recital, turns 6 and 10: control goes to P2, score unchanged) — CR 472 — D:098.04-099.00, D:215.03-216.00.
- `Play Janna, Savior -> Emperor's Dais` offered (idx=156) and control of Emperor's Dais kept while the Dais trigger is on the chain although its cost had just bounced the only unit there (same for P1 at D:181-183) — CR 323.5: control is lost only "if the turn is in an Open State"; the Sand Soldier arrives before that.
- Emperor's Dais: may + [1] + bounce at finalization, 2M Sand Soldier (exhausted) at resolution; not offered with no ready rune (D:036.10) — D:141-150, D:180-183.
- Blue Sentinel doubles its own "When I hold" as well as Amateur Recital's (`SCORE_TRIGGERS: 2 ability(ies) trigger 1 additional time(s)`, two delayed [Add]s, pool 2) — "Your hold effects for holding here"; CR 383.4.d.1-.2 (a unit's "When I hold" is a Hold Effect) — D:118.09-11, D:131-133; the second pair still resolves after Blue Sentinel was moved to base by the Recital (D:200-202).
- Amateur Recital offers every unit at a battlefield, the enemy Sand Soldier included ("a unit at a battlefield") — D:192.04.
- Ravenbloom Student triggers after each of its controller's spells has resolved (CR 420.4.a) — D:063.06, D:082.06, D:095.06, D:206.06; +1s stack (D:097.03 -> 4M) and end with the turn.
- Nine-Tailed Fox: one trigger per attacking unit, -1 each (Rengar 8 -> 7, Yi 4 -> 3), no cost; Yi then dies to 3 damage at 3 [M] — D:161.04-05, D:177-179.
- Combat sums: D:179 ATK 3+7 = 10 vs DEF 3+2 = 5; D:252 ATK 3+3 = 6 vs DEF 3+2+6 = 11; D:257 ATK 4 (no Shield while attacking) vs DEF 3+2 = 5; every option list assigns the whole sum with at most one non-lethal amount; stunned Pit Rookie assigns nothing (D:066).
- Defender keeps the battlefield when all attackers die (D:252, D:257); no recall needed.
- Rengar with [Ambush] as a Reaction on his controller's own chain: `-> Emperor's Dais` (own units) and `-> Amateur Recital` (enemy units only); enters exhausted; 5E + a Body rune — D:221-227.
- Sprite Fountain: Temporary kill before scoring, Deathknell makes a ready 3M Sprite in base; a Sprite made in the Beginning Step of turn 10 lives until the Beginning Step of turn 12 — D:185-191, D:230.07.
- Stupefy "to a minimum of 1": buffed-less Sand Soldier 2 -> 1 — D:206.03. "This turn" Might changes and damage are gone at the next turn's first board in every case (Rengar 5 -> 6, Janna 2 -> 3, Yi 9 -> 4, Pit Rookie 1dmg -> 0).
- Channel 2 / 3 (second player's first turn) / P2 skips the first draw; rune counts in play + rune deck = 12 at every board for both players; every Power recycle is of a Domain of the card (Charm Calm, Rengar / Punch First Body, Janna Calm, Blue Sentinel / Sprite Fountain / Ahri Mind); deck counts match the draws (P1 26, P2 23 at the end).
- Standard Moves: only ready units; base <-> battlefield only (no battlefield-to-battlefield option at idx=143, 236); "move another unit with this move" lists the other ready units at the same origin (idx=171, 174, 231, 270).
- Game end: P2 at 7 holds Amateur Recital and wins at once; the Recital trigger fired at D:268.07 is never resolved — CR 474.

### g15 leblanc vs akali

- First player skips the first draw, second player channels 3 — CR 489.7 — D:004.17, D:008.09-.11. (The trace
  line cites "CR 482.7"; the rule is 489.7 in the 2026-10-09 text. Wording of a log line only.)
- Moving the last unit off a controlled battlefield in an Open state loses control at once and the facedown card
  there goes to the trash, revealed — CR 323.5 ("Players lose control of any controlled Battlefields without their
  Units occupying them if the turn is in an Open State and there is no Showdown or Combat ongoing there"),
  107.3.d, 422.4 — D:037.03-.04 (P2 hid Mischievous Marai at Star Spring at D:036 and moved its only unit away).
- Hidden: hide only at a controlled battlefield, for [A]; one facedown card per battlefield (second Hidden Blade's
  Hide not offered at idx=67 once one lay at Void Gate — CR 107.3.b); played from facedown for 0 from the next
  turn on, as a Reaction, targets limited to that battlefield — CR 811.1.b, 811.1.d — D:047 (own turn, Scoring
  Step chain; only LeBlanc offered), D:072.03 (opponent's turn, combat; the four units at Void Gate offered).
- Hidden Blade on a unit whose death Zhonya's Hourglass replaces: the unit is healed, exhausted, recalled with its
  Long Sword; "Its controller draws 2" still happens (no "if you do") — CR 359.3.e.14.b — D:125.03-.11.
- Hidden Blade on its controller's own Reflection: token ceases to exist, P1 draws 2 — D:074.03-.08.
- LeBlanc, Fragmented dying in the Scoring Step draws 2 — the Scoring Step is part of the Beginning Phase
  (CR 315.2.b) — D:049.03, D:055.03. (Not D56: that is the Awaken Phase.)
- Deathgrip whose only target died in response: kill and +Might ignored, "Draw 1" still done — CR 359.3.e.5 /
  359.3.e.1 — D:057.03-.05.
- Deathgrip with two units: kill Watchful Sentry (1M), LeBlanc +1 -> 4M this turn (3M again on turn 4's board),
  draw 1, Sentry's Deathknell draws 1 as its own chain item — D:027.02-029.04.
- Deceiver: discard and exhaust paid at finalization (D:042.03-043.03), token ready "there" at resolution
  (D:059.03); with no other unit there the Reflection stays a 0 [M] token with [Temporary]; a 0 [M] unit with no
  damage is not killed (CR 143.2.b "nonzero damage") and holds the battlefield until it leaves — D:059-061, boards
  of turn 5-6.
- Mirror Image: unit chosen at play (an enemy unit in its base is legal: "Choose a unit"), token ready to P1's
  base, copy of Mischievous Marai is 2M (the original's Long Sword +2 is not copied), [Temporary] kills it at P1's
  next Beginning Step before scoring — D:088-104, D:118.07.
- Ki Barrier may choose an enemy unit ("Choose a unit"); Power paid with an Order rune only (recycle list at
  idx=109 holds the two Order runes) — D:094-100.
- Thousand-Tailed Watcher: -3 to every enemy unit wherever it is, floor 1 (Kai'Sa 4->1, Marai 2+2 sword ->1,
  Empowered Witness 2+2 ->1, Herder 3->1, Akali 3->1), gone next turn (board of turn 12) — D:138.03-.08.
- Combat at Void Gate, turn 6: P2's Rogue Assassin pulls the attacker Marai (with Long Sword) to base, not
  readied (legend not Empowered); remaining ATK Witness 2M vs DEF Scout 1M; Scout dies, its Deathknell channels 1
  rune exhausted; Witness survives (1 damage < 2M), is empowered when the combat ends (+2 -> 4M) and conquers:
  P2 2 -> 3 — D:068.03-081.05.
- Rogue Assassin: [E] move ability only offered to P2 in showdowns on P2's turn (idx=19, 73, 81, 158), not after
  it is exhausted (idx=86 forced pass). The `Activate Rogue Assassin` of the Main Phase lists is the Empower
  [3][A]; it is there exactly when 3 ready runes exist (exhaust 3, then recycle any rune in play for the [A]):
  present at idx=45 (ready 2R 1G of 5 in play) and at idx=93 with the legend exhausted (Empower has no [E] cost);
  absent at idx=18 (2 ready), idx=128 (1 ready), idx=183 (1 ready).
- Akali, Deadly Weapon Empower [2][R] offered only with her on the board and 2 ready runes plus a Fury rune in
  play (idx=123, 138, 151, 162, 169; absent idx=128, 183).
- Standard Move of several units as one action, one Akali trigger, showdown at the empty uncontrolled battlefield
  after the trigger has resolved; battlefield-to-battlefield never offered (no [Ganking] here) — D:140-146.
- Non-combat showdown Focus list (idx=158): Back Off ([Action]) offered, Shuriken Flip (neither keyword) not.
- Kai'Sa, Survivor's conquer trigger draws 1 — D:146.04-148.05. Back Off from hand: stun + draw 1 — D:155.03-.05.
- Cull the Weak: no target, each player picks one of their own units as it resolves (turn player first), both
  die together; Marai's Long Sword is unattached and recalled to base (CR 323.6); Soaring Scout's Deathknell
  channels 1 exhausted — D:177-179, D:188-192.
- A permanent played in a showdown / at chain priority enters without a priority round of its own (Long Sword
  D:068.05, D:163.03) — CR 359.2 "A Permanent leaves the Chain and becomes a Game Object" at finalization.
- Costs: every `PAY_COST` equals the hand cost on the board; single-Domain cards' Power was only payable with a
  rune of that Domain (recycle lists: LeBlanc Order, Long Sword Fury, Ki Barrier / Hidden Blade / Cull the Weak
  Order, Thousand-Tailed Watcher Mind, Blitzcrank Calm); rune counts after every recycle and channel re-added and
  right (P1 ends 7 in play + 5 in the rune deck; P2 10 + 2).
- Scores re-added: P2 conquer SS (1), hold SS (2), conquer VG (3), hold VG (4, 5, 6), conquer SS (7), hold SS (8,
  first hold of turn 14 ends the game — a Hold may be the winning point). P1 conquer VG (1), hold VG (2).
  Draw / deck counts re-added for both seats: right.
- Could not determine: Mirror Image ([3] + 2 Power, Domains Mind and Order) was paid with one Order and one Mind
  rune, and its recycle prompts offered every rune (idx=103: 7 options, idx=104: 6). The sheet gives "2 Power"
  without the Domain of each symbol and the class has `power_cost = 2` with both Domains; if the printed cost is
  two Mind symbols (the deck-findings card table prints it as `[3][M][M]`, which may only be that table's
  notation), an Order rune should not have paid it. `[D:092.01]   RECYCLE_RUNE: Order Rune (id=49, ready) for power [agent choice]`
  / `[D:093.01]   RECYCLE_RUNE: Mind Rune (id=51, exhausted) for power [agent choice]`. Needs the printed card.

### g16 jayce vs ambessa

- Second player's first turn channels 3, first player skips the first draw — D:004.17, D:005.09-005.11
- Platewyrm Egg "This enters exhausted" (D:006, D:021, D:118: board `Platewyrm Egg(,exh)`); Empower "[1], [E]" (D:069.03, D:125.03); [Add] [2] once Empowered, [1] otherwise (D:092.03 against D:094.03); Empowered persists across turns (CR 442)
- Defender of Tomorrow Empower [2][A][A] paid with 2 floating Energy and 2 recycled runes of any Domain — D:046.03; legend stays Empowered
- Floating Energy added in the Beginning Step is gone by the Main Phase (CR 167: "Every player's **Rune Pool** empties at the start of each player's Main Phase and the end of each player's turn.") — turn 15: Pool E5 at SNAP 368, Elder Dragon (12E+4P) not offered at idx=248 with 10 ready runes
- Dredge Up from hand, then by Flow [2] in the same turn, then banished — D:032-039, `P1 | Banish: Dredge Up`
- Bellows Breath Repeat [1] + Mind Power: a Body rune exhausted, the exhausted Mind rune recycled — D:027.01 with boards SNAP 54/55
- Sigil of the Storm "you must recycle one of your runes": chosen as the ability resolves, not a target (CR 355.10.f, in terms) — D:061.02-062.03
- Standard Move options: base -> either battlefield, battlefield -> base only (no Ganking in either list); several units in one move — every Main Phase list, e.g. idx=78, 136, 191, 256, 327
- Units may be played to a controlled battlefield only once it is controlled — idx=65 onwards (Sigil of the Storm), never to Risen Altar
- Kinkou Initiate "draw 1 if your other units have total Might 5 or more": the "if" is not right after the condition, so the ability always goes on the chain (CR 383.2.a.1, Loose Cannon example); 6+3+3+1+4+4+2 = 23 — D:194.05
- Pit Rookie: "another friendly unit" list leaves itself out; Vi 5 -> 6 with a buff — D:152.04-155.03
- Punch First: target chosen before the cost; +5 this turn (3 -> 8), gone at the next turn (SNAP 203, SNAP 422) — D:109-114, D:263-268. Target list holds enemy units too ("a unit") — idx=271
- Sabotage: whole hand revealed, unit cards not choosable, card recycled to the bottom, deck +1; with no non-unit card nothing is recycled — D:148-149, D:186-187, D:220.03-220.04
- Bird token: [Deflect] charged [A] to the opponent as the Bird is chosen, before the spell's own cost — D:202.03-202.06; token ceases to exist — D:207.04
- Public Execution's 1 Power paid with a rune of either of its Domains (CR 135.2.e.6.c) — idx=207 lists Body and Order runes
- Irresistible Faefolk pulling an exhausted Jayce from P1's base to Risen Altar: unit chosen at finalization (D:211.03), the staged showdown becomes a combat with P2 (who contested) attacking (CR 344.1) — D:213.03-213.04
- Combat arithmetic: Faefolk 1 vs Jayce 6 (an exhausted unit still deals damage); Faefolk dies; P1, the defender, establishes control of the uncontrolled Risen Altar and that is a Conquer and a point on P2's turn (CR 468.5, 468.5.d, 468.5.e "This does not have to be the player that applied **Contested**") — D:220.05-220.12
- After an item resolves, priority goes to the controller of the newest item left on the chain, not to the turn player (CR 340.4) — D:144-147 (P1's Egg, then P1 again; then P2 for its Sabotage)
- Holds: one point per Beginning Phase for the one battlefield controlled; P1's hold of Risen Altar at D:239.04; control of Risen Altar lost when Jayce moved to base (SNAP 372 `BF Risen Altar  [Ctrl:--]`)
- Ambessa, The Wolf Empower [3][O]: 3 runes exhausted + a Body rune recycled, 4 -> 7 [M]; Matriarch of War empowered by it; Matriarch's ability (disempower, [A], exhaust) readies a unit that was played this turn, which then moves — D:269-278
- Jayce's first-gear trigger: "the first time you play a non-token gear each turn" fired for Dazzling Aurora (D:300.02), not for the gear played before Jayce was in play. Its list holds only exhausted objects and not Jayce; it also holds the OPPONENT's exhausted units, runes and legend (idx=310, 22 options) — the text says "something", with no "friendly"; P1 readied P2's Order Rune (D:304.03). Recorded as correct by the text; if "something" is meant to be narrower, this is the place.
- Garbage Grabber: three trash cards recycled as a cost at activation, then draw 1 on resolution; not offered with fewer than 3 cards in the trash (idx=127-135) — D:305-310
- Dazzling Aurora: 12 non-units revealed, the first unit banished and played to base (the only location: P1 controlled no battlefield), the rest recycled, deck 27 -> 26; Elder Dragon enters exhausted and its play trigger fires — D:313-314
- Elder Dragon "choose up to one enemy unit at each location": one prompt per location holding enemy units (P2's base, Sigil of the Storm), "None" allowed; with nothing chosen the ability still goes on the chain (CR 355.13) — D:314.06-318.02
- Rune counts of both players add up every turn (channel 2, recycles for Power, Sigil's recycle); rune deck never ran out
- Winning point: P2 at 7 scored Sigil of the Storm by Hold (D:318.08) and then conquered Risen Altar (D:321.04-321.05) — every battlefield scored that turn, so the Conquer point wins (CR 473.1.b)

### g17 lillia vs azir

- Ravenbloom Student triggers AFTER the spell resolves, and not at all for a countered spell — CR 420.4.a ("trigger when the act of playing the card has been *completed* by the resolution of the card"), 420.4.a.1 and 426.1.b (a countered card "is not considered to have been played for abilities that trigger on cards being played") — turn 12: +1 after Discipline (D:174.07, 3M), nothing for the countered Smoke and Mirrors, +1 after P1's own Defy resolved (D:183.04, 4M).
- Eye of the Herald's "When I move" item resolves with no Recruit after its bearer was killed in response — "here" is a referent checked on execution, and a unit off the board has no location (CR 359.3.f.2, 359.3.e.12) — D:148.04 queued, D:153.03 Azir killed, `[D:155.02] RESOLVE: Eye of the Herald (chain_item=41, ability=1, spell=0)` with no TOKEN line.
- Lillia's Sprite goes to the place she moved FROM (her base), exhausted, and the item resolves before the showdown opens — CR 359.3.f.3 (the Lillia example), CR 344 (showdown opens in a Neutral Open State); text has no "ready" — D:133.03-135.05.
- Smoke and Mirrors from facedown is offered with the first unit at its battlefield and the second elsewhere — CR 811.1.d.4 example (Smoke and Mirrors) — idx=46. From hand, all 6 ordered pairs of P1's three units at three locations are offered, with no [Temporary] unit among them (the move is conditional, "Draw 1" is not) — idx=197.
- Charm (neither [Action] nor [Reaction]) is NOT offered at Focus in the non-combat showdowns of turn 12 (idx=188, 197, with 6 and 4 ready runes) and is offered in the Main Phase (idx=138, 143, 146, 186) — the correct side of D5.
- Defy is not offered against a chain holding only an ability (idx=167, Eye of the Herald's item) and is offered once Deathgrip is on it (idx=170).
- The Temporary Sprites die before scoring, control is lost in the next cleanup and no Hold is scored — CR 816, 190.4.c, 323.5 — turns 4, 6, 8; the facedown Smoke and Mirrors goes to the trash with the control (CR 107.3.d, 323.6.a) — D:046.03.
- Control is lost when Hidden Blade kills the lone Sprite (CR 190.4.c) and its controller P1 draws the 2 — D:023.03-023.08.
- Seat of Power draws 0 three times: P1 controlled no other battlefield at each conquer (D:035.03, D:062.03, D:137.03; on turn 2 Seat of Power was conquered before Dusk Rose Lab).
- Bashful Bloom costs [3] with one [Temporary] Sprite (D:029.03) and is not offered when unaffordable (idx=59: 1 ready rune, cost 3; idx=132: 2 ready, cost 4).
- Emperor of the Sands is offered only after an Equipment was PLAYED that turn (idx=50, 165, 166, 174, 176), not after one was merely attached (idx=124) and not without Energy (idx=90, 0 ready runes).
- Guards!: only the base is offered (P2 controls no battlefield); the reflexive "may pay [C]" is asked and paid at that item's finalization with an Order rune ([C] = Power of the card's Domain, CR 135.2.e.6) and the Soldier readies at resolution — D:071.03-075.03.
- Brutalizer: 4+1+2 = 7M (Azir), 3+1+2 = 6M (Lillia, attached at a battlefield from the base), 2+1+2 = 5M (Sand Soldier); +2 gone next turn (5M / 4M / 3M; the lag is D23).
- Combat turn 5: Sand Soldier 2M vs Sprite 3M — 2 and 3 assigned, Soldier dies, Sprite survives, damage gone after combat, P1 keeps Seat of Power, no score — D:081.04-081.11.
- Winning point by Conquer at Dusk Rose Lab with Seat of Power held earlier that turn: both battlefields scored this turn (CR 466.1.b as cited in W/CLAUDE.md) — D:167.04, D:185.05-185.06.
- Payments: every PAY_COST against the printed cost and the runes that moved (Sprite Fountain 2E+Mind, Hidden Blade 2E+Order, Defy 1E+Calm x3, Guards! 3E, Azir 4E, Lillia 3E x2, Ravenbloom 2E, Brutalizer 2E x2, B.F. Sword 4E, Eye 1E, Soul Sword 1E, En Garde 1E x2, Stupefy 1E, Discipline 2E, Smoke and Mirrors 2E, Deathgrip 2E; Hide [A]; Accelerate [1]+Domain). Rune counts: P1 2 per turn, P2 3 then 2. Draws: P1 deck 35 -> 25, P2 35 -> 28, each step accounted for (apart from N1).
- Two Lillia, Fae Fawn on P1's board at once (Champion Zone copy and Main Deck copy) — no rule read that forbids it.

### g18 missfortune vs zed

- A battlefield regained in the turn it was already held is not a Conquer: P1 holds Zaun Warrens (`[D:191.08] SCORE: P1 +1 via Hold BF#1 -> 4`), moves its only unit to base (D:192.03; board idx=219 `BF Zaun Warrens  [Ctrl:--]`), plays Miss Fortune there (D:196.03), non-combat showdown (D:201.02), control returns (idx=230 `[Ctrl:P1]`) with no CONQUER line, no point and no Zaun Warrens trigger — CR 471.1 ("a **Battlefield** they did not yet **Score** this turn"), 472, 473.2.c; losing control on leaving: CR 190.4.c / 323.5.
- Miss Fortune "may play me to an open battlefield": offered to both empty uncontrolled battlefields (idx=7, 16, 35), not to an enemy-held or enemy-occupied one (idx=106, 153), and it opens a showdown (CR 190.3.a.1). Her grant to other units was never needed (she was at a battlefield P1 controlled while alive).
- [Ambush]: Kha'Zix (idx=37), Rengar (idx=108, 320) and Nidalee (idx=263-275) are offered at chain priority only to battlefields where P1 has units, Rengar also where there are only enemy units; the unit is on the board at once, without a priority round of its own (CR 337.2, section 6); all enter exhausted. Rengar played into Targon's Peak makes P1 the attacker (D:287.03).
- A unit that joins a combat during its showdown is an attacker in the damage step: Nidalee, 11 = 3 + 4 + 4 at D:250.08.
- Combat arithmetic, all 7 damage steps: 7 v 6 (D:126), 7 v 0+7 (D:162), 6 v 0 (D:169), 2 v 6 (D:190), 8 v 4 (D:212), 11 v 0+8 (D:250), 6 v 8+4 (D:287). Perfect Execution's [Assault 3] on the defending Rengar adds nothing (D:126.06). A 0 [M] Shadow Clone needs 1 damage (CR 142.4.b example) and every offered assignment obeys CR 467.2.c.3 / .c.4 (idx=180, 286, 287, 331).
- Kha'Zix's +2 [M] / 2 XP only with exactly one enemy unit there, on attack and on defence; XP 2, 4, 6.
- Costs that vary: Rhasa E=0 with 11 and 10 cards in the trash (D:183.04, D:210.04), offered at trash 4 with 7 runes (idx=123) and withheld at trash 3 (idx=129) and at trash 9 with no ready rune (idx=210); Noxus Hopeful E=4 as the first card (D:174.04), E=2 after Rhasa (D:219.04); Spoils of War at full cost with no enemy death that turn (D:140.04); Flow [3][R] and [1][A][A] (D:116.02, D:133.04, D:157.05); Death Mark from hand may recycle Fury or Chaos (idx=97; CR 135.2.e.6.c); Equip [P] always took a Chaos rune (boards idx=92, 208, 312) and two chosen trash cards; Rampage's [O] took a Body rune (board idx=274).
- Master of Shadows: empowered by its controller's own Flow spells leaving the chain (D:118.04, D:136.04), a trigger with no effect while already Empowered (D:160.04 -> D:162.02), its ability offered only while Empowered and ready, in the Main Phase and at Focus in a combat ([Action]), never afterwards (idx=238, 296).
- Spirit Wheel triggers on Rampage choosing a friendly unit (D:240.03) and not on the five choices of an enemy unit made while it was in play (Bounty Hunter x2, Irresistible Faefolk, First Mate) nor on a Standard Move's "move another unit".
- First Mate "ready another unit" and Perfect Execution "Ready a unit" list ready units and enemy units too (D:277.04, idx=124) — text; Targon's Peak's list (idx=102) holds exhausted runes only, but every rune on the board was exhausted, so nothing can be said about a ready rune.
- Showdown Focus: first to the player whose unit applied Contested (the turn player in all eleven showdowns and combats here, CR 345 / 466.2.c.1.a); it does not pass after a chain opened by a trigger (D:157.02, then P1 still passes first; CR 346.1); the showdown ends on two passes in sequence. P2 may act at Focus in a showdown at a battlefield where it has no unit (D:051.04; CR 347 names all players).
- Every Conquer was by the attacker (or by the only player present) at a battlefield that player had not scored that turn, one point each; Last Rites' trigger fired on its bearer's conquer (twice) and hold (D:289.09) and not for holds by other units.
- Re-channel order: recycled runes come back first-recycled-first for both players (P1: 53, 49 / 48, 51 / 45, 43; P2: 101 then 99 / 95, 100 / 104, 102).
- Final point by Hold with one battlefield held: D:289.08, game ends at once (CR 323.1).

### g19 teemo vs nasus

- Second player's first Channel Phase: 3 runes — CR 487.7 — D:013.05-013.07. Every other Channel Phase 2; P2's Rune
  Deck empty at 12/12+0 from D:245.06; recycled runes return (ids 106, 105, 101, 97 re-channeled later).
- Every `PAY_COST` against the printed cost and the runes that moved (25 plays): Energy = number of `EXHAUST_RUNE`
  lines; Power always recycled from a rune of the card's Domain (Mind for Teemo, Strategist / Singularity x2 /
  Temporal Breach / Mel / Nasus; Calm for Blitzcrank / Charm; Chaos for Nocturne). An exhausted rune recycled for
  Power — legal.
- Hide: offered only in P1's own Main Phase, only for [Hidden] cards in hand, only at a battlefield P1 controls
  (idx=39, 70, 105; none while P1 controlled no battlefield), and no second Hide at The Academy once a card was
  facedown there (idx=107, 115) — CR 811.1.b, 107.3.b. Cost prompt "pay [A] / pay [1]" — Swift Scout's first
  ability — D:100.00.
- Facedown Teemo, Strategist put into the trash when P1 left The Academy empty — CR 107.3.d, 323.5 — D:108.06.
- Control lost when the last unit leaves (Swift Scout bounce D:040.04, D:108.04; Blitzcrank's hold bounce D:157.03;
  P2's move to base D:216) and the battlefield conquered again on a later turn (The Academy by P1 on turns 2 and 4)
  — CR 323.5, 468.5.b.
- Dusk Rose Lab triggers for the turn player whoever owns or controls it, and offers the kill only with a unit of
  that player there — CR 190.6.b — D:109.09 (P2 with Nasus there), D:004.11 (nobody there).
- Nasus, Guardian of Knowledge: defending, kills Tideturner, channels 1 rune exhausted — D:102.08-104.05.
- Combat arithmetic: turn 6 Tideturner 2 vs Nasus 6; turn 8 Sneaky Deckhand 2 vs Mel 4 + Scuttle Crab 0 = 4; turn 13
  Teemo, Scout 1 vs 4 + 0 + 6 = 10 (D:264.07). Survivors' damage gone after the combat — CR 143.3.b.2.
- Damage option "Scuttle Crab(1), Mel(1)" at idx=169: 1 damage is lethal to a 0 [M] unit — CR 142.4.b and its
  example — and lethal must be assigned before the next unit (467.2.c.3).
- Sneaky Deckhand banished and re-played by Temporal Breach during its own attack is an attacker again in the damage
  step — CR 466.2.c.3.a — D:151.03-151.04. (That Temporal Breach could be played there at all is D5.)
- Temporal Breach on Mel: owner P2 plays it at Dusk Rose Lab, Mel's play ability draws for P2 — D:278.03-280.05.
- Stupefy -1 to a minimum of 1 (2 -> 1), Discipline +2 (-> 3), both gone next turn; Teemo, Scout +3 this turn (4M,
  1M next turn); En Garde +1 only, Mel not alone at The Academy (D:198.03), gone next turn.
- The Academy: hold ability for whoever holds it (P1 D:096.05, P2 D:157.06, D:191.06); the next spell that turn is
  offered [Repeat] for its base cost (En Garde [1], D:196.00); not offered for Consult the Past on turn 9 with 0
  runes left after its [4].
- Blitzcrank: "may move an enemy unit to here" asked only with an enemy unit on the board (D:119.07 yes, D:166.07
  no); returned to hand on each hold (D:157.03, D:202.03).
- Curator of the Sands offered for Tasty Faefolk ([7] unit) and for Steel Paws' [Empower] [7] (an activated ability)
  — D:210.04, D:250.06. Spell play triggers at resolution — CR 420.4.a.
- Steel Paws [Empower] [7] -> 7 [M] (board idx=277); Mel's [Empower] not offered again while Empowered (idx=179 on).
- Charm not offered with no enemy unit on the board (idx=179); Defy not offered with no spell on the chain (D:281).
- Sprite Call: ready 3 [M] Sprite with [Temporary] in the base (P1 controlled no battlefield), dead at P1's next
  Beginning Step — D:188.03, D:225.07 (the manner of the kill is D53).
- Scoring: P1 conquer 1, conquer 2, hold 3; P2 conquer 1, hold 2, conquer 3, holds 4 and 5 (two battlefields), hold 6,
  conquer 7, hold 8 — the winning point by Hold (CR 466.1.b). One score per battlefield per turn throughout.
- Mulligan: 11 options (0, 1 or 2 of 4 cards), replacements drawn before the set-aside cards go back — D:003, D:004.

## 7. Judge questions

In the format of `docs/judge-questions-2026-10.md`. J1 is asked only if the text is not taken as settled.

**J1 (G1). First draw of the first player in 1v1.** Mode: 1v1 Duel / Match. Situation: the player going first
reaches the Draw Phase of the first turn. Question: does that player draw a card? For a draw: CR 315.4.b ("The
Turn Player draws 1") and CR 487.7 / 488.7, whose First Turn Process names only the second player's extra rune.
Against: CR 489.7 / 490.7 / 491.7 print "The player going first does not draw a card during their first Draw
Phase" for the three other modes; was it dropped from 487.7 / 488.7 by mistake?

**J2 (G3). Who gains Focus when an effect moves an enemy unit.** Charm: "Move an enemy unit." Situation: on my
turn I move an opponent's unit from their base to an empty battlefield nobody controls; a showdown opens.
Question: who gains Focus as it begins — the opponent, whose unit applied Contested (CR 345, 190.3.a.1,
323.10.a), or the turn player, whose spell caused the move? (For a combat CR 466.2.c.1.a already gives it to the
attacker.)

**J3 (G23). Brush over a battlefield with a restriction.** Green Father: "... replace that battlefield with a
Brush battlefield token." Situation: I hold Vilemaw's Lair ("Units can't move from here to base.") and replace
it with a Brush. Question: can my units at the Brush now move to my base? For yes: CR 187.8 gives the Brush
exactly two abilities; CR 439.5 puts the replaced card in Banishment. For no: CR 439.1 "inheriting all effects
or statuses of the game object it replaced" and the example under 439.1.a ("the Brush inherited all statuses and
conditions"). Same question for Rockfall Path and Forgotten Monument under a Brush.

**J4 (G24). Green Father on a Brush.** Holding or conquering a battlefield that is already a Brush token: may I
exhaust Green Father to replace that Brush with a new Brush (CR 439.6 lets a token be replaced; 439.7.b.1 keeps
the original card eligible to swap back)?

**J5 (G10). Battlefield abilities at the start of a Beginning Phase.**
(a) Frozen Fortress: "At the start of each player's Beginning Phase, deal 1 to each unit here." Player B
controls it; player A's Beginning Phase starts. Who controls the ability — B (CR 190.6.a), or A because "each
player's Beginning Phase" "indicates another player"? It decides who puts it on the chain and has priority first.
(b) Dusk Rose Lab: "At the start of your Beginning Phase, you may kill a unit you control here to draw 1."
Nobody controls it when a player's Beginning Phase starts. Does the ability trigger at all? CR 190.6.b gives an
uncontrolled battlefield's abilities to the Turn Player; CR 190.6.d says that with no controller "“you” refers
to no one, and all such instructions are ignored". The engine puts an empty item on the chain every turn.

**J6 (G25). Astral Heron when the first card of the turn is countered.** "When you play your first card each
turn, if I'm at a battlefield, the next card you play this turn costs [2][A][A] less." Situation: my Heron is at
a battlefield; the first card I play is countered; my second card resolves; I play a third. Does anything
trigger the Heron this turn? (a) No: the first card was countered and triggers nothing (CR 420.4.a.1); the
others are not my first card. (b) Yes, the second: "play" for a trigger means a completed play (420.4.a).
CR 420.5 leans to (a). The engine does (a).

**J7 (G26). A play watcher that enters during the spell's resolution.** Ravenbloom Student: "When you play a
spell, give me +1 [M] this turn." Temporal Breach: "Banish a unit, then its owner plays it to the same location,
ignoring its cost." Situation: I play Temporal Breach on my own Ravenbloom Student; it is banished and played
back during the resolution (a new object, CR 124). Does the played-back Student trigger for that Temporal
Breach? For: CR 420.4.a (such abilities trigger when the play is completed by the resolution; the new Student is
on the board then, 384.2). Against: the new object did not exist when the spell was played. The engine
triggers it.

**J8. Noxian Drummer killed in response.** "When I move to a battlefield, play a 1 [M] Recruit unit token
here." Situation: the Drummer moves to a battlefield, the ability is on the chain, and the opponent kills the
Drummer in response. Is a Recruit played at that battlefield? For no: CR 359.3.f.1-f.2 ("here" is referenced
from the source and checked on execution) and 359.3.e.12 ("A unit that is no longer on the board has no
location"). For yes: CR 359.3.f.3 (information referenced from the trigger condition is fixed when the condition
is fulfilled), since the condition itself names the battlefield moved to. The engine answers no (g10 D:224).

Met again, already asked: judge question 1 of `docs/judge-questions-2026-10.md` (Targon's Peak; the opponent's
runes were readied, section 5); D28's question (Deathgrip with one friendly unit; played three times); Q2 of the
decks document (Rek'Sai and tokens).

## 8. Observations on the agents' play (not findings)

The same picture in all 19 games, in both seats (`ismcts:sims=50`, a score-difference leaf value):

- **Turns ended with everything available.** End Turn as the first action with 6 to 12 ready runes and several
  playable cards, in every game; champions and cheap cards held unplayed for the whole game (Vex, Apathetic in
  g08, Darius in g10, Sprite Fountain in g19 through 17 offers).
- **Effects aimed at the wrong side.** Bounty Hunter's [Ganking] on an enemy unit 5 of 5 (g18); Pridestalker's
  +1 [M] on an enemy unit 6 of 8, paying Deflect for it once (g04); Punch First's +5 [M] on an enemy unit in
  g04, g12 (twice) and g13; damage spells on the caster's own units in g03, g09, g11, g12; own spells countered
  (g01, g06, g08, g17).
- **A winning move not taken.** g08 idx=326 (D:292): at 7-7, with one battlefield already scored by Hold and the
  other empty, `Move Tideturner -> Rockfall Path` wins on the spot; P1 chose End Turn.
- **Battlefields given up.** Units walked off held battlefields to the base for nothing, in most games.
- **The "nothing" option.** g09: P1 answered all 8 Predict prompts with option [0] (recycle), including its
  champion. g16: P1 took the empty option repeatedly ("None" for Elder Dragon twice, Bellows Breath with no unit
  twice, "Ready a gear" over "Ready 2 gear"). g12: every Gold (6 of 6) activated at the first priority decision
  of a Beginning Step, where the Power is lost before the Main Phase.

No reader found an option missing behind any of these: each list compared with hand, runes and board was
complete (apart from the known D25 / D37 and G23). No log holds a fallback or mirror warning. It reads as a
search that cannot see these effects (the leaf value is the score difference, 50 simulations), not as a
generator defect. Two things may still deserve a look by whoever owns the agents: (1) whether a prompt's options
for "own" and "opposing" objects are told apart by the search at all (in Targon's Peak's prompt own and opposing
runes are mixed in object-id order, and the opponent's were taken 3 of 3 in g01); (2) whether option [0] of a
two-option prompt is favoured (g09's Predict, g16's empty choices).

Rendering notes from the readers (replay, not engine): two options with the same label for distinct objects
(g13 idx=217 `Assign Damage: Recruit(1), Recruit(12)` twice); `Champion Zone: (on board)` shown after the
champion went hand -> facedown -> trash (g19).

The readers' own observations, game by game:

### g01 irelia vs ornn — agents

- P1 ends turns 2 and 4 with a playable unit or gear and all runes ready; holds Stellacorn Herder (4E) unplayed through turns 12, 14 and 16 with 7 to 12 ready runes; on turn 8 ends the turn with a ready Adaptatron and an empty, uncontrolled Veiled Temple.
- Self-defeating picks, each a legal option that was really on the list: P1 plays Discipline on the enemy Ornn and counters it with its own Defy (D:093-100); P1 plays Flash choosing no unit (D:149); Adaptatron's controller kills its own Zhonya's Hourglass rather than the enemy Guardian Angel (D:126); P2 counters its own Defy (D:156, a D21 consequence); P2 readies the opponent's runes with Targon's Peak three times out of three picks that mattered (D:088, 258, 259), while its own runes were on the same list — with the opponent's and the own runes mixed in one list in object-id order, a 50-simulation search may not tell them apart; worth a look at how that prompt's options are encoded if the judges allow the choice.
- P2 walks units home off battlefields it controls (turn 9 Riven, turn 13 Aphelios + Riven) and gives up the Hold.
- No option was missing in any of these; they read as weak search, not as generator defects.

### g02 fiora vs ivern — agents

- All of these were legal choices from correct option lists (ISMCTS, 50 simulations); none points at option generation except the missing moves of N1.
- P1 turn 7: moved Sett and Fiora off Risen Altar to the base (giving up the hold), then killed its own champion Fiora for Sacrifice. P1 turn 9: `End Turn` as the first action with 9 ready runes and Poppy, Rift Herald, Divining Shells, Mobilize and two Sett moves on offer (idx=155). P1 turn 11: Divining Shells' +2 [M] given to the enemy Ivern in its own attack (idx=190); then End Turn with a ready 7M Poppy and P2 at 7 points.
- P2 turn 11: played the hidden Hidden Blade on its own Vi, then Defy, which (D21) took that Hidden Blade. P2 declined Green Father on three of four offers and Brush's swap-back once.
- P2's units at the Brush (Astral Heron, Trusty Ramhound, Vi) never moved after arriving: no move was ever offered (N1; while it was Vilemaw's Lair that was correct).

### g03 pyke vs kaisa — agents

- Both seats add resources and then end the turn without spending them: P2 Daughter of the Void D:008 -> End
  Turn D:011; D:134 -> D:137; P2 Gold + Daughter D:054-059 then a unit and End Turn; P1 two Golds D:110-126 ->
  End Turn D:127 with Bewitching Spirit affordable; P1 Gold D:241 -> End Turn D:244 with Kai'Sa affordable. Five
  times in one game: worth a look at how an [Add] activation is valued / encoded.
- Spells aimed at the caster's own side or to the opponent's benefit: P1 Falling Star's second hit on its own
  Treasure Hunter, losing Void Gate (D:083.03); P2 Falling Star on its own Noxus Hopeful (D:100.03); P2 Smoke
  Screen on its own Noxus Hopeful (D:159.03); P2 Rocket Barrage on its own Watchful Sentry (D:174.03); P1 Hard
  Bargain with Repeat on its own Void Seeker (D:189-194); P1 Cleave on the enemy Brynhir Thundersong
  (D:303.03); P1 Star-Crossed with no unit (D:111).
- P1 walks Baron Nashor off Baron Pit to base as its last action of turn 17 (D:320), and P2 wins by holding
  Void Gate next turn; P2 moved its whole board off and back onto Zaun Warrens on turns 10 and 12 for nothing.
- All of this is legal and reads as a 50-simulation search, not as a missing option.

### g04 rengar vs vi — agents

- P1 gives its own beneficial effects to enemy units most of the time: Pridestalker's +1 [M] went to an enemy unit in 6 of 8 triggers (D:027.03 Honest Broker, D:034.03 and D:061.03 Gem Jammer, D:097.03 Rengar, Unseen — paying Deflect for it, D:122.03 Honest Broker, D:158.03 Kennen), Punch First's +5 [M] went onto the enemy defender Honest Broker in P1's own attack (D:066.01, the attacker then died to 12 damage), and First Mate readied the enemy Rengar, Unseen, again paying Deflect (D:121.03-121.05). The options were all legal; it reads as choices the evaluator cannot tell apart (score difference only), not as an option defect.
- Turns ended with everything available: P1 turn 2 (3 ready runes, three playable units), turn 8 (6 ready runes, four playable units, a ready Rengar), turn 16 (9 ready runes, three playable units, Darius and Inferna ready in base against a lone 6M Kai'Sa holding Seat of Power for P2's 7th point); P2 turn 1 (3 runes, three playable cards) and turn 13 (`[D:193.01] CHOSE: [P2] EndTurn` with 12 ready runes and four playable units, idx=214).
- Units walked off battlefields for nothing: P1 moves Inferna and Darius from Seat of Power to base and loses control of it (D:195, turn 14), First Mate and Rengar back to base on turn 12 (D:150.01, D:178.03); P2 moves Vi, Kennen and Gem Jammer to base on turn 11 (D:132). Twice a single 3M unit attacked 12 Might of defenders (D:065, D:072).
- P2 spent Against the Odds on a showdown with no enemy unit (+0, D:046.05-050.02), activated Gold and never used the Power (D:183), and killed its only unit with Deathgrip (D:209).

### g05 lucian vs ezreal — agents

- P1 turn 6 (idx=83): six ready runes, Rengar (5E+1P) and Skyfall of Areion affordable, empty board; ends the turn at once.
- P1 turn 10 (idx=154): Kai'Sa (6 [M], ready) in base, both battlefields empty and uncontrolled; ends the turn without moving her (a free conquer and her draw).
- P1 never played Skyfall of Areion (in hand from turn 2) or First Mate (from turn 8), although one or both were in the option list at idx=23, 46, 83, 107, 145, 185 and 243-251.
- P2 turn 7: Star-Crossed with no enemy unit returns its own Vex (the D8 option). P2 turn 11: Wages of Pain on its own Watcher. P2 turn 15: Gust from the trash with no legal target, then Bellows Breath with no target and its Repeat paid (the D8 / D45 options). P2 turn 13: names "Shadow Isles" for The List, a tag only its own cards have. P2 turn 5: Turn to Dust on its own fresh Gold. These read as a 50-simulation search taking do-nothing options that should mostly not be in the list.
- P2 turn 9 (idx=142): Fizz and Ezreal affordable with 5 ready runes; ends the turn.

### g06 kennen vs diana — agents

- P2 counters its own Eclipse with its own Hard Bargain (D:050-058), Rebukes its own Vex and so gives up Abandoned Hall and the hold (D:251-256), and plays Stupefy on its own attacking Hwei just before the combat (D:242).
- P1 plays Gust on its own Tentacle twice (D:217, D:302).
- P2 activates Scorn of the Moon in the Main Phase or on a non-showdown chain 6 times of 7; that Energy can only be spent in showdowns and was spent once.
- P1 gives [Assault 2] to Carrion Dredger and then moves it to the base (D:273, D:310); on turn 10 it attacks 4M into 6M with the legend Empowered and `Activate Heart of the Tempest` on offer at Focus (idx=224) and passes.
- P1 ends turn 4 with four ready runes and Kennen, a Disciple and Lightning Rush on offer; P2 holds Sprite Fountain for six turns with the cost available and never plays it.
- Nothing here looks like a missing option: each of these was a choice among options that were listed.

### g07 yasuao vs khazix — agents

- P1 turn 9: plays Hard Bargain at Void Assault and then Defy, which removes its own Hard Bargain (D:097-104). Looks like play no agent would choose; it is what D21 makes of the option `Play Defy #45[S]` (no target shown, top item only).
- P1 ends turn 6 with 6 ready runes and four playable cards (Fizz, Irelia, Lonely Poro, Yasuo; idx=68), and never plays Irelia or Up from the Deep in 11 turns although both were affordable from turn 6 on. P1 plays Discipline on the enemy Zed (idx=133) and moves its only battlefield unit back to base on turn 10 (idx=144), giving up Seat of Power for nothing.
- P2: Punch First on an exhausted unit in its base outside any showdown (idx=32-33); Void Assault used so that the opponent conquers (idx=104, 119); Star-Crossed on its own Zed (idx=162); three units moved off Sandswept Tomb to base on turn 11. Tail-Cloaked Matriarch stays in hand from the mulligan to the end.
- None of these points at a missing option: every option list I recomputed was complete (apart from D25) and held nothing unaffordable or mistimed. They read as 50-simulation search noise.

### g08 mel vs vex — agents

- P1, turn 18 (idx=326, D:292.01): score 7-7, Bandle Tree already scored by Hold that turn (D:291.08), Rockfall Path empty and uncontrolled, two ready Tideturners in base, 10 ready runes. `Move Tideturner #199 -> Rockfall Path` (options [1] and [4]) conquers the only other battlefield, so the winning-point rule is met and the game is won on the spot. P1 chose `End Turn` as its first action. Two turns later it made the same move when it could not win with it (D:328.05).
- End Turn as the first action with resources and playable cards: P1 turn 4 (idx=26, 5 runes, 9 options), turn 10 (idx=117, 7 runes), turn 18; P2 turn 7 (idx=54, 8 runes, 11 options).
- P2 never played its champion Vex, Apathetic from turn 3 (first offered idx=20) to turn 21, with [4] available on most turns.
- Spells aimed at the caster's own disadvantage: P1 Rebuttal on its own Shock Blast (D:064), Gust on its own Ravenbloom Student (D:125), Stupefy on its own Tideturner (D:301); P2 Rebuke on its own Astral Heron and then Abandon on that Rebuke (D:152-D:162), Discipline on the enemy Fizz in a combat it was attacking (D:264).
- P2 repeatedly walks its last unit off a battlefield it holds and so gives up control: Steel Paws D:133.03, three units D:170.01-D:170.03, Astral Heron D:222.03 (at 7 points; no hold on turn 17). A lone 0-Might Steel Paws attacks a 2-Might Tideturner (D:150).
- Taken together the choices look close to uniform over the legal list in both seats; nothing here points at a missing or mis-encoded option other than what is listed above.

### g09 rumble vs reksai — agents

All options were present where I checked; these read as weak 50-simulation play, listed because the brief asks.
- P1 ended three turns with ready 7 [M] units in base, runes ready and uncontrolled, empty battlefields on offer: idx=244 (`[D:227.01] CHOSE: [P1] EndTurn`, both battlefields empty and uncontrolled), idx=270 (`[D:253.01]`), idx=305 (`[D:284.01]`, 8 ready runes, three playable cards, nothing played or moved).
- P1 aimed its damage at its own units: Falling Star on its two Thousand-Tailed Watchers (D:226.03-04), Singularity on its own Watcher (`[D:242.03] SPELL: Singularity (id=31) targets=[Thousand-Tailed Watcher(id=6)]`), Falling Star on the Gem Jammer it had just played (D:336.03). Cleave on an exhausted enemy Inferna in base (D:091), Eclipse on an enemy unit in base during its own Beginning Step (D:029).
- P1 moved both Watchers off Frozen Fortress back to base on turn 19 right after scoring the hold, giving the battlefield up (D:354.01-02); P2 did the same with three units at Void Gate on turn 16 (D:292.01, D:294.01, D:295.03).
- P2 played Blood Rush on the enemy Forecaster twice (D:022, D:073), ended turn 4 with 6 ready runes and four playable cards (idx=53).
- P1 answered every Predict prompt (8 of 8: D:015, D:035, D:046, D:063, D:103, D:112, D:120, D:360) with option [0] "recycle" — including recycling Rumble, Thousand-Tailed Watcher and Temporal Breach. A uniform first-option answer on a two-option prompt may be worth a look at how the search values / encodes that prompt.

### g10 leona vs darius — agents

- P1 played Hidden Blade on its own Leona twice: turn 6 with Guardian Angel attached (D:043.06 — draws 2, loses the Guardian Angel and the showdown), and turn 8 with nothing to save her (D:064.05 — the champion goes to the trash, P1 draws 2, and the empty Trifarian War Camp she was about to conquer is given up). Both are legal; the second throws away a point and the champion.
- P1's Charm on turn 8 moved P2's Kai'Sa onto an empty battlefield and handed P2 a conquer, a card (Kai'Sa's trigger) and the hold points that followed.
- P2 played its facedown Hidden Blade on its own Ferrous Forerunner (D:161) and P1 countered it with Defy.
- P2 activated Hand of Noxus with Legion off twice (D:009, D:062) for no effect.
- Turns ended with everything unused: P2 turn 7 (8 ready runes, 12 options, `End Turn`), P1 turn 2 (4 runes), P1 turn 14 (9 ready runes, five playable cards, `End Turn` at idx=259).
- P2 never played Darius, Trifarian (offered at every Main Phase decision it could pay from turn 3), Trifarian Gloryseeker (in hand from turn 5) or the second Kai'Sa. With 50 simulations this may be search weakness rather than an option or encoding defect; the options themselves were present each time.

### g11 lux vs draven — agents

- P1 ends its turn at once with a full hand and 7-12 ready runes on turns 7, 15, 19, 22 and 24 (idx=75, 196, 286, 343, 394) — turns 19 and 22 are its two Additional Turns. Seal of Insight, Honest Broker, the second Sprite Fountain and Progress Day were offered in 9 Main Phases and never played. P1 also walked all its units off a held battlefield (idx=162-164).
- Both seats aim spells against themselves: P2 Cleave and Blood Rush on enemy units (idx=125, 166), Falling Star on its own Tentacle (idx=138), Star-Crossed on its own Draven (idx=267), Switcheroo between two enemy units (idx=362-363); P1 Stupefy on its own Sprite and Watcher (idx=16, 253), Drag Under on its own Plundering Poro in a combat (idx=350), a Gold spent on the opponent's turn with nothing to pay (idx=289). All were legal options; with 50 simulations this looks like search noise, but the "@ target" options of one spell may be hard for the agent to tell apart.

### g12 sivir vs viktor — agents

- P1 activated every Gold it had (6 of 6: D:057, D:079, D:142, D:201, D:324, D:325) at the first priority
  decision of its own Beginning Step, where the Power cannot be used and is lost at the start of the Main
  Phase. Never once held for a payment.
- Targets look unconsidered: Punch First (+5 [M]) was put on an ENEMY Recruit twice (D:148.01 during P1's own
  attack, D:288.01); P2 aimed Bellows Breath at its own Recruit (D:038), Singularity at its own Recruit + Vi,
  Peacekeeper with an enemy Sivir legal (D:172.03), a second Singularity at its own Bird alone (D:216.03), and
  played Bellows Breath with no target (D:239.06); P1 killed Sivir rather than Pyke to Dusk Rose Lab at 7 points
  (D:323). `CLAUDE.md` says a search agent whose mirror cannot reproduce the live game answers uniformly at
  random at every decision that is not a top-level main-phase one — the prompt answers here would fit that,
  but three of the above are top-level main-phase intents (target sets carried by the Play intent), so it may
  be only the 50-simulation search. Worth a check of whether the mirror fell back in this game.
- P2 held Xin Zhao, Vigilant from turn 2 to the end and never played it (13 option lines offer it); Viktor,
  Leader was offered in P2's Main Phases from turn 2 on (13 option lines) and never played.
- P1 ended turn 5 with 6 ready runes and six playable cards, and turn 13's last decision with 6 ready runes
  and 15 options, taking End Turn.

### g13 renata vs poppy — agents

- P1 turn 6: Hidden Blade on its own champion Renata and facedown Wages of Pain on its own Soaring Scout (D:052-061) — kills both of its units and gives up the battlefield it held.
- P2 turn 10 (defending): Punch First on an enemy attacking Recruit (`[D:111.01] TARGET (play): Punch First -> Recruit`), raising the attack from 3 to 8 against Poppy's 7; Poppy dies and P1 conquers. Option [0] of a 5-option target list.
- P2 turn 5: End Turn with 7 ready runes, three affordable units in hand and a ready Poppy that could move (idx=43). Turn 7: plays one 2-cost unit and ends with 7 ready runes, Poppy still unmoved (idx=70).
- P1 ends turns with resources unused: turn 10 with 7 ready runes and four playable cards (idx=132); turn 14 with 10 ready runes, five playable cards and `Activate Vanguard Armory` offered (idx=218); turn 16 leaves the new Vanguard Armory unactivated twice (idx=293, 298) and sends a lone Recruit into three defenders.
- P1 turn 12: activates both Gold with nothing to pay for (pool Power 2 unused, D:162-167). Turn 16: Jayce kills the ready Vanguard Armory rather than the exhausted Gold (idx=285).
- Rendering: idx=217 lists two options with the same label (`Assign Damage: Recruit(1), Recruit(12)` twice) and idx=296 two `Master Yi, Tempered #675(1)` — distinct objects, indistinguishable to a reader.

### g14 yi_rebuild vs ahri — agents

- Both seats repeatedly pointed effects at the wrong side although the other choice was in the same list: P2
  Discipline on P1's Pit Rookie (D:013, forced target — the play itself was the choice), P2 Stupefy on its own
  Janna (D:092), P1 Discipline on enemy Janna (D:171), P1 Punch First on Yi in base during a showdown he was not in
  (D:135), P1 facedown Back Off on its own Sand Soldier (D:187). All were picks among several legal options or a
  pass, so they read as 50-simulation noise, not an option-list defect.
- P2 used Amateur Recital to move its own last unit off the Recital twice (D:078 Ravenbloom; D:193 Blue Sentinel),
  losing control each time and re-taking it later the same turn for no point (CR 472).
- P1 never moved a ready 6M Rengar from base on turn 7 (idx=115-127: `Move Rengar -> Emperor's Dais` (empty) was
  offered three times) and ended turns with affordable plays left; P2 ended turn 10 with 8 ready runes and three
  playable cards (idx=235). The options were present each time.

### g15 leblanc vs akali — agents

- P2, turn 10 (idx=138): End Turn as the first action with 10 ready runes, four ready units in base, an empty
  uncontrolled Star Spring and 19 options. Turn 4: hid Mischievous Marai at Star Spring (idx=40), then moved its
  only unit there to base (idx=41), losing the battlefield and the hidden card at once.
- P1, turn 7 (idx=116): ended the turn with a ready unit (the Marai copy) in base and Star Spring empty and
  uncontrolled. P1 never moved a unit to a battlefield after turn 3 and never played Glasc Mixologist, Ruined Rex,
  Chakram Dancer, B.F. Sword or Kennen although they were offered turn after turn (idx=99, 130, 137, 139, 185,
  193, 196).
- P1 played Ki Barrier on the opponent's Kai'Sa (D:095.01), and Cull the Weak twice with one unit of its own each
  time, the first killing its own 7M Thousand-Tailed Watcher (D:179.03).
- All of these were choices among complete option lists (checked); they read as weak 50-simulation play, not as
  missing options.

### g16 jayce vs ambessa — agents

- P1 (jayce, ISMCTS 50) never moved a unit to a battlefield by choice in 9 turns: its only point by play came from being pulled into a combat. At idx=248 it moved Jayce OFF the battlefield it held (D:240.03), and at idx=313 and idx=319 it ended the turn with Jayce ready in base and Risen Altar empty (`[1] Move Jayce ... -> Sigil of the Storm` `[2] Move Jayce ... -> Risen Altar` on offer).
- P1 took the empty or null option repeatedly: Bellows Breath with no unit chosen twice (D:024, once paying the Repeat; D:241), "None" for both Elder Dragon locations when 1 damage from it kills (D:315, D:316), "No" to Jayce's ready with six exhausted runes (D:173), "Ready a gear" over "Ready 2 gear" (D:129), an opponent's rune as the thing to ready (D:302), End Turn at turn 9 with 7 ready runes and five affordable plays (idx=99). It also exhausted its Eggs for Energy in the Beginning Step on four turns, where the Energy is lost before the Main Phase. With 50 simulations this may be search noise, but the option at index 0 / the "nothing" option being taken this often is worth a look at how such options are valued or encoded.
- P2 (ambessa) ended turn 4 with 6 ready runes and five playable cards unplayed (idx=31), left Risen Altar empty and unconquered from turn 6 to turn 14 with ready units in base, cast Punch First twice on a unit that was not in any combat (D:108, D:262), and shuttled units between Sigil of the Storm and base most turns.

### g17 lillia vs azir — agents

- P1 countered its own spells with Defy twice (En Garde at D:089-097, Smoke and Mirrors at D:179-183, losing its "Draw 1").
- P2 let a ready Azir (7M on turn 7, 5M on turn 9) stand in the base with both battlefields empty and ended the turn (idx=124, idx=135: turn 9 with 6 ready runes and nothing done); on turn 11 it moved Azir in and killed him itself with Deathgrip before the combat (possible only through D28).
- P1 ended turn 8 with a ready Lillia in the base and two open battlefields (idx=132), and turn 6 with 2 ready runes and a playable Ravenbloom Student (idx=114).
- P2 declined Weaponmaster's free Brutalizer attach both times (idx=87, idx=181) and on turn 11 then paid a rune for the same Equip one decision later (D:164.03) — a strictly dominated line; the prompt's options are rendered `No — …` / `Yes — …`.
- With 50 simulations these look like weak search rather than missing options: every option list I compared with hand, runes and board was complete.

### g18 missfortune vs zed — agents

- Whenever a beneficial effect could be put on any unit, the search agent put it on an ENEMY unit: Bounty Hunter's [Ganking] 5 times out of 5 (Tideturner, Shadowblade Lurker, Shadow Clone, Noxus Hopeful twice — D:070.01, D:099.01, D:164.01, D:203.01, D:255.01), First Mate's ready on an already-ready enemy Rhasa (D:278.03), and P2's Perfect Execution (ready + [Assault 3]) on P1's Rengar (D:116.01). Seven of seven; with 50 simulations and a score-difference leaf value these effects may simply be invisible to the search, but it is also what a prompt whose "own card" / "opponent's card" slots were confused would look like.
- P1 held Onslaught (4 Energy) from turn 6 to the end and never played it, with the Energy for it on turns 8, 12, 14 and 16.
- P1 twice walked its only units off a battlefield it controlled (turn 8: both Kha'Zix to base, turn 12 and 16: Kha'Zix to base), and on turn 12 then paid for Miss Fortune to take the same battlefield back for no point.
- P2 never played Zed until turn 15, though payable from turn 3, and declined both Last Rites conquer triggers.

### g19 teemo vs nasus — agents

- P1 never played Sprite Fountain: in hand from the mulligan (D:003.05) to the end, offered and payable in 17 Main
  Phase decisions (idx=7, 31, 58, 63, 105, 107, 115, 119, 147, 202, 208, 247, 252, 258, 293, 297, 308), never taken.
- P1 used Swift Scout three times to take its own Teemo back: turn 2 from The Academy straight after conquering it
  (giving up the hold); turn 6 again from The Academy, which also sent the Teemo, Strategist it had hidden there one
  decision earlier to the trash (D:108.06); turn 14 from its base.
- P1, turn 10 (idx=208): End Turn with a ready [Temporary] Sprite in base, Dusk Rose Lab empty and uncontrolled, 4
  ready runes — a free conquer not taken, and the Sprite dies next turn.
- P1 Temporal Breach on its own attacker (turn 8, no effect) and on P2's Mel (turn 14, P2 draws a card).
- P2 declined Curator of the Sands both times it really applied (D:211.03, D:251.03) with exhausted runes to ready;
  P2 played Discipline on the enemy Teemo (turn 1); P2 moved Mel and Scuttle Crab off The Academy to base on turn 11,
  giving up a hold, and still won.
- All of this looks like a 50-simulation search, not a withheld option: every one of these decisions had the better
  option in its list.
- Rendering only: `P1 | >> Champion Zone: (on board)` is shown to the end of the game although the chosen champion
  went hand -> facedown -> trash (D:040.04, D:099.04, D:108.06).
