# Report: test-games19 (19 paired games, card text look-up)

**26 new findings (G1-G26), 42 of the 93 known items seen again (881 sightings counted), 0 of the 19 games
without a new finding** — G1 is in every game; leaving G1 aside, every game still has at least one new finding
or a new card on a known root. Find and record only: no engine, card or test code was changed.

- Branch `test/round2`, head `c838151` (worktree `.worktrees/round2`), not pushed. Engine binary
  `v0.1.0+2d59c14`, i.e. BEFORE the testing-phase fixes.
- Document: `docs/testing-findings-games19-2026-10.md` (in the worktree). Scripts: `scripts/testing/games19_pairing.py`,
  `games19_cardsheet.py`, `games19_reading.py`, `games19_check.py`.
- The test suite was not run and nothing was built: this stream changes no code.
- Scratch (logs, HTML replays, reading files, the 19 readers' notes):
  `/tmp/claude-1000/-home-pirateporkchop-code-riftbound-engine/b5224b1b-642c-4612-8b9a-069a6f1dcf2a/scratchpad/test-games19/`
  — `tr/*.log`, `tr/html/<game>/<timestamp>/replay.html`, `read/gNN.read.txt`, `notes/gNN.md`.

## The 19 games

Pairing seed 20261011 (`random.Random`, sorted names shuffled, neighbours paired, a second draw for the seat);
game k has engine seed 2026101100 + k; `ismcts:sims=50` in both seats. All 19 ended with a winner at 8 points
and no engine error; no replacement seed was needed. Game 7 was replayed from its command line: identical output.

| # | deck1 (P1) | deck2 (P2) | seed | winner | score | turns | decisions |
|---|---|---|---|---|---|---|---|
| 1 | irelia | ornn | 2026101101 | P2 | 5-8 | 17 | 289 |
| 2 | fiora | ivern | 2026101102 | P2 | 3-8 | 12 | 209 |
| 3 | pyke | kaisa | 2026101103 | P2 | 6-8 | 18 | 355 |
| 4 | rengar | vi | 2026101104 | P2 | 4-8 | 17 | 260 |
| 5 | lucian | ezreal | 2026101105 | P1 | 8-2 | 16 | 307 |
| 6 | kennen | diana | 2026101106 | P1 | 8-5 | 14 | 355 |
| 7 | yasuao | khazix | 2026101107 | P2 | 4-8 | 11 | 174 |
| 8 | mel | vex | 2026101108 | P1 | 8-7 | 22 | 405 |
| 9 | rumble | reksai | 2026101109 | P2 | 6-8 | 20 | 393 |
| 10 | leona | darius | 2026101110 | P2 | 2-8 | 15 | 259 |
| 11 | lux | draven | 2026101111 | P1 | 8-7 | 26 | 411 |
| 12 | sivir | viktor | 2026101112 | P1 | 8-4 | 17 | 361 |
| 13 | renata | poppy | 2026101113 | P2 | 3-8 | 17 | 304 |
| 14 | yi_rebuild | ahri | 2026101114 | P2 | 4-8 | 14 | 295 |
| 15 | leblanc | akali | 2026101115 | P2 | 2-8 | 14 | 206 |
| 16 | jayce | ambessa | 2026101116 | P2 | 2-8 | 18 | 329 |
| 17 | lillia | azir | 2026101117 | P1 | 8-0 | 12 | 206 |
| 18 | missfortune | zed | 2026101118 | P2 | 6-8 | 17 | 334 |
| 19 | teemo | nasus | 2026101119 | P2 | 3-8 | 15 | 311 |

Replay: `RIFTBOUND_ROOT=. ./build/riftbound --deck1 decks/<a>.txt --deck2 decks/<b>.txt --agent1 ismcts:sims=50 --agent2 ismcts:sims=50 --seed <seed> --trace --debug`

## Coverage

One read-only reader per game, 19 in all (the user's decision; 17 started at once, the last two as soon as the
20-agent limit freed a slot). Every reader states that every decision of its game and the log tail were read:
5,763 decisions, nothing left unread. The lead looked up all 469 trace lines the readers quoted (467 found; the
other 2 were not quotes and are not used), read the rule behind every new finding in `rules/core-rules.md`,
compared all 516 cost payments with the printed costs (22 differ, each explained by a printed discount, a Fizz
play or the known D18) and checked scoring, the winning-point rule and channeling by script (clean). The 285
trace lines quoted in the document all exist in the game they are cited from (`games19_check.py cite`).

Limits: a game shows only the cards drawn and played (Kennen's granted Flow, D78, never came up in game 6);
the reading file is omniscient, so hidden information is judged only from which lines exist; the readers read
the 93 one-line items in full but the detail sections only for the items they matched against.

## New findings, by root cause, with the fix batch

Batches as in `plans/remaining/10-testing-phase.md`: 1 triggers and chain, 2 combat timing and continuous
effects, 3 targets / finalization / payments, 4 cards / options / validator. 18 CERTAIN, 5 LIKELY, 3 QUESTION.
"known root" = the defect pattern is a known item, on a card that item does not name.

| id | finding | confidence | batch | games |
|---|---|---|---|---|
| **Turn structure** | | | | |
| G1 | 1v1: the player going first is made to skip the first draw. CR 487.7 / 488.7 give 1v1 only the second player's extra rune; the no-draw sentence is in 489.7-491.7 (multiplayer). The engine cites "CR 482.7" (old numbering); `BurnOutScoringTest.FirstPlayerFirstTurn_DrawPhaseSkipped` pins it | CERTAIN | 4 | all 19 |
| G2 | Time Warp on the opponent's turn: the Additional Turn replaces the owner's regular turn instead of being inserted before it (CR 735, 737) — the spell does nothing | CERTAIN | 4 | g11 |
| G3 | The turn player always gets Focus first, not the player who applied Contested / the attacker (CR 345, 466.2.c.1.a) | CERTAIN combat, LIKELY non-combat | 2 | g07 g10 g14 g19 |
| G4 | Setup: the battlefield is an agent decision; a Duel (CR 487.5) picks at random, a Match (488.5) chooses — which mode is an engine game? | QUESTION (to the project) | 4 | all |
| **Cleanup after a chain has emptied** | | | | |
| G5 | A unit with lethal damage is still on the board at the Focus decision after the attack / defend triggers resolved (Forgefire Cape); Charm then chose and moved it | CERTAIN | 1 | g01 |
| G6 | A battlefield with no units keeps its controller through a Main Phase decision when the last unit's Deathknell was queued by the cleanup that killed it (CR 323.5) | CERTAIN | 1 | g13 |
| **Continuous effects** | | | | |
| G7 | Sivir, Mercenary: the turn-scoped +2 [M] / [Ganking] is still applied through the next turn's Beginning Step | CERTAIN | 2 | g12 |
| G8 | Mechanized Menace "Your Mechs have [Shield]" is missing when a combat goes straight to damage (known root D42 / D60 / D76); it changed a conquer and the result of game 9 | CERTAIN | 2 | g09 |
| **Triggers** | | | | |
| G9 | Eight more cards put an ability on the chain with their printed condition false (known root D51): Curator of the Sands (14 empty items in one game), Lady of Luminosity - Starter, Gutter Palace, Patched Porobot, Poppy, Paragon, Pyke, Dockside Butcher, Blitzcrank, Impassive, Vanguard Captain ([Legion]) | CERTAIN (Vanguard Captain LIKELY) | 1 | 9 games |
| G10 | A battlefield's start-of-Beginning-Phase ability is given to the turn player while the other player controls the battlefield (Frozen Fortress, Dusk Rose Lab; CR 190.6.a, 190.6.d) | LIKELY | 1 | g05 g17 |
| G11 | Heart of the Tempest: no trigger for a play from the Champion Zone or a play by an effect from the trash | CERTAIN | 1 | g06 |
| G12 | Heart of the Tempest triggers as the Flow spell goes on the chain, not when it has resolved (CR 420.4.a) | CERTAIN | 1 | g06 |
| G13 | Tornado Warrior "Disempower it at end of turn" is a flag flipped in the Expiration Step, no ability in the Ending Step (Sanction by code) | LIKELY | 1 | g07 |
| G26 | Ravenbloom Student played back by Temporal Breach triggers on that same Temporal Breach | QUESTION | 1 | g14 |
| **Targets, finalization, payments** | | | | |
| G14 | A chosen target that left the board and came back is still acted on: Amateur Recital moved a unit Temporal Breach had banished and replayed (CR 359.3.e.4 in terms) | CERTAIN | 3 | g14 |
| G15 | Activated abilities with a target or mode pay their whole cost before the choice (Heart of the Tempest, Defender of Tomorrow, Matriarch of War, Bounty Hunter; CR 403-405) | CERTAIN | 3 | g06 g16 g18 |
| G16 | A play-time-target spell played by Fizz, Trickster is paid for before its target is chosen (Punch First) | CERTAIN | 3 | g12 |
| G17 | Emperor's Divide: the units to move are chosen after RESOLVE; the unit named at the play only stands for the battlefield | LIKELY | 3 | g10 |
| G19 | Defender of Tomorrow is activated, paid and put on the chain with nothing to choose (known root D67) | CERTAIN | 3 | g16 |
| G20 | Public Execution is offered, paid ([5] + 2 Power by Flow) and put on the chain with no legal pair (known root D8 / F7; the gate must test pairs) | CERTAIN | 3 | g16 |
| **Cards and options** | | | | |
| G18 | Defender of Tomorrow: "Ready a gear" refuses a ready gear (its pick list is narrower than its text) | CERTAIN | 4 | g16 |
| G21 | Kennen, Keeper of Balance played from facedown stuns a unit in the enemy base (known root D70; CR 811.1.d.3) | CERTAIN | 4 | g04 |
| G22 | Hidden units (Tornado Warrior, Tideturner) are offered and played onto Rockfall Path | CERTAIN | 4 | g08 |
| G23 | A Brush that replaced Vilemaw's Lair keeps "Units can't move from here to base" (slot flags survive the replacement; Rockfall Path / Forgotten Monument by code) | LIKELY | 4 | g02 |
| G24 | Green Father's "may" is withheld when the battlefield is already a Brush | LIKELY | 4 | g02 |
| G25 | Astral Heron: with the first card of the turn countered, no card of that turn triggers it | QUESTION | 4 | g08 |

Each has its card text, rule, quoted trace lines, decision and "should have happened" in section 4 of the document.

## Known items seen again

42 ids, 881 sightings counted (a lower bound for the engine-wide ones). Most frequent: D4 151 (19 games), D1
109 (18), D9 107 (19), D2 75 (12), D13 49 (16), D40 48 (16), D11 35 (11), D5 23 (10), D15 21 (9), D3 20 (11),
D21 20 (7), D23 19 (4), D8 17 (5), D44 17 (4), D53 16 (6), D10 15 (6), D51 13 (5). Also seen: D14, D18, D19,
D20, D22, D25, D27, D28, D31, D32, D33, D35, D36, D37, D38, D39, D41, D45, D46, D54, D58, D66, D67, D72, D76.
Table by id and game, and each reader's list with decisions, in section 5.

Sightings that show more than the known item says (section 5): Gold's "Kill this" is paid at resolution (D38's
pattern, 5 games); [Add] as a chain item on five more cards, once with a changed outcome (g06: Hard Bargain
countered a spell its controller could have paid for); Focus passing after an [Add] on four more cards (D31);
a Flow replay offered at Focus (D5); the Heron's discount spent on the OPPONENT's turn (D18); players countering
their own spell because Defy takes the top item only (D21, 5 times); Pyke, Returned queued twice against "Once
each turn" (D27); the turn player's trigger placed between the opponent's (D40); Relentless Pursuit joins D3 /
D41; Deathgrip with one unit was played three times (D28); D76 decided a combat in g14; Targon's Peak readied the
opponent's runes (pending judge question 1).

## Judge questions

Eight, written out in section 7: J1 first draw in 1v1 (only if the text is not taken as settled), J2 who gains
Focus when an effect moves an enemy unit, J3 a Brush over a battlefield with a restriction, J4 Green Father on a
Brush, J5 who controls a battlefield's Beginning Phase ability (and whether Dusk Rose Lab triggers when
uncontrolled), J6 Astral Heron after a countered first card, J7 a play watcher that enters during the spell's
resolution, J8 Noxian Drummer killed in response to its move trigger.

