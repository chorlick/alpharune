**Trainer survey, 2026-10-11.** Nothing was built, run or modified. The trainer is paused at the end of v6 Phase 0 / early Phase 1, with no v6 model code yet, and it will most likely fail to configure against current engine `master`.

Three things to know before the detail:
- The memory directory `/home/pirateporkchop/.claude/projects/-home-pirateporkchop-code-riftbound-trainer/memory/` is empty: no `MEMORY.md`, no notes.
- There is no `CLAUDE.md` and no top-level README in the trainer.
- The only statement of "what the trainer thinks" is `/home/pirateporkchop/code/riftbound-trainer/plans/tooling-v6/README.md` (last edited 2026-10-10 17:10) plus the git log.

## 1. What the trainer is

- **Repo:** git, branch `master`, 11 commits, all dated 2026-10-10 (initial commit 11:18, last `b6c0704` at 17:10). Four result files are untracked.
- **Two layers:**
  - **Legacy (May 2026, not built):** `src/ml`, `src/training`, `src/runner` hold the libtorch V2–V5 models, Deep CFR, OSFP/PPO and ReBeL inference. `CMakeLists.txt:117-119` excludes them. There is no Python.
  - **v6 tooling (built, torch-free):** `src/config/*`, `src/match/*` (the `riftbound_match` binary with agents `random`, `scripted`, `mcts:sims=N`), `tests/*` (`trainer_tests`), and `scripts/v6_baselines.sh`.
- **Build:** `cmake -B build -G Ninja && cmake --build build --target riftbound_match trainer_tests collision_census`.

**Coupling to the engine** (all in `/home/pirateporkchop/code/riftbound-trainer/CMakeLists.txt` unless noted):
- **Subdirectory build:** line 23 does `add_subdirectory(${RIFTBOUND_ENGINE_DIR} engine EXCLUDE_FROM_ALL)`, defaulting to `../riftbound-engine` (lines 16-17). It therefore builds whatever is checked out there.
- **Engine sources compiled directly into trainer targets:**
  - lines 55-57: `src/openspiel/riftbound_game.cpp`, `riftbound_state.cpp`, `src/agents/mcts_agent.cpp`
  - line 94: `src/agents/scripted_agent.cpp`
  - lines 112-113: `tools/collision_census.cpp`
- **Linked targets:** `riftbound_core`, `open_spiel_core`, the OpenSpiel object libraries `algorithms`, `game_transforms`, `utils`, abseil, `GTest::gtest_main`, and `nlohmann_json` from the engine's scope.
- **Engine commit stamp:** baked in at configure time (lines 27-45, 98-99) and written to every result JSON.
- **Headers used by v6 code:** `agents/{agent_interface,random_agent,scripted_agent,mcts_agent}.h`, `cards/card_registry.h`, `core/{card_db,events}.h`, `engine/{game_engine,knowledge_tracker}.h`, `rules/deck_validator.h`.
- **Hard-coded values in v6 code:**
  - `src/match/match_runner.h:49`: `kSearchMoveCap = 600`, a copy of the engine's `kMaxGameLength`.
  - `src/match/agent_factory.cpp:55`: passes `"cards/registry.json"` to `MctsAgent` (the engine now ignores it).
  - `src/match/match_runner.cpp:38-41`: substring matching on `termination_reason` ("Max turns", "burn-out", "conced", "reached").
  - `tests/test_knowledge_gate.cpp:123-128,150`: an allow-list of engine files that may edit `main_deck`, and a requirement of more than 900 engine source files.
  - `tests/test_knowledge_gate.cpp:79-82`: reads `state.knowledge.facts` and `DeckEnd` directly.
- **Hard-coded values in legacy code:**
  - `src/ml/v5_model.h:66`: `num_action_slots = 9615`.
  - `src/ml/v2_model.h:52`: `num_card_defs = 1100 + 1`.
  - `src/training/config_driver.cpp:110,123,136`: `kStateFeatureDim`.
  - `src/training/config_driver.cpp:735-741`: paths to the deleted `riftbound_openspiel` binary.
- **Decks:** four lists copied into `decks/*_test.txt`. Engine `master` no longer has them (`decks/` there holds only `fiora.txt` and `yi_rebuild.txt`).

## 2. State according to the trainer's own plan

- **Current generation:** v6 (`model.kind = "v6"`), a rebuild around imperfect information. The last trained generations are V4 (Deep CFR) and V5 (OSFP), both archived.
- **Why the rebuild** (plan lines 13-33):
  - No network was ever shown to beat random without MCTS attached.
  - V4's value and strategy heads were never trained.
  - 39–47% of legal intents are unreachable through colliding action slots.
  - Search is clairvoyant.
  - The old tooling does not build.
