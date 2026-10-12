# test-fuzz — interim report 1 (first 100,000-game pass)

Branch `test/fuzz`, head `cd956e4` (tooling only; no engine or card behaviour changed). Optimised build.
Not yet done: the checked build, the 1,000,000-game pass, the 18 deck lists, the coverage table, the
findings document, the full suite and gates. Numbers below are from one pass and will be replaced by the
final ones.

## 1. The pass

`RIFTBOUND_ROOT=. ./build/deck_fuzz --decks 5000 --games-per-pair 20 --seed 20261011 --threads 3`

| | |
|---|---|
| Games | 100,000, all played to an end or a recorded failure |
| Decks | 5,000 by seed, 5,000 distinct, 0 rejected by `DeckValidator`, all 49 legends used |
| Cards in at least one deck | 934 of 955 (the 21 others: 12 banned, 9 token cards) |
| Wall time, 3 threads, coverage listener on | 49.9 s: 2,006 games/s, **669 games/s per thread** |
| Building the 5,000 decks | 0.02 s |
| Failures | **707** in 2 causes (below) |
| Turn cap / no winner / no reason | 0 / 0 / 0 |
| Decisions per game that ended | mean 279, median 267, p99 481, p99.9 587, max 834 (36 turns); nothing odd in the longest 100 |
| Ends | 99,225 by points, 36 The Grand Plaza, 25 Gutter Palace, 7 burn-out point |

## 2. Findings so far (not fixed, by instruction)

### F1 — Viktor, Innovator (117) triggers on its own Recruit token: the chain never empties — 686 games (0.69%)

- **What happens:** on an opponent's turn the controller plays any card; Viktor's ability plays a Recruit
  token; that token play triggers Viktor again; and so on.
- **Count:** 686 of 100,000. 673 stops name Viktor as the latest chain item. The other 13 name a bystander
  that triggers on each Recruit (Rally the Troops 4, the keyword Vision 4, Lillia, Protector of Dreams 3,
  Vex, Apathetic 2); I traced one game of each and every one is the Viktor loop (about 250 Viktor triggers
  before the stop).
- **Without the fuzz driver's stop the engine does not fail.** `ChainManager::processFEPR` gives up after
  1,000 passes, drops the chain with a warning line and plays on with about 1,000 Recruits on the board.
  From there I saw, in three separate games: a game of 8,285 decisions that then ran out of memory (21 GB
  resident when I killed it; the machine has 23); a game that sat inside one decision for over 300 s
  (Divine Judgment with a thousand units); 3,993 "move another unit with this move" prompts in a row.
  **This matters to the trainer: random-deck self-play hits it in about 1 game in 150.**
- **Reproduction** (89 decisions, stopped after 12 chain items instead of the default 500):
  `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 8392194388957710676 6231710079672200936 412255519 --max-chain 12 --trace --tail 30`
- **Trace, last lines:**
  ```
  [TRC] RESOLVE: Viktor, Innovator (chain_item=21, ability=1, spell=0)
  [TRC] TOKEN: created Recruit (1M, id=110) for P2
  [DBG] TRIGGER: Viktor, Innovator (P2) fires ability
  [DBG] TRIGGER: Viktor, Innovator (P2) fires ability        (two Viktors in play)
  [TRC] VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base
  [TRC] DECISION #88 (P2 — chain priority): 1 legal actions
  ...
  ```
- **Reading (mine, not verified):** the card registers for `WhenYouPlayAUnit` (with spell and gear) to stand
  for "when you play a card"; since tokens are played (CR 350.2, `TokenCreatedEvent`), its own Recruit
  fires it. A token is not a card, so the text should not fire on it. Separately, the 1,000-pass bound that
  drops a chain silently hides the loop instead of failing the game.

### F2 — Alpha Strike (751): finalize pass ends before its marker when its choices are made without a play-time target — 21 games

- **Exception:** `GameEngine::runFinalizePass: Alpha Strike returned from its finalize pass before its
  end-of-choices marker and not because a choice helper had nothing to decide (the rule is at
  Card::finalizingChoices)`
- **Routes seen** in the 10 shortest: Mystic Reversal taking control of an Alpha Strike on the chain (8),
  Blind Fury playing a revealed Alpha Strike (1), Promising Future playing it (1).
- **Reproduction** (34 decisions):
  `RIFTBOUND_ROOT=. ./build/deck_fuzz --replay 15524873237644084445 9693623397702818148 464654890 --trace --tail 30`
- **Trace, last lines:**
  ```
  [TRC] RESOLVE: Blind Fury (chain_item=4, ability=0, spell=1)
  [TRC] BLIND FURY: opponent reveals Alpha Strike
  [TRC] EFFECT: banish Alpha Strike (id=4)
  [TRC] FINALIZE-CHOICES: Alpha Strike (chain_item=5, ability=0, spell=1)
  ```
- **Reading (mine, not verified):** `AlphaStrike::onResolve` opens with
  `if (targets.empty() || !ctx.state.objectExists(targets[0])) return;`. The friendly unit is a play-time
  target handed in by the Play intent; a play by an effect, or new choices after a control change, has
  none, so the pass leaves at that line, which the finalize contract forbids.

### V1 — `DeckValidator` does not enforce [Unique] (CR 825.3.a)

- Forgefire Cape (507), Rabadon's Deathcrown (508) and Shurelya's Requiem (509) print "[Unique] (Your deck
  can have only 1 card with this name.)" and carry `Keyword::Unique`. The validator accepts three copies:
  `checkCopyLimits` knows `unlimited_copies` only.
- The random deck builder keeps to the rule (one copy), so the fuzz decks are legal by both.
- Nothing else found between the validator and `rules/tournament-rules.md` 402 / 601 so far (exactly 40
  with the champion, 12 runes, 3 battlefields of different names, the 3-copy and Signature limits, bans).

## 3. What the tool had to survive

Nothing stopped the fuzz from running, so nothing was fixed. The driver gained three stops because of F1:
a chain with 500 items finalized without emptying (`chain_runaway`, deterministic), a per-process memory
limit (a runaway game ends as `std::bad_alloc` and is replayed alone before it is reported), and a
per-game time limit checked at decisions and at every 256th `StateSettledEvent` (`time_cap`).

## 4. Tooling committed (`cd956e4`)

- `src/rules/random_deck.{h,cpp}`: `RandomDeckBuilder`, seed -> legal deck (in `riftbound_core`, so the
  trainer can use it; a new file, nothing existing changed).
- `tools/deck_fuzz.cpp` (target `deck_fuzz`, not in `all`) and `tools/fuzz/fuzz_game.h`.
- `tests/test_deck_fuzz.cpp`: 10 tests, 0.45 s, 36 fuzz games.
- `CLAUDE.md`, Testing: how to run the large one.
