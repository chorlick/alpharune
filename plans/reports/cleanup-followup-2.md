# Follow-up 2

Stream cleanup, worktree `/home/pirateporkchop/code/riftbound-engine/.worktrees/cleanup`, branch `fix/cleanup`, head `ab898bc`, working tree clean. `fix/integration` is merged in (last merge `81815e4`).

All eleven items assigned to this follow-up are closed and ticked in `plans/09-found-during-fix-run.md`: 9.F1, 9.D8, 9.E3, 9.F5, 9.H5, 9.G11, 9.L3, 9.M39, 9.M40, 9.M41, 9.P1. 9.G11 closes as not a defect as stated, and 9.M41 closes on a corrected predicate; both are explained below.

## Verification

Lane A lock, `-j3`, launcher `scripts/ccache_pch_safe.sh`, after the last merge:

- Full build: exit 0.
- `riftbound_tests`: 2592 passed, 0 failed, 1 disabled.
- New server, batch and fixture tests repeated 10 times: 10 of 10 green.
- `ctest -R 'riftbound_one_card_table|^riftbound_openspiel_tests$'`: 2 of 2 passed.
- The two commits after that run change only plan text and comments. The hand-built-intent answer check you warned about broke none of my converted tests.

## Commits not yet on integration

| Commit | Content |
| --- | --- |
| `35c663e` | tests: post-intent cleanup in every engine-path helper (9.F1); Predict default is keep (9.D8) |
| `af4442f` | main: one card table per process (9.E3); ctest `riftbound_one_card_table` |
| `26c757b` | build: launcher writes a checksum of the PCH's inputs; cause confirmed (9.F5) |
| `7f39acd` | tests: batch fault injection (9.L3) |
| `ccfc341` | docs/tests: 9.M39, 9.M40, first pass of 9.M41 |
| `7bfc665` | tests: strict `choosing()` (9.H5); fixture contract tests and notes (9.G11) |
| `a5b4df1` | io: showdown and priority lists rebuilt after a god-mode edit (9.P1) |
| `3591a2e` | tests/docs: 9.M41 final form; ISMCTS text after 9.E2; merged Jayce test |
| `f6363a2`, `ab898bc` | plans: boxes ticked; rebuild timing in the launcher notes |

## 9.F5: the integration build failure, and the cost of the fix

**Cause: reproduced and confirmed, with one correction to the hypothesis.**

- Setup: a scratch copy of the repository, its own cache, the launcher exactly as at `a8723a4` (delete stale `.gch`, then ccache), and separately plain `ccache`.
- Sequence that fails: build; reword a comment in `core/types.h` **in place** (same line count, as `e9f8b79` did); `cmake --build --target clean`; build.
- What happens: the PCH compile is a cache hit and the previous `.gch` comes back. The first unit that is really compiled against it fails with `types.h:25: error: redefinition of 'constexpr const uint32_t riftbound::kInvalidId'`.
- The correction: a comment-only edit that adds a line does not trigger it (the PCH compile misses). That is why my earlier experiment did not reproduce it.
- Units that are themselves cache hits hide the failure. In my run `card_db.cpp.o` was a hit and "built"; `types.cpp.o`, never built before, failed.
- With the `f3670f3` launcher and with the new one, the same sequence builds.

**Cost: fixed rather than accepted.** `pch_external_checksum` is supported by this ccache (4.9.1) and is now used.

- For a PCH compile the launcher still never uses the cache. After a successful compile it writes `<pch>.gch.sum`: a SHA-256 of the compiler's `--version`, the full command line, and the name and raw bytes of every file in the compile's depfile.
- Raw bytes, not preprocessed text, so a comment-only edit changes the key (line numbers reach object code through `__LINE__`, `assert` and debug info).
- If the depfile is missing no `.sum` is written and ccache falls back to hashing the `.gch`: slow, not wrong.
- Paths are part of the checksum, so checkouts at different paths do not share PCH-using objects.