- **v6 design:** engine-side knowledge tracker, legal-action-list scoring instead of a fixed vocabulary, token observation (global 78, battlefield 25, entity and own-deck 90 floats, card table 1101 rows; `interfaces.md`), transformer, supervised sanity gates, then PPO self-play, then determinized search.
- **Done:**
  - P0.0–P0.6: interface headers, build, config, match runner.
  - P5.1: scripted agent.
  - P1.1: reveal audit (`plans/tooling-v6/p1.1-reveal-audit.md`).
  - P2.1: collision census, on `a2776e5` only.
  - P1.2: tracker data model.
  - P1.3 part 1: every Main Deck edit goes through `deck_ops`.
  - P1.4: gate test for deck positions (208 games, 57,689 decisions, no disagreement).
- **In progress / stated next:** "Trainer work is paused" while engine review findings are fixed; "it resumes with the rest of Phase 1" (plan lines 54-56). That means P1.3 part 2:
  - hooks for public cards returning to hand, hand reveals and Hide;
  - cards 505, 594 and 760;
  - correcting 19 wrong `CardRevealedEvent` sites;
  - removing `PlayerState::observed_cards`.
- **Not started:** Phases 2–7. No `v6_model.*`, no `src/data/*`, no `policy:` agent.
- **Open questions for the owner** (plan lines 550-559): whether to assume the opponent's deck obeys construction rules; which decks are in scope; whether to delete or archive the legacy code; local versus rented GPU.
- **Open interface points** (`interfaces.md`, end): a `prompt` field on `StepResult`; trash as tokens; runes as tokens.

## 3. Runs

**Checkpoints** (`checkpoints/legacy/`, gitignored, all May 2026, old 4623 / 9615 shapes, unloadable):

| Directory | Size | Files |
|---|---|---|
| `gpu_cfr_resume2000` | 6.0G | 688 |
| `local_v5_validate` | 246M | 46 |
| `local_cfr_validate` | 212M | 22 |
| `gpu_cfr_miss_fortune` | 169M | 14 |
| `smoke_v5_miss_fortune` | 36M | 14 |
| `gpu_v5_miss_fortune` | 17M | 2 |

The legacy headline claims in the docs are V4 at 87% versus random at small scale and V5 + MCTS-20 at 62%, both with search attached. No v6 training has happened.

**Baselines** (`logs/baselines/<commit>/`, 400 games, seed 1, seat-swapped, all games ended on victory score):

| Directory | Contents | Complete? |
|---|---|---|
| `a2776e5` | random ×10, scripted ×16, MCTS-5 ×4, `census/` ×4 | yes |
| `0307347` | random ×10, scripted ×16 (same engine code, adds game-length fields) | yes |
| `b05e51d` | random ×10, scripted ×16, MCTS-5 ×4, MCTS-20 ×4, MCTS-50 ×1 | MCTS-50 for Jhin, Rengar, Kha'Zix never ran |
| `895abfe` | random ×10, scripted ×16, MCTS-5 ×4, MCTS-20 ×3 | MCTS-20 Kha'Zix and all MCTS-50 missing |

**Headline numbers:**
- **Random vs random on `b05e51d`:**

| First deck | vs Jhin | vs Rengar | vs Kha'Zix |
|---|---|---|---|
| Miss Fortune | 84.0% | 42.8% | 63.5% |
| Jhin | | 8.2% | 23.8% |
| Rengar | | | 74.2% |

- **Scripted vs random, mirrors:** 74.5–87.0%.
- **MCTS vs random, mirrors, by sims:**

| Deck | MCTS-5 | MCTS-20 | MCTS-50 |
|---|---|---|---|
| Miss Fortune | 74.2% | 69.5% | 72.2% (4,428 s) |
| Jhin | 59.2% | 54.5% (54.0 on `895abfe`) | not run |
| Rengar | 66.0% | 50.2% | not run |
| Kha'Zix | 69.8% | 67.8% | not run |

- More simulations did not help and often hurt. That fits the plan's statement that these MCTS numbers "do not measure MCTS" (the perspective bugs, items 1.1 and 1.2 in the engine's `plans/01-openspiel-search.md`, are now marked fixed on engine master).
- **Collision census** (`a2776e5`): 61–71% of real-choice decisions have a collision.

**Interrupted:** the MCTS sweep was stopped during MCTS-50. No `.tmp` files remain and no `riftbound_match` process is running. The last file was written at 18:13 on 2026-10-10; the four untracked files are the uncommitted tail of that sweep.

## 4. Where the engine changes break or stale the trainer

