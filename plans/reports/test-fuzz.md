# test-fuzz — final report

## 1. Result

- Branch `test/fuzz`, head `bf56bc8`, rebased onto `master` at `0844be3` (master had moved by documents
  and scripts only; no conflict). Not pushed, not merged.
- **Nine findings (F1 to F9) and one validator finding (V1), none fixed**, in 4.3 million fuzz games. The
  full list with reproductions, 30-line traces and readings is
  `docs/testing-findings-fuzz-2026-10.md` in the worktree.
- Gate at `bf56bc8`, optimised build: `riftbound_tests` **3083 passed, 0 failed** (3065 + 18 new),
  `riftbound_openspiel_tests` 27 passed, clone-equivalence 10/10, smoke 200 games **P1 138 / P2 62 / 0
  draws / 295 decisions**, 0 warnings. A clean rebuild (`--target clean`) was done for the same code one
  commit of documents earlier (`065248f`), with the same results.
- Checked build (`build-check`): `riftbound_tests` 3083 passed. Not run on the checked build: the
  OpenSpiel tests, clone-equivalence and the smoke. Its binaries were built before the rebase, from the
  same sources.
- No engine or card behaviour changed. Three additions touch `src/`: `rules/random_deck.{h,cpp}` (new),
  `agents/random_player_agent.h` (new), and the registration of the new agent in `main.cpp` (spec parser,
  `--help`) and in `GameRunner`'s fallback factory (one new branch each).

## 2. Findings

Counts per 1,000,000 games on random decks, optimised build: random / random-player (pass=0.2) /
random-player:pass=0. Readings are mine and not verified by a fix.

| | Finding | Count | Reading |
|---|---|---|---|
| F1 | Viktor, Innovator (117) triggers on its own Recruit token; the chain never empties | 6,718 / 8,377 / 5,271 | registers `WhenYouPlayAUnit` for "play a card"; token plays fire it |
| F2 | Alpha Strike (751): finalize pass ends before its marker | 188 / 252 / 235 | `if (targets.empty() ...) return;` when played by an effect or given new choices (Blind Fury, Promising Future, Mystic Reversal) |
| F3 | Board state written to an object that has left the board; checked build only | 32 / 53 / 95 per 100,000 checked games | Green Father's battlefield replacement skips `leftBoard`; Fading Memories, Vi, Hotheaded and Pendulum Blade act on an object that has left |
| F4 | Emperor's Divide (366) re-offers a unit whose move to base is blocked: 1,024 prompts | 0 / 0 / 382 | blocked unit stays in the list and is logged as moved |
| F5 | "You may pay [N] to <choose a unit>" triggers: the target's Deflect is paid first, the [N] is then unpayable | 3 / 6 / 15 | Kennen, Keeper of Balance; Sinister Poro; Icevale Archer. The `canAffordWith` lead of follow-up 2, in triggered abilities |
| F6 | "Kill a friendly unit" additional cost, the death replaced for a price: spell left unpayable | 0 / 5 / 9 | Sacrifice with The Boss; Heedless Resurrection with Altar of Blood |
| F7 | Strike Down (429) offered with no enemy unit to choose | 4 / 3 / 4 | `hasLegalTargets` checks half of the pair |
| F8 | Imposing Challenger and Irresistible Faefolk move each other without end | 0 / 0 / 9 | two "may"s answering each other; legal by the rules, but the engine has no answer except F1's silent drop |
| F9 | Star Spring (771): finalize pass ends early | 0 / 1 / 0 | two returns of its own before the marker |
| V1 | `DeckValidator` accepts three copies of a [Unique] card (CR 825.3.a) | 3 cards | `checkCopyLimits` knows `unlimited_copies` only |

Things the orchestrator should know beyond the table:

- **F1 is worse than a failure count.** Outside the driver's stop the engine does not fail: the chain's
  1,000-pass bound drops the chain with one warning line and the game goes on with about 1,000 Recruits.
  I saw 21 GB of memory in one such game (of 23 on the machine), over 300 s inside one decision in another,
  and 3,993 prompts in a row in a third. Random-deck self-play meets this in about 1 game in 150.
