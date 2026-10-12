# Testing phase after the 2026-10 review fix run

Decided with the user on 2026-10-11. The fix run closed every item of the review; this phase looks for
what the review and two decks could not see. It starts from `master` after the final merge.

## Why

Real-game coverage during the fix run was two decks (Fiora, Yi). The first quick run with three more deck
lists found an engine error in real games within 40 games per pairing (Switcheroo and a Deflect paid ahead
of a spell's cost, plans/09 item 9.VV1). Unit tests play one card on a board the author imagined; they do
not find the board a real game wanders into.

## What is run

1. **The constructed deck lists in `decks/`** (18 on 2026-10-11: akali, azir, diana, ezreal, fiora, irelia,
   kaisa, khazix, leblanc, mel, reksai, rengar, teemo, vex, viktor, yasuao, yi_rebuild, zed). Every pairing
   and seating, random agents and ISMCTS, on the optimised build and on the checked build
   (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`). These test cards the way they are played: per deck, each card's
   behaviour in a traced real game is read against its text and `rules/core-rules.md`.
2. **Randomly built legal decks from the whole pool.** Decks chosen by seed (legend, its champion, a
   domain-legal main deck, battlefields, runes; no banned card), random against random, at least 100,000
   games on the optimised build and 10,000 on the checked build. This tests breadth: every card, next to
   cards nobody would pair it with. Every `engine_error` is a finding recorded with deck seed, game seed and
   message. Random play cannot tell that a card did the wrong thing legally; it finds crashes and broken
   contracts.
3. **Coverage.** For each of the 955 registered cards: games in which it was in a deck, was played or put in
   play, had an ability trigger or be activated, had that ability resolve. Every card with a zero is listed
   with the reason where it is obvious.

The fuzz is kept as a re-runnable tool (a non-default target or a test with a small default game count),
documented in `CLAUDE.md`.

## How fixes are made

Batched (user decision). Findings are collected first, with seeds. They are then fixed together on a
branch — or a few branches by area — so that there is one rebuild and one gate per batch, not one per fix.
A batch is merged to `master` after its gate. Each fix is red first on the engine path, as in the fix run.

Gate baseline from `master` at the end of the fix run: 200-game smoke (fiora vs yi_rebuild, seed 20261010,
6 threads) P1 138 / P2 62 / 0 draws, 17 turns, 295 decisions.

## Leads already known

From the last stream report of the fix run (`plans/reports/checkpoint-followup-2.md`):

- `ChainManager::payDeflectForChoices` (spells offered once per target; Deflect paid as the item goes on the
  chain) still pays through the plain rule. Not established whether it runs before or after such a spell's
  cost; if before, it has the fault 9.VV1 fixed for pre-play choices.
- `GameEngine::canAffordWith` treats Energy and Power as independent. That is right only while every
  payment exhausts before it recycles; a payment that recycles a ready rune ahead of another cost breaks an
  offer the same way.
- The deck files were untracked in the main checkout on 2026-10-11. Commit them when the phase starts so
  that the decks a result came from are pinned.

## Not part of this phase

Play UI (`docs/play-ui-followups.md`), the performance recommendations (`docs/perf-followups.md`), and the
v6 ML tooling.

## The fuzz is iterated (user decision, 2026-10-11)

The first 100,000-game pass found two engine defects and a validator gap, so the fuzz is run in rounds:

1. a pass with fresh seeds on both builds;
2. failures grouped, each with a one-command reproduction; nothing fixed during a pass;
3. one fix batch on a branch, one rebuild, one gate, merged to master;
4. the next pass, compared with the last (groups gone, groups new, coverage per card).

Later rounds use coverage-guided decks (weighted toward the least-exercised cards) and a seat that plays
cards rather than passing, because uniform random play leaves rare cards and multi-step abilities rare. The
stopping rule is set in numbers from the fuzz stream's final report (consecutive clean passes of a stated
size, and a minimum number of resolutions per card).

## Focus for the next deck round: Flow (user request, 2026-10-11)

`decks/kennen.txt` (Heart of the Tempest / Kennen, Storm of Shuriken) is built around [Flow]; use it to
check Flow end to end. First look, 20 traced random games of the list: the legend granted [Flow] 16 times,
but a spell was actually played for Flow only twice, so random play barely exercises the replay itself.
Run this list with the `random-player` agent (it plays rather than passes) and with ISMCTS in one seat, and
read against the text and the rules (CR on Flow; `Card::flowCost`, `GameEngine::refreshFlowGrants`):

- when the spell that gets [Flow] is chosen (finding D78 says at resolution; the rules say finalization);
- which spells are offered, and that the grant lasts "this turn" only;
- that a granted or printed Flow play is offered from the trash only, for the right cost (Energy and Power
  by domain; "equal to its cost" for Kennen's grant), once, and at the spell's own timing;
- that the spell is banished instead of trashed after a Flow play, including when it is countered;
- that its choices, targets, Deflect and Repeat work as from hand;
- "when you play a spell" / "not from hand" watchers on a Flow play; Death Mark 931's own Flow.

## The constructed-deck round robin (plan agreed with the user, 2026-10-11)

Run after fix batch 1 is merged, over every list in `decks/` that validates (34 on 2026-10-11; 36 when
`ambessa.txt` and `azir.txt` are corrected). Every ordered pairing, both seatings, mirrors included
(1,296 pairings at 36 lists), fixed seeds, optimised build for volume and the checked build on a subset.
Game counts of the two fast passes are doubled at the user's request; the `ismcts` pass stays at 2-4 per pairing (it is the bottleneck).

| Pass | Agents | Games per pairing | Purpose |
|---|---|---|---|
| Volume | random vs random | 400 | crashes, loops, no-winner games; a baseline win matrix |
| Active play | `random-player` vs `random-player`, and mixed with random | 400 | cards played and abilities fired |
| Purposeful play | `ismcts` in one seat, then both | 2-4 | cards used with intent, for the reading |

Recorded: every failure with decks and seed, grouped by cause; the win matrix by seat (under random play an
anomaly detector, not a strength measure); per-deck game length and decisions; per-card played / triggered /
resolved; first-player advantage. Sideboards are exercised as a separate set of pairings (each list with its
sideboard exchanged into the main deck), not mixed into the matrix.

Reading: only cards not read in the first round (261 cards from 18 lists) get a full read against a traced
game; a card with a tracked finding is re-read after its fix in the game that showed the problem; Flow
through the Kennen list is the named focus.

A larger `ismcts` vs `ismcts` matrix as a training baseline is a separate, later run.