| No-change clean rebuild of `riftbound_tests`, `-j3` | Time | Cache hits |
| --- | --- | --- |
| `f3670f3` launcher (PCH never cached, units keyed on `.gch` bytes) | 222 s | 2 of 108 |
| New launcher (`.gch.sum`) | 14 s | 109 of 109 |

**Correctness evidence for the new launcher** (scratch experiments, no automated test):

- Two copies sharing one cache, `core/types.h` changed differently in each: each `.gch` contains only its own change.
- One tree, same unit each time:

| Step | Expected | Observed | `.sum` |
| --- | --- | --- | --- |
| First build | miss | miss | `89b45f5e` |
| Clean, no change | hit | hit | `89b45f5e` |
| Real header change | miss | miss | `634a50f9` |
| Comment reworded in place, clean | miss | miss | `57e90b26` |
| A unit never built, against that PCH | builds | builds | `57e90b26` |
| Original bytes restored, clean | hit | hit | `89b45f5e` |

The script comment, the CMake configure warning and CLAUDE.md "ccache" now state the confirmed cause, the mechanism and the measured cost. Worktrees still configured with another launcher need a reconfigure to get this.

## 9.M41: what the 54 decisions are

Post-merge the literal predicate (Closed state, a chain, the turn player asked, the other player holding priority) is met 52 times in the six games (7, 19, 1, 5, 15, 5 for seeds 1 to 6). I printed every one.

- **Decision kind:** all 52 offer only `MakeChoice` options. None offers End Turn or any other game action.
  - 34 are labelled "may" prompts: Fiora, Worthy; Grand Duelist; Emperor's Dais; Sunken Temple.
  - 18 are unlabelled choices (a target or a payment pick; I did not identify which for each).
- **Who is asked:** the turn player in all 52 (that is the predicate), and every option names that player.
- **What is on the chain:** 1 to 3 items. In 49 an item is being resolved (`chain.resuming` set). In 3 it is not: one finalized ability of the asked player, unlabelled options (seed 1 decision 18, seed 4 decision 207, seed 5 decision 212).
- **Whose item:** the top chain item belongs to the asked player in 51. In 1 (seed 2 decision 63) the asked player's Grand Duelist trigger is resolving above the opponent's spell during a showdown.
- **Verdict:** none is the defect sweep-flow fixed. `priority_holder` is simply not updated while an item is finalized or resolves, so the predicate over-matches.

The test now asserts three things and the 5% tolerance is deleted:
1. Every option names the asked player, at every decision (previously skipped in the tolerated state).
2. No decision that offers End Turn has an item on the chain: count is 0.
3. In the literal condition every option is `MakeChoice`; a game action there fails with seed and decision number.

## 9.G11: not a defect as stated

The claim was that the engine marks every unit with `card_def_id == kInvalidId` as a token. It does not: there is no such code in the constructor or anywhere else. Such a unit has `isToken() == false`, and killed it goes to its owner's trash and is listed there.

What is true, and is now documented at `addUnit`:
- The engine skips an id-less unit wherever it needs a CardDef (no cost, no abilities, no text).
- Hand-set battlefield control survives a cleanup only while that player has a unit there.

I did not change the fixture to build real cards: the premise was wrong and there are 1319 call sites. Pinning tests: `FixtureContractTest.AUnitWithoutACardIdIsNotAToken`, `.HandSetControlNeedsAUnitToSurviveACleanup` (new file `tests/cards/test_fixture_contract.cpp`).

## The other items

**9.F1 (post-intent cleanup everywhere)**
- `playThroughEngine`, `fireTriggerThroughEngine` and `resolveThroughEngine` all end with `settleLikeTheEngine` (chain, cleanup, chain).
- Of the 61 direct `testHook_executeIntent` calls, 54 now call it too. Seven stay direct:
  - six `AssignCombatDamage` intents in `tests/test_rules_compliance.cpp`: mid-combat, where the engine runs no main-phase cleanup;
  - `ChainTriggerFixTest::play`: its tests look at the state before `drainChain`.
