# Stream cleanup: final report

Worktree `/home/pirateporkchop/code/riftbound-engine/.worktrees/cleanup`, branch base `0ca1b19`, four commits, working tree clean.

| Commit | Content |
| --- | --- |
| `5e6bbbd` | build: PCH-safe ccache launcher, engine-relative OpenSpiel paths, `riftbound_openspiel_tests` registered with ctest |
| `681533e` | io: first shutdown signal ends a spectator game; god-mode edit recomputes the pending main-phase decision |
| `2c2a2e0` | tests: Jhin and equipment trigger helpers go through the engine path |
| `e9f8b79` | cleanup: stale comments, dead helpers and scripts, README / CLAUDE.md, plan outcomes for 5.13 / 5.14 |

## Verification after the interruption

Everything below was re-run after the resume, under the lane A lock at `-j3`.

- Full build (`cmake --build build -j3`, launcher `scripts/ccache_pch_safe.sh`): exit 0, no configure warning.
- `RIFTBOUND_ROOT=. ./build/riftbound_tests`: 2206 passed, 0 failed, 1 disabled.
- New server tests (`PlayServerShutdown.*:PlayServerGodMode.*`) repeated 15 times: 15 of 15 green.
- `ctest --test-dir build -R '^riftbound_openspiel_tests$'`: 1 of 1 passed.

## Carry-over items

### FIXED: trainer cannot configure against the engine (`src/openspiel/CMakeLists.txt`)
- The three `${CMAKE_SOURCE_DIR}` uses (the `mcts_agent.cpp` source path, the test `WORKING_DIRECTORY`, `RIFTBOUND_ROOT`) are now `${PROJECT_SOURCE_DIR}`.
- A grep of every tracked `CMakeLists.txt` and `*.cmake` finds no other `CMAKE_SOURCE_DIR` or `CMAKE_BINARY_DIR` use. The one in a root `CMakeLists.txt` comment was reworded.
- Verification: a throwaway parent project (`project()` plus `add_subdirectory(<worktree> engine EXCLUDE_FROM_ALL)`) in the scratchpad, configured into a fresh build directory:
  `cmake -S <scratch>/parent -B <scratch>/parent_build2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_OPEN_SPIEL=... -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=...`
  Result: exit 0, "Generating done", and the generated `build.ninja` references `<worktree>/src/agents/mcts_agent.cpp`.
- Only configure and generate were run for the parent project; it was not built. The trainer repo was not touched.

### FIXED (found while verifying the above): `riftbound_openspiel_tests` was never registered with ctest
- `enable_testing()` came after `add_subdirectory(src/openspiel)`, so the `add_test` there was silently dropped: `ctest -R openspiel` reported "No tests were found" and there was no `build/src/openspiel/CTestTestfile.cmake`.
- `enable_testing()` now precedes the `add_subdirectory`. `ctest -N` total went from 2373 to 2374 and the test runs and passes through ctest.

### FIXED: ccache hands back a PCH built from other headers
- Mechanism: CMake rebuilds a PCH with the old `.gch` still in place; plain ccache keys the result on the old `.gch` instead of on the headers.
- Reproduced with two scratch copies of the repository sharing one cache, `core/types.h` changed differently in each: with plain `ccache`, copy B's `.gch` contained copy A's new function and not its own. With `scripts/ccache_pch_safe.sh` each `.gch` contained only its own.
- It also happens in a single tree (minimal experiment: clean build, edit, clean build at the original, edit differently).
- The new launcher deletes the stale `.gch` before a PCH compile. CMake warns at configure time when PCH is on and the launcher is plain ccache. CLAUDE.md "ccache" is rewritten accordingly.
- Other worktrees still configured with plain `ccache` remain exposed until they are reconfigured with the script or built from clean.

### FIXED (io-cli): first Ctrl-C did not end a game with no human seat
- Test `PlayServerShutdown.FirstShutdownEndsASpectatorGameAtTheNextDecision`: after `requestShutdown()` no agent is consulted again and the result's termination reason contains "conceded".

### FIXED (io-cli): god-mode edit left the pending decision's legal list stale
- At the main-phase decision the list is rebuilt, `edit_ok` carries `"legal_recomputed": true`, the decision gets a new id, and a `choose` for the old id is refused as stale.
- At any other decision and at the spectator gate the list is kept and `edit_ok` says `"legal_recomputed": false`.
- Tests `PlayServerGodMode.EditRecomputesThePendingMainPhaseDecision` and `PlayServerGodMode.EditAtANestedDecisionSaysTheListWasKept`; `docs/play-api.md` updated.

### FIXED: file-local trigger helpers bypassed finalization
- `invokeOnTrigger` (`tests/cards/test_jhin_deck.cpp`) and `fireWeaponmasterTrigger` (`tests/cards/test_equipment.cpp`) now go through `fireTriggerThroughEngine`.
- Six Virtuoso tests failed on the first run after this change because the helper passed no trigger subject. The helper now passes the played spell as subject, as `TriggerManager::onSpellResolved` does. No engine or card code was changed for this.

## Plan items 5.13 and 5.14: not closed

Per-sub-point outcomes are recorded in `plans/05-integrity.md`.

- **Done in files this stream owns:**
  - `zoneToStr` and the `nameOfId` lambda removed.
  - `scripts/audit_deck_cards.py` removed (it globbed directories that do not exist).
  - `scripts/generate_replays.py` rewritten against `build/riftbound`; it was not run.
  - Stale comments fixed in `legal_action.h`, `card_registry.{h,cpp}`, `card_db.cpp`, `game_object.h`, `types.h`, `mcts_agent.cpp`, `CMakeLists.txt`, README and CLAUDE.md.
- **Stale finding:** `executePlayFromHidden` is called now (`GameEngine::executeIntent`, the `card.is_hidden` branch).
- **Still open, handed to sweep-verify because the files belong to running sweep streams:**
  - `src/engine/game_engine.{h,cpp}`: `swapBackBattlefield`, `generateCombatDamageActions`, `advanceCleanup` and `activePlayer` have no callers; `executeEndTurn` is empty; `had_temp_might` is unused; `contested_by` / `combat_staged` are left set after combat resolution; the header still says "stubbed" and "worker thread".
  - `src/core/events.h`: `CleanupNeededEvent` has no emitter.
  - `src/core/game_state.h`: `CostPaymentCursor` is labelled POD but holds a `std::vector`; the `current_play_source` comment is wrong.
  - `src/cards/cards_init.cpp` line 1 still says "Auto-generated".

  These are real and unfixed. Under the nothing-optional rule, 5.13 and 5.14 stay open until sweep-verify lands them.

## Caveat on coverage

The original stream brief was not in the resumed transcript. This report covers what the diff, the plan file and the coordinator messages show. If the brief listed other items, they need checking against this list.