## Observation on the agents (not a finding)

Both ISMCTS seats played close to uniformly at random in all 19 games: End Turn as the first action with 6 to 12
ready runes and playable cards, beneficial effects on enemy units (Bounty Hunter's [Ganking] 5 of 5,
Pridestalker's +1 [M] 6 of 8), own spells countered, a winning move not taken (g08 idx=326). No reader found a
missing option behind it and no log has a fallback warning, so it reads as the search (score-difference leaf, 50
simulations) not seeing these effects. Two things may deserve a look by whoever owns the agents: whether a
prompt's own and opposing objects are told apart (Targon's Peak: opponent's runes taken 3 of 3 in g01), and
whether option [0] of a two-option prompt is favoured (g09: 8 of 8 Predict prompts answered "recycle").

## For the orchestrator

- **Items closed / not closed:** none owned; this stream records.
- **Needs another stream:** every G above, by the batch in the table. G1's fix must also correct the test that
  pins the skip. G9 is one root, eight card classes. G5 and G6 may share a root (no full cleanup once a chain has
  emptied on those paths).
- **Deviations:** the games were run under the build lock before the instruction to use the run lock arrived
  (the one replay check used the run lock). Two readers (g02, g13) each ran one read-only `git log -1` in the
  worktree against their brief; nothing else was run or written by a reader outside its notes file.