- **F3 is invisible on the optimised build**: the object silently keeps the state. The message names the
  object written to, not the writer; the split by writer (Green Father 67, Fading Memories 47, Vi 17,
  Pendulum Blade 14 of 145 example games) is exact for Green Father and Vi and an attribution for the
  other two (I read one game of each label).
- **The 18 deck lists**: 388,800 games (324 pairings x 200, three agents, both builds), one failure, the
  same game on both builds: F5, Kennen in `leblanc.txt` against Irelia's [Deflect] in `irelia.txt`, with
  pass=0 only.
- **Leads of follow-up 2**: `payDeflectForChoices` produced no failure. The `canAffordWith` lead is F5.
- No game hit the turn cap, ended without a winner or a reason, hung or ran out of memory once the chain
  stop was in.

## 3. What was run

| Run | Build | Games | Failures (random; pass=0.2; pass=0) |
|---|---|---|---|
| 50,000 seed decks x 20, seed 20261012 | optimised | 3 x 1,000,000 | 6,913; 8,644; 5,925 |
| 5,000 seed decks x 20, seed 20261011, plus pass=0.2 against random | optimised | 4 x 100,000 | 707; 859; 528; mixed 788 |
| guided against unguided, seed 20261014 | optimised | 2 x 100,000 | 674 unguided; 288 guided |
| 5,000 x 20 (seed 20261016) and 2,500 x 8 (20261015) | checked | 3 x 100,000 + 3 x 20,000 | 687; 859; 630 and 184; 203; 144 |
| 18 deck lists, 324 x 200 | both | 6 x 64,800 | 0; 0; 1 on each build |

`DeckValidator` accepted all 67,500 seed decks and all 18 lists.

## 4. Coverage

- 1,000,000 games, any of the three agents: 934 of 955 cards in a deck, 937 in play; every legal card's
  triggered ability triggered (436 of 444), activated ability was activated (131 of 132), spell resolved
  (230 of 233). The missing ones are all on banned cards.
- Never in a deck and never in play, 18 cards: the 12 banned cards (their code never runs here) and six
  token cards whose tokens are made by name without the card (Recruit 954, Sprite 569, Bird 559,
  Reflection 566, Gold 564, Baron Pit 558).
- Rarely exercised: **Daisy! (754)**, whose attack trigger needs four tags among your units: 2, 4 and 3
  resolutions per million. The next lowest card is at 552. Daisy! needs a deck built for it.
- Deck lists: 231 cards, all in play, 110 of 110 triggers, 26 of 26 activations, 71 of 71 spells.

## 5. The orchestrator's five points and the agent

1. **A pass is one command with a seed** and writes a JSON result (totals, groups, per-card coverage).
   `scripts/deck_fuzz_compare.py old.json new.json [--min-resolved K]` lists signatures that disappeared,
   are new or stayed (per 100,000 games) and cards that gained or lost coverage; exit 1 for a new one.
   `scripts/deck_fuzz_report.py` prints a result as Markdown.
2. **Known failures**: `--known docs/testing-known-failures.txt` (`ID|text`, `a && b` for two parts)
   counts them apart; exit status 1 only for a new group. The file now lists F1 to F9; a fresh
   20,000-game pass with it reports 0 new. Its F3 line matches every "off the board" failure whoever
   wrote the state, so it must come out first when fixes land. The stops are flags: `--max-chain` (500),
   `--max-decisions` (20,000), `--max-seconds` (120), `--memory-limit-gb` (4).
3. **Coverage-guided decks**: `--guide prev.json`. At 100,000 games: never-exercised 1 -> 0, cards under
   200 from 20 to 10, 5th percentile of the least-exercised count 522 -> 655; the median falls (2,106 ->
   1,406) and triggers per game fall (22.7 -> 17.2). F1 failures fell 60%, since Viktor is well covered.
   At 1,000,000 unguided games nothing but Daisy! is low, so it pays in short passes. A guided seed is a
   deck only with its guide file. Pairs never seen together: not done (judged not cheap).
