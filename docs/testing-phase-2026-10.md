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