- Tests corrected at setup or assertion: C6 ElderDragon (the damaged unit dies under "any damage is lethal"), C0 SolariShrine (each unit marked just before its kill), C2 Shakedown (asserts the unit is dead).
- The remaining `damage_marked` assertions were not read one by one. Any that asserted lethal damage after an engine-path helper would now fail, and none does.

**9.D8 (Predict default)**
- With no picker, `EnginePathAgent` answers a Predict "recycle or keep" prompt with keep; `EnginePathAgent::first()` is the explicit recycle.
- Three tests relied on the silent recycle and now say so: C4 GemcraftSeer, C1 ScryersBloom, SweepTriggers Vision.
- The `accepting()` picker still takes the first option of a Predict prompt, which is recycle. I left that as is.

**9.H5 (strict `choosing`)**
- `choosing(id)` fails the test at a "may" it was not told about; `choosing(id, May::Yes)` or `May::No` announces one.
- A mode, an X or an unlabelled yes/no also fails ("use `script()`"). A one-option prompt is taken. An object prompt without `id` takes the first, because negative tests assert on `wasOffered`.
- 73 tests met a "may" naming the card under test and now pass `May::Yes`. The conversion is per test, so every `choosing` call in those tests got it, not only the one that meets the prompt.
- Three tests now state a mode or cost answer explicitly: two Blast Cone tests and SweepCosts Ezreal.
- No test was found accepting an unrelated "may".
- This touched test files owned by other streams (`test_review_fix_sweep_triggers.cpp`, `test_sweep_costs.cpp`, `test_review_fix_verify_cards.cpp`, `test_review_fix_verify_engine.cpp`), picker arguments only.

**9.E3 (one card table)**
- `src/main.cpp` uses `openspiel::sharedCards()` for the game and for `--dump-registry`.
- `CardDB::tablesBuilt()` counts builds; `--debug` prints "Card tables built in this process: N".
- ctest `riftbound_one_card_table` runs an mcts vs ismcts game and expects 1. I did not run it against the old `main.cpp` to see it report 2.

**9.L3 (batch after a throw): confirmed and pinned, no behaviour change**
- One thread: earlier games finish and are counted, the failing game is named `game N [seed=S]`, later games are not started, `runBatch` throws.
- Several threads: games already running finish and count; unstarted ones are skipped.
- The binary then prints the error and "k of n games completed" and exits 1, with no results table for the completed games. If you want that table, it is a `main.cpp` change I have not made.
- Tests: `BatchRunnerFaultInjection.OneThread_EarlierGamesCountLaterGamesAreSkipped`, `.ManyThreads_TheFailedGameIsNamedAndNeverCounted`.

**9.P1 (edit at a showdown or priority decision)**
- `recomputeLegalForParkedHuman` now asks `GameEngine::recomputePendingLegalActions()`, which covers main phase, showdown and chain priority, and hands the human the rebuilt list. `docs/play-api.md` is updated.
- Test: `PlayServerGodMode.EditAtAShowdownRecomputesAndAStaleClickDoesNotEndTheGame` (live server): at a showdown a playable card is edited out of hand; the list is rebuilt, the old id is refused as stale, and Pass Focus from the new list is accepted and the game goes on.
- There is no live-server test at a chain priority decision; it goes through the same call.

**9.M39** The `extractStateFeatures` comment names `kStateFeatureDim` = 6188. The "25-dim" action comment was correct and now names `kActionFeatureDim`.

**9.M40** `main.cpp` header and `--help` are rewritten from `agents/mcts_agent.cpp` as merged, after 9.E2 landed: heuristic evaluator not rollouts; a mirror that replays only the choices since the last decision; ISMCTS is our `IsMctsSearch`; hidden cards are not sampled; what a god-mode edit does to the mirror.
