# Engine review fix run — final report

2026-10-10 to 2026-10-11. Baseline `c7143f1` (master at stage 0), final integration commit `079da50` plus the
closing documentation commits. 656 commits, 1,104 files changed (+86,549 / −18,868).

## Result

Every item is closed: **760 closed, 0 open** (`plans/remaining/` holds no item; `plans/completed/` has each
item with its outcome).

- 514 findings of the 2026-10 review (plans 01–08 and the card batches).
- 246 more found while fixing (plan 09), each closed the same way.
- 40 of the 760 are closed as NOT A DEFECT or NOT REPRODUCED, each with evidence, a pinning test where one
  is possible and a note at the code site. 1 was moved out of the run by the user (play-UI default, 9.QQ2).
  The rest are fixed, each with a test shown red first.

## Gates

| | Tests (`riftbound_tests`) | OpenSpiel tests | Smoke, 200 games fiora vs yi_rebuild, seed 20261010 |
|---|---|---|---|
| Stage 0 baseline, `c7143f1` | 1511 run: 1508 passed, 2 failed, 1 skipped | — | — |
| Stage 2 gate, `6a3e52b` | 2040 passed | 17 | rc 0 |
| Stage 3 gate, `f6345e2` | 2998 passed | 25 | P1 139 / P2 61, 294 decisions |
| Stage 4 gate, `b0859a9` | 3018 passed | 25 | 139 / 61, 294 |
| Final gate, `079da50` | **3065 passed, 0 failed** | **27** | **P1 138 / P2 62 / 0 draws, 295 decisions** |

Final gate: default release configuration, reconfigure and clean rebuild, 0 warnings; clone-equivalence
10/10; MCTS batch rc 0; injected engine fault ends the batch rc 1 (a failure, not a draw); `ismcts:sims=50`
and `ismcts:sims=20` ten runs each, 20 of 20 clean; `collision_census` builds. The smoke moved from
139 / 61 to 138 / 62 with the last fix (9.VV1: a rune recycled for Deflect now gives its Energy first).

## What changed, by stage

1. **Engine foundation and CLI**: an engine exception ends a game as a failure, never a draw; agent answers
   are validated in one place; decision ids and seats in the play protocol; build integrity (PCH-safe
   ccache launcher, unity tests, job cap).
2. **Cards**: all 955 card files read against their text; choices made where the rules finalize them;
   triggers never handed targets.
3. **Sweeps, cleanup, performance**: the engine no longer reads card text; card-declared Equip / Repeat /
   additional costs; death batches and replacement order; reflexive triggers as their own chain items;
   leaving the board makes a new object at the move with a snapshot of the card as it left; ISMCTS made
   reproducible from its seeds; object table with a board index. Random play 3.0 s → 1.2 s per 1,000 games;
   `mcts:sims=50` 5.2 s → 2.7 s (5 games).
4. **Strong id types**: every id kind is its own type, one "none" value; about 1,300 compile-error sites,
   no real mix-up found. A `GameState` copy 14.9 µs → 6.2 µs.
5. **Checkpoint-breaking**: see below.

## Checkpoint-breaking changes (stage 5)

Full detail in `docs/checkpoint-breaking-2026-10.md`. Every stored `InfoSetId`, action id, history token
and anything trained on the old flat layout is invalid.

- **State vector**: one ordering convention, relative to the observing player (it mixed absolute and
  relative order; from Player 2's seat one slot meant different players in different blocks).
  `kStateFeatureDim` 6188 → **10944**; use `ml::layout::*` for offsets.
- **Card id space**: `kCardVocabSize` 1100 → **2048** (user decision). 1..955 registered through Vendetta,
  956..1165 reserved for Radiance (210), 1166..1565 for Legacy (about 400), 1566..2048 spare.
- **Action ids**: no two distinct legal intents share an id. Before, 39% of decisions had a collision and
  44% of distinct legal intents could not be chosen through OpenSpiel (every mulligan; 81% of object
  prompts). `id = slot + kNumSlots × twin`; `kNumSlots` 127,124; `NumDistinctActions` 130,174,976 — do not
  size anything by it.
- **`Intent`**: the setup battlefield choice has its own field (`battlefield_pool_index`).
- **Knowledge tracker**: hears every reveal and every move into a hidden zone from one place each.

## Rulings and decisions recorded

- Judges, 2026-10-11 (`docs/judge-questions-2026-10.md`): a bare "ready up to N runes" may ready an
  opponent's runes; Void Hatchling applies to the Rune Deck; hidden Guards! where units can't be played
  plays no soldier; Disposal Order may choose zero; Emperor's Divide is playable from facedown choosing
  zero; Curator may exhaust to ready zero.
- User: an unplayed hidden card goes to the trash when its controller loses the battlefield (CR 323.6.a);
  a revealed, unused Hourglass is recalled to base. Play-UI work is out of the run. No event-bus rework and
  no moving header bodies now. Tests are compiled at `-O0` in development builds
  (`-DRIFTBOUND_TEST_OPT_LEVEL=0`).

## Not done, and where it is written

- **Play UI** (never exercised in a browser; two-human default, seat release): `docs/play-ui-followups.md`.
- **Performance recommendations** (a bus is built per cloned state, about 12% of an MCTS batch; header
  bodies; card optimisation level): `docs/perf-followups.md`.
- **Rune Deck knowledge** is tracked nowhere; a feature for the v6 layout: `docs/trainer-notes-2026-10.md`.
- **Questions still with the user**: reprint region tags (per-printing vs union) and Karma, Channeler 235's
  tags. The engine uses the per-printing gallery data.
- **Not verified**: nothing in a browser; no thread sanitizer run. Commit `0999945` in the history has
  conflict markers in a plan file: do not bisect there.

## What the run did not prove

Real-game coverage was two decks until the last hour. Three more deck lists found an engine error within
40 games per pairing (9.VV1). The **testing phase** that follows — the 18 deck lists in `decks/`, a
whole-pool fuzz with randomly built legal decks, per-card coverage, fixes batched per rebuild — is planned
in `docs/testing-phase-2026-10.md`, with two leads already known.

## What the main checkout needs

- `build/` in the main checkout was never rebuilt during the run: reconfigure and rebuild it. Recommended:
  `-DCMAKE_BUILD_TYPE=Release -DRIFTBOUND_TEST_OPT_LEVEL=0` with the launcher line from `CLAUDE.md`
  (`scripts/ccache_pch_safe.sh`). ccache's size limit was raised to 40 GB.
- The 16 deck lists added on 2026-10-11 are untracked in `decks/`.
- The trainer repository must follow `docs/checkpoint-breaking-2026-10.md` and
  `docs/trainer-notes-2026-10.md` before it builds against this engine.