1. **Configure will probably fail against engine master.** Engine `src/openspiel/CMakeLists.txt:110` now lists `${CMAKE_SOURCE_DIR}/src/agents/mcts_agent.cpp` for the new `riftbound_openspiel_tests` target. Under the trainer's `add_subdirectory` (`CMakeLists.txt:23`), `CMAKE_SOURCE_DIR` is the trainer root, which has no `src/agents/`. Lines 125 and 127 have the same problem for the test's working directory. `895abfe` had no such reference. This is inferred from reading, not tested; the fix is engine-side (`CMAKE_CURRENT_SOURCE_DIR/../..` or `PROJECT_SOURCE_DIR`).
2. **The existing build directory is dead.** `build/CMakeCache.txt:485` has `RIFTBOUND_ENGINE_DIR=/home/pirateporkchop/code/riftbound-engine-v6`, which no longer exists. The binaries in `build/` are engine `895abfe`.
3. **`scripted_agent.cpp` is now compiled twice.** `CMakeLists.txt:89-94` compiles it into `riftbound_match_lib` because it was "not yet in the engine's own source list"; engine `CMakeLists.txt:420` now has it in `riftbound_core`. It probably links, but the comment is stale and the duplication should go.
4. **Result directories can be mislabelled.** `scripts/v6_baselines.sh:25-26` names the output directory from the engine's HEAD when the script starts, while the binary carries its own configure-time commit. This has already happened in `logs/baselines/b05e51d/`:
   - `mcts20_khazix…` and `mcts20_rengar…` say `engine_commit: b29156f-dirty`;
   - `mcts50_miss_fortune…` says `895abfe`.
5. **`ENGINE` path and the fix run.** `scripts/v6_baselines.sh:18` and `CMakeLists.txt:16` assume `../riftbound-engine`, whose main checkout is `master` at `6a3e52b` and moves at each stage gate.
6. **`applyChoice` now throws on a bad index.** `tests/test_knowledge_gate.cpp:92-94` passes the `std::find` index and `tests/test_scripted_agent.cpp:89` passes `selectIndex`. Both look in-range; a bug would now surface as a throw instead of silently applying action 0.
7. **Engine exceptions in the match runner.** `src/match/match_runner.cpp:113-116` aborts the whole match on any exception, which is consistent with the new "failure, not a draw" rule. However `terminationKind` (lines 36-43) has no case for the new `"engine_error"` reason and would file it under `"other"` with winner `None`, i.e. a draw. I did not confirm whether `runGame` can return that or always throws.
8. **`MctsAgent` registry argument.** `src/match/agent_factory.cpp:55` still passes `"cards/registry.json"`. It is harmless today because the engine ignores it.
9. **Knowledge-gate source scan.** The allow-list at `tests/test_knowledge_gate.cpp:123-128` still matches engine master by my grep (only `game_engine.cpp:426`, `state_editor.cpp:310` and `deck_validator.cpp:322` outside `deck_ops.cpp`). Stage 3 card rewrites could add a direct `main_deck` edit and fail it.
10. **Cards asking more or different `MakeChoice` prompts.** No v6 code depends on prompt counts, but every baseline shifts, and the scripted agent's rule tests (`tests/test_scripted_agent.cpp:44-93`, `BeatsRandomInAMirror` at 97-112) are behaviour-sensitive.
11. **Stage 5 against the v6 token layout.** `interfaces.md` fixes 1101 card rows and the token widths. The engine's `plans/README.md:180-193` tells the stage 5 stream to close its items against v6 if v6 has replaced the encoding. v6 has only headers so far (`token_layout.h`, `kLayoutVersion = 1`), so stage 5 may still change the old flat layout and vocabulary; that only affects legacy code and checkpoints, which are already unloadable.
12. **Legacy code only (not built):**
    - `InfoSetId`: `src/ml/rebel_inference.cpp:28-30`.
    - `ApplyAction` now throwing: `src/training/self_play.cpp:218,460,470,500,809,819,847` and `src/ml/rebel_inference.cpp:93`.
    - Deleted `riftbound_openspiel` / `openspiel_match.cpp`: `src/training/config_driver.cpp:683-741`, `scripts/baseline_matchups.sh:22`, `scripts/sample_replays.sh:68`, `docs/openspiel-match-libtorch-extension.md`.
    - `registry.json`: `src/runner/probe_v2_model.cpp`, `src/runner/probe_v3_model.cpp`, `src/training/riftbound_train.cpp:51`, `tests/test_entity_tokens.cpp:32-47`.
    - No trainer code uses `actionHistory()` or `makeFromSnapshot`.

