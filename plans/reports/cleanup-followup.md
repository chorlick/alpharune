# Follow-up

Stream cleanup, worktree `/home/pirateporkchop/code/riftbound-engine/.worktrees/cleanup`, branch `fix/cleanup`. `fix/integration` is merged in (`f7375a9`, includes `f3670f3`). Working tree clean. New commits:

| Commit | Content |
| --- | --- |
| `bbe7381` | tests: every `-Wall -Wextra` warning in `tests/` fixed |
| `2c7d674` | tests: `playThroughEngine` runs the engine's post-intent cleanup; three tests set up state that survives it |
| `0227a08` | build: CMake warning and CLAUDE.md say a PCH is always compiled, never cached |

Final state, lane A lock, `-j3`, launcher `scripts/ccache_pch_safe.sh`: full build exit 0; `riftbound_tests` 2248 passed, 0 failed, 1 disabled (the count rose from 2206 with the merged sweep tests).

## 1. Warnings (5.12 carry-over)

Counts are unique `file:line:col` warnings from a clean build of `riftbound_tests` and `riftbound` (`cmake --build build --target clean`, then build, output saved and grepped for `warning:`). "Before" is at `a8723a4`; "after" is at `0227a08`, after the merge.

| Directory | Before | After | Owner |
| --- | --- | --- | --- |
| `tests/` | 41, plus 1 libstdc++ warning raised from `tests/test_chain.cpp` | 0 | cleanup: fixed |
| `src/io`, `src/main.cpp`, `src/agents`, `src/ml`, `src/rules`, `src/core` | 0 | 0 | cleanup: nothing to fix |
| `src/engine` | 3 | 3 | not mine: listed below |
| `src/cards/card.h` | 18 | 18 | sweep-verify-cards: listed below |
| `src/cards/<type>/` | 25, plus 2 libstdc++ warnings raised from card files | 25, plus 2 | sweep-verify-cards: listed below |

The integrity stream's "src/io (3)" no longer exists: the clean build shows none before or after.

What was fixed in `tests/`:
- 7 dangling-else (`if (...) EXPECT_...;` now braced).
- 22 missing-field-initializers (`UnitDiedEvent{x, p}` now names `LocationId{}, 0`, the values it already got).
- 9 unused variables and 2 unused parameters.
- 1 maybe-uninitialized in libstdc++ `<variant>` from `tests/test_chain.cpp:1095` (`optional::emplace` instead of assignment).
- 1 `-Wtype-limits` in `tests/cards/test_equipment.cpp` that was a real helper bug: `addGearOnBoard` took `BattlefieldId at_bf = -1` (unsigned), so `at_bf >= 0` was always true and default-placed gear went to battlefield 4294967295 instead of the base. The parameter is now `int`. No test changed result.

Not fixed, for other owners:
- **perf-search:** `src/openspiel/openspiel_tests.cpp:94:8` dangling-else. This only shows in a full build; the file is not part of the two targets above.
- **Engine owners:**
  - `src/engine/chain_manager.cpp:186:14` unused variable `finalized_something`
  - `src/engine/game_engine.cpp:4457:10` `greedyCascade` set but not used
  - `src/engine/trigger_manager.cpp:499:10` `bf_loc` set but not used
- **sweep-verify-cards, `src/cards/card.h`** (18, all unused-parameter in default virtual bodies): lines 74, 75, 79, 86, 87, 235, 236, 547 (2), 587 (2), 605, 606, 614 (2), 632, 633, 634.
- **sweep-verify-cards, card files** (25, all unused-parameter):
  - gear, `unit` and/or `targets`: `0408_world_atlas`, `0430_warmog_s_armor`, `0437_trinity_force`, `0439_boneshiver`, `0445_doran_s_ring`, `0455_cull`, `0493_sacred_shears`, `0507_forgefire_cape`, `0658_hunter_s_machete`
  - units, `targets`: `0066_ahri_alluring`, `0073_sona_harmonious`, `0075_tasty_faefolk`, `0087_lecturing_yordle`, `0096_watchful_sentry`, `0137_stormclaw_ursine`, `0190_kog_maw_caustic`, `0216_soaring_scout`, `0714_black_rose_dignitary`