4. **`random-player`** is an engine agent (`--agent1 random-player`, in `--help`, `CLAUDE.md`, 8 tests):
   uniform over non-pass actions, **default pass=0.2**, `pass=0` forced play, seeded like random. What
   counts as a pass is decided from Intent fields and listed at `isPassOption`: PassPriority, PassFocus,
   EndTurn, the two Decline types, a mulligan that keeps, and in a prompt the bare value 0 beside options
   that name an object or card, or in a prompt of exactly the values 0 and 1. **Known imprecision**: the
   last rule also matches a two-mode choice, an X of 0 or 1, "cost [1] less / [A] less" and a Hide cost's
   two payments, so forced play never takes option 0 of those.

   Same decks and seeds, 100,000 games each:

   | | random | pass=0.2 | pass=0 |
   |---|---|---|---|
   | Failures not F1 / F2 | 0 | 1 | 33 |
   | Findings seen | F1, F2 | + F7 | + F4, F5, F6, F7, F8 |
   | Decisions per game, mean / p99 / max | 279 / 481 / 834 | 272 / 495 / 1,005 | 299 / 527 / 1,003 |
   | Abilities activated / everything resolved, per game | 8.1 / 36.7 | 8.7 / 37.7 | 11.6 / 41.0 |
   | Games per second per thread | 669 | 666 | 646 |

   - Coverage barely moves; **failures do**: seven of the nine findings needed it or far more random games.
   - Termination: 1,349,600 forced-play games, none at the turn or decision cap. The two things that do
     not end are F4 and F8, recorded as findings.
   - pass=0.2 as P1 against random wins 40.7% to 58.5%: it is a test instrument, not a baseline.
5. **Stopping rule proposed**: two consecutive passes with fresh seeds, each = 3 x 1,000,000 optimised
   (random, pass=0.2, pass=0), 3 x 100,000 checked, the deck lists x 3 agents on both builds, with: no
   failure group against an empty known file; no flagged game; every non-banned, non-token card exercised
   in at least **K = 200** games of each optimised million in every way that applies (K = 50 for the
   pass=0 run, where Dominus sits at 50). One pass costs about 55 minutes at 3 threads (31 + 12 + 8 + the
   suite). Daisy! fails K today and needs a deck list or a named exception. Reason for the size: the
   rarest finding came up once in 4.3 million games and five of nine would have been missed by
   100,000-game passes.

## 6. Throughput

- Optimised: 669 games per second per thread at 100,000 games (558 over the million, with lane B
  building); checked build 141 to 147. 1,000,000 games on 3 threads: 10 minutes.
- Building decks: 50,000 in 0.20 s. Random decks cost the trainer nothing; F1 does.

## 7. Tooling committed

`src/rules/random_deck.{h,cpp}`, `src/agents/random_player_agent.h`, `tools/deck_fuzz.cpp` (target
`deck_fuzz`, not in `all`), `tools/fuzz/fuzz_game.h`, `tests/test_deck_fuzz.cpp` (10 tests, 36 games,
0.45 s), `tests/test_random_player_agent.cpp` (8 tests), `scripts/deck_fuzz_report.py`,
`scripts/deck_fuzz_compare.py`, `docs/testing-known-failures.txt`,
`docs/testing-findings-fuzz-2026-10.md`, and two paragraphs in `CLAUDE.md`.

## 8. Things to decide or check

- `DeckFuzz.DeckOfSeedOneIsPinned` fails when the card pool changes (a new set, a ban): intended, since
  seeds in the findings then mean other decks, but it is one more pinned value.
- The interim report's F numbers were kept for F1 and F2; the rest are ordered by frequency.
- The result files of every run are in the session scratchpad
  (`.../scratchpad/test-fuzz/runs/*.json`), not in the repository. Say if they should be kept.
- Slower large passes (558 against 669 per thread) were not investigated.