## 5. Contradictions and stale claims

- **MCTS-20/50:** plan line 68 says "MCTS-20 and MCTS-50 not run" and line 98 says none should be run before the engine fixes. Nine such result files exist, written after the last commit.
- **Phase 0 status:** plan line 240 says "not started"; the Progress table says done.
- **Engine master:** plan line 54 says master is `895abfe`; it is `6a3e52b`, 178 commits later.
- **No-op activation:** plan lines 166-172 name Sett 543 as the open cause; engine commit `d06b58d` fixed it. Last Rites 471 was touched in `76abd99`, and I did not check whether its Equip no-op is fixed. The `games_at_search_cap` figures are therefore stale.
- **Gate run tense:** plan line 318 speaks of the 200-game gate run as future; it has run.
- **`interfaces.md` status:** it says "proposed … awaiting owner review" while the plan says the headers are merged.
- **Direct deck edits:** `interfaces.md` still says "27 card files pop `main_deck` directly"; the P1.1 audit corrected this to 14 sites in 13 files, and P1.3 routed all of them.
- **Reveal audit:** `p1.1-reveal-audit.md` lists engine defects against `a2776e5`; the Repeat rune one is fixed and others have probably been closed by the fix run. Not cross-checked.
- **`docs/backlog.md`** (May) is wholly pre-v6:
  - "Wire up trainer CMakeLists.txt" is unchecked;
  - it speaks of `find_package`, `registry.json`, "536 unit tests" and `observed_cards` as the plan of record.
- **Old dimensions:** `docs/additional-gamestate-dims.md` and the V3–V5 design docs describe a 4623-dim, 787-card vector with a 9615-slot vocabulary; the engine is at 6188 / 1100.
- **V5 design doc:** `docs/v5-osfp-design.md` says "Status: Plan / pre-implementation" although V5 code and checkpoints exist.
- **Census paths:** the census JSONs record deck paths under `../riftbound-engine-v6/`.
- **Engine `CLAUDE.md:3`** points to the trainer's `docs/` for project status; the live status is in `plans/tooling-v6/README.md`.

## 6. Most valuable next steps

**Can be done now, low CPU:**
1. **Reconcile the plan with reality.** Fix the stale lines in section 5, commit or discard the four untracked MCTS files, and move the three mislabelled files out of `logs/baselines/b05e51d/`.
2. **Harden `scripts/v6_baselines.sh`.** Take the directory name from the binary's own commit (or refuse on mismatch), and add an `ENGINE` / `THREADS` policy for coexisting with the fix run.
3. **Answer the four owner questions and review the Wave 0 interfaces** before stage 5 starts, since stage 5 is told to check v6. Decide explicitly whether stage 5 should close items 1.9, 1.12 and 7.33 against v6 rather than churn the old layout.
4. **Hand the engine run a trainer-compat item:** the `CMAKE_SOURCE_DIR` use in `src/openspiel/CMakeLists.txt:110,125,127`. On the trainer side, drop `scripted_agent.cpp` from `CMakeLists.txt:94` and add an `engine_error` case to `terminationKind`.
5. **Write the missing `CLAUDE.md` and a memory note** so the next trainer session starts from the plan, not from `docs/backlog.md`.
6. **Start design-only or stub-only v6 work** that the plan says depends only on the frozen layout: P4.1/P4.2 (model and tests against synthetic tensors) and the P5.2 record format. This needs libtorch wired into `CMakeLists.txt`, which is currently absent.

**Must wait for the engine's final merge:**
1. **Reconfigure and rebuild** against final master with a fresh build directory, then run `trainer_tests`.
2. **Re-record everything:** the full baseline table and the collision census (never re-run after `a2776e5`), and re-measure cap hits.
3. **Check whether MCTS now scales with simulations.** This is the first honest test of the fixes to 1.1 and 1.2.
4. **P1.3 part 2 and Phase 2 (P2.2 action encoding).** Both edit engine files the fix run is rewriting (`effect_executor.cpp`, `riftbound_state.cpp`, `action_vocab.*`, card files).
5. **Freeze the token layout** only after stage 5 lands, then move to Phase 3 and the Phase 5 data generation.

## Could not determine

- Whether the trainer actually configures and builds against engine master (item 4.1 is from reading only).
- Whether the four deck lists are still legal under current card data.
- Whether `runGame` can return `"engine_error"` or always throws.
- Whether the Last Rites no-op is fixed.
- Which process or instruction started the MCTS-20/50 sweep against the plan's own rule. The session transcript (4.2 MB, ends 19:08 on 2026-10-10) was not read, as instructed.
- Any trainer intent recorded only in that transcript.