- **sweep-verify-cards, libstdc++ warnings raised from card code:** `-Wfree-nonheap-object` in a lambda in `DisposalOrder::onResolve` (spells); an array-bounds `memcpy` warning from a `std::string` `operator+` in `GarbageGrabber::onActivate` (gear). I did not investigate whether either is a real defect.

## 2. Fixture: cleanup after an intent

Done as a fixture-side change; no engine change was needed.

- `playThroughEngine` in `tests/cards/card_test_fixture.h` now runs the sequence `GameEngine::resolveMainPhaseDecision` runs: execute the intent, run the chain, `cleanup()`, run the chain again. All four hooks already existed.
- Three tests relied on the old behaviour. Each was corrected at its setup or assertion, none weakened:
  - `C3Test.CurtainCall_TargetFitsTheModeAndALaterCastStartsFresh` asserted `damage_marked == 6` on a 6-Might unit. That is the case cards-c2 described. It now asserts the unit is in the trash after the second cast.
  - `C3Test.HereToHelp_MayBeDeclinedAndNeverPlaysToBase` and `ReviewFixC2Test.GuerillaWarfare_ChoosesTheCardsAndHidingIsFreeThisTurn` set a battlefield's controller with no unit there, and cleanup took the control away. Each now places a P1 unit at the battlefield.
- Scope limit: only `playThroughEngine` changed. The 61 direct `testHook_executeIntent` calls in tests still get no cleanup, and `fireTriggerThroughEngine` / `resolveThroughEngine` were not changed. I did not audit the remaining `damage_marked` assertions; those that still pass assert non-lethal damage or use a direct call.

## 3. `play_server.cpp` / `play_index_html.cpp` in `riftbound_core`

Complete.

- Both files are in the `riftbound_core` source list in `CMakeLists.txt`, under the comment "In the library (not only in the `riftbound` binary) so tests link it instead of #including the .cpp files".
- `grep -rn '#include ".*\.cpp"' tests src` returns nothing: no test or source includes a `.cpp` file.
- `RIFTBOUND_TEST_UNITY_EXCLUDE` is empty ("no test file needs it at present"), so `tests/test_review_fix_io_cli.cpp` is compiled in a unity batch like every other test file and does not need an exclusion. The suite links and passes that way.

## 4. PCH launcher after `f3670f3`

- **Docs:** the CLAUDE.md "ccache" section and the CMake configure warning now say that a PCH is always compiled and never stored in or taken from the cache, and give both reasons (stale-`.gch` keying; the preprocessed-text hit after a comment-only edit that leaves stale `#pragma once` records). The earlier advice that a clean build is a safe fallback is removed, because a clean build does not avoid the second failure.
- **Reproduction, comment-only case:** added as step 3 of the two-copy experiment. In copy A: build the PCH and `card_db.cpp.o`, append a comment to `core/types.h` and rebuild, append a second comment and rebuild, restore the earlier bytes and rebuild.
  - The fixed launcher survives it: all four builds succeed, and step 2 still gives each copy only its own `.gch`.
  - Plain `ccache` also built all four, so my sequence does **not** reproduce the "redefinition of kInvalidId" failure you hit on integration. It is not a discriminating test for that failure. The step-2 mix-up still reproduces with plain `ccache` (copy B's `.gch` has copy A's function and lacks its own).
- **Cost observed with the fixed launcher:** `card_db.cpp.o` missed the cache in all four step-3 builds, including the last one where the sources were byte-identical to the first. A recompiled `.gch` appears not to match the previous one, so units that use the PCH are cold after every PCH rebuild. That is a build-time cost, not a correctness problem; I did not measure it beyond this one object.
