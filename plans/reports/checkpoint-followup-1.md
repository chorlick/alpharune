# checkpoint — follow-up 1 (9.TT1)

Branch `fix/checkpoint`, head `c1797b5`. **Verified against integration `c560196`**, merged in before the work (merge commit `79cfdb5`, made with `-m`, trailers present).

## 1. Result

9.TT1 is closed, all three parts. Every gate passes on a clean rebuild of `c1797b5`.

Clean rebuild (`cmake --build build --target clean`, eight targets, `-j6`, both lane locks, tests at `-O0`): **0 errors, 0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **3060 passed, 0 failed** (3059 + 1 new) |
| `riftbound_openspiel_tests` | 27 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | byte-identical to `2cb28f8` |
| observation checksum | `965b13284eb6dc5c` (unchanged by this item) |
| `mcts:sims=50` 5 games seed 20261010; `ismcts:sims=20` 8 games seed 1 | same games as at `d150457` (5 / 0 and 4 / 4); each run twice, identical |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | 10/10 clean, ten identical outputs |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | 10/10 clean, ten identical outputs |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build) | 3060 passed; OpenSpiel 27; clone-equivalence 10/10; smoke split the same; 0 warnings |
| `ctest -R riftbound_strong_id` | 26/26 |
| `collision_census`, 100 games | 0 collisions, 0 intents without an id |

One thing to know about the trace row: the gate script's own trace step reported 8 differing files. That was my script writing into the output directory of the previous final run, so each HTML file held two replays. A capture into a fresh directory gives 0 differing files and the same 14,708,563 bytes; that is the number in the table.

## 2. Item closed

**9.TT1 — FIXED**, commit `c1797b5`. Ticked and moved to `plans/completed/`; `plans/remaining/` holds 9.O2 only.

### (a) Hand reveals are announced to all players

- **CR 425.1**: "Revealing is the act of presenting a card to all players". No reason for caster-only was found, so the event now says what the card says.
- Changed to `revealed_to_all`: Sabotage 156, Mindsplitter 192, Decree of Strength 872, Insightful Investigator 697, Ashe 731.
- **A sixth card had the same fault**: Scuttle Crab 615's Deathknell ("they reveal their hand, and I may look at their facedown cards"). The hand half is now public; the look at facedown cards stays the looker's alone.
- **Trace lines**: all six were already public `RIFT_TRACE` lines, which is right for a public reveal. Nothing changed, so traces are byte-identical.
- **One side effect handled**: a public reveal would have added a player's own hand to their `observed_cards` count. `GameState::recordReveal` now skips the owner of a revealed hand, so the count is what it was.
- Tests:
  - `KnowledgeReveals.AHandRevealIsAnnouncedToAllPlayers`: the six cards through the engine; every hand card revealed, each to all players, no facedown look public. Red before.
  - `CardTestFixture.Sabotage_RevealsEveryCardInOpponentsHand` pinned caster-only (`revealed_to == P1 && !revealed_to_all`). Corrected to all players with the CR citation. Its sibling `Sabotage_RevealedCardsLandInCasterMemoryBank` still passes unchanged in what it asserts (only its comment was reworded).

### (b) The two tests now show the gap

Both read the tracker from the agent callback while the card's prompt is open, and both are **red on the sources before 9.SS1** (I reverted `src/` to `7e70b67` under the new test file, ran, and restored from a patch).

| Test (renamed) | Where it looks | Before 9.SS1 |
|---|---|---|
| `AnOpponentsRevealedTopCardIsKnownToBothWhileItIsOnTop` (Blind Fury) | at the "play it to" prompt, with the revealed card still on top of the opponent's deck | failed: neither player's record had the card |
| `ALookAtTheTopCardsIsKnownWhileTheOfferMadeFromThemIsOpen` (Fate Weaver) | at the "reveal a spell and draw it?" prompt | failed: 0 of the 4 looked-at cards known |

On those old sources 8 of the 12 `KnowledgeReveals` tests fail.

### (c) Literals that mean the id space

Replaced:

| Where | Was | Now |
|---|---|---|
| `tests/test_review_fix_sweep_triggers.cpp`, two loops | `n <= 1100` | `ml::kCardVocabSize` |
| `tests/test_review_fix_verify_engine.cpp`, one loop | `n <= 1100` | `ml::kCardVocabSize` |
| `src/cards/card_registry.h`, comment | "the vocabulary is 1100" | names the constant |

Found and left, with the reason:

| Literal | Where | Why it stays |
|---|---|---|
| 955 | three card files, `effect_executor.h` comment, four test files | Shadow Clone's own card id, not a range |
| 1100 | `tests/test_review_fix_chain_triggers.cpp` (`kStubShowdownBattlefield`) | one test card's id inside the reserved range |
| 1150, 1151 | `feature_extractor.h`, `action_vocab.h`, `token_layout.h` | the definitions themselves |
| 955 / 956 / 1100 / 1150 | comments next to those definitions | they explain the ranges |
| 1151 | `ActionVocab.SizesArePinned` | pinned on purpose |

There is no constant for "last registered id" and no code needed one: nothing in `src/` or `tests/` uses 955 or 956 as a bound.

## 3. New findings

No new findings.

Play UI, deferred: none noticed.

## 4. How the work was run

- Both lane locks for every build, suite run and gate run.
- The merge of `fix/integration` was made with `git merge --no-ff -m ...` and carries both trailers. The earlier merge `7e70b67` still has none (it is not the head any more, so I did not rewrite it).
- No `rm`.
