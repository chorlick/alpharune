cmake_minimum_required(VERSION 3.20)
project(riftbound-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Version stamp ────────────────────────────────────────────────────────────
# Capture the current git commit at configure time so binaries + HTML replays
# can be tied back to a specific build. Falls back to "unknown" outside a git
# checkout.
#
# Auto-refresh on new commits: CMake re-runs whenever .git/HEAD or the
# packed-refs file changes (configure_file with COPYONLY adds them as
# CMake input dependencies). New commit lands → next `cmake --build` runs
# CMake → version stamp updates. No manual `touch CMakeLists.txt` needed.
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE RIFTBOUND_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE RIFTBOUND_GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT "${RIFTBOUND_GIT_DIRTY}" STREQUAL "")
        set(RIFTBOUND_GIT_COMMIT "${RIFTBOUND_GIT_COMMIT}-dirty")
    endif()

    # Force CMake re-run when HEAD moves (new commit, branch switch, etc.)
    # or when packed-refs is rewritten (push, fetch). configure_file with
    # COPYONLY registers them as CMake input dependencies — Ninja then
    # re-runs cmake automatically before the next build.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/.git/HEAD"
                       "${CMAKE_CURRENT_BINARY_DIR}/.git_HEAD" COPYONLY)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git/packed-refs")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/.git/packed-refs"
                       "${CMAKE_CURRENT_BINARY_DIR}/.git_packed_refs" COPYONLY)
    endif()
endif()
if(NOT RIFTBOUND_GIT_COMMIT)
    set(RIFTBOUND_GIT_COMMIT "unknown")
endif()

string(TIMESTAMP RIFTBOUND_BUILD_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/core/version.h
    @ONLY
)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/generated)

option(RIFTBOUND_BUILD_OPENSPIEL "Build the OpenSpiel out-of-tree game wrapper (~adds OpenSpiel + abseil to _deps)" OFF)
option(RIFTBOUND_BUILD_LIBTORCH  "Build LibTorch-dependent targets (riftbound_train, LibTorchAgent). Adds ~200MB FetchContent unless LIBTORCH_DIR is set." OFF)

# ─── Dependencies ─────────────────────────────────────────────────────────────

find_package(Boost REQUIRED COMPONENTS program_options filesystem system)
find_package(Threads REQUIRED)

# nlohmann_json: prefer system. If RIFTBOUND_BUILD_OPENSPIEL=ON, OpenSpiel's
# bundled json subproject defines the `nlohmann_json` + `nlohmann_json::nlohmann_json`
# targets first; we guard find_package to avoid a target-name collision.
include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# OpenSpiel — gated; off by default to keep clean-build time small for everyone
# who isn't iterating on the OpenSpiel wrapper.
#
# Build mechanics worth knowing:
# 1. OpenSpiel's CMakeLists.txt reads ENV vars (not CMake cache vars) to gate
#    its optional features, so we set them via set(ENV{...}) before populating.
# 2. OpenSpiel does NOT ship abseil-cpp / nlohmann_json as git submodules — its
#    install.sh script clones them into the source tree, then CMake assumes
#    they're already there. We run install.sh between Populate and add_subdirectory.
if(RIFTBOUND_BUILD_OPENSPIEL)
    set(ENV{OPEN_SPIEL_BUILD_WITH_PYTHON}   "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_HANABI}   "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_ACPC}     "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_JULIA}    "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_LIBNOP}   "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_LIBTORCH} "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_ROSHAMBO} "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_GAMUT}    "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_ORTOOLS}  "OFF")
    set(ENV{OPEN_SPIEL_BUILD_WITH_XINXIN}   "OFF")
    set(ENV{OPEN_SPIEL_ENABLE_JAX}          "OFF")
    set(ENV{OPEN_SPIEL_ENABLE_PYTORCH}      "OFF")
    set(ENV{OPEN_SPIEL_ENABLE_PYTHON_MISC}  "OFF")

    FetchContent_Declare(open_spiel
        GIT_REPOSITORY https://github.com/google-deepmind/open_spiel.git
        GIT_TAG v1.6.14
    )
    FetchContent_GetProperties(open_spiel)
    if(NOT open_spiel_POPULATED)
        FetchContent_Populate(open_spiel)

        # OpenSpiel's CMakeLists.txt does `add_subdirectory(abseil-cpp)` /
        # `add_subdirectory(json)` and includes `pybind11_json/include` —
        # but those dirs aren't shipped in the source tree. Their install.sh
        # clones them in, but it also tries to `sudo apt-get install` system
        # packages, which can't run from CMake.
        #
        # So we do just the clones install.sh would do, skipping the
        # system-package step entirely. Pinned versions match v1.6.14's
        # install.sh as of 2026-05.
        function(_riftbound_clone_dep TARGET_DIR GIT_URL GIT_REF)
            if(NOT EXISTS "${TARGET_DIR}")
                message(STATUS "  Cloning ${GIT_URL} @ ${GIT_REF}")
                execute_process(
                    COMMAND git clone --depth 1 --branch ${GIT_REF} ${GIT_URL} ${TARGET_DIR}
                    RESULT_VARIABLE _rc
                    OUTPUT_QUIET ERROR_QUIET
                )
                if(NOT _rc EQUAL 0)
                    message(FATAL_ERROR "Failed to clone ${GIT_URL} @ ${GIT_REF} into ${TARGET_DIR}")
                endif()
            endif()
        endfunction()

        message(STATUS "Preparing OpenSpiel source tree (clones abseil-cpp + json + pybind11_json)...")
        _riftbound_clone_dep(
            ${open_spiel_SOURCE_DIR}/open_spiel/abseil-cpp
            https://github.com/abseil/abseil-cpp.git
            20250814.1)
        _riftbound_clone_dep(
            ${open_spiel_SOURCE_DIR}/open_spiel/json
            https://github.com/nlohmann/json.git
            v3.11.3)
        # pybind11_json's include dir is unconditionally added to
        # OpenSpiel's include path (even with PYTHON=OFF). Headers-only,
        # so cloning it is enough — no build cost.
        _riftbound_clone_dep(
            ${open_spiel_SOURCE_DIR}/open_spiel/pybind11_json
            https://github.com/pybind/pybind11_json.git
            master)
        # DDS (double-dummy-solver) is referenced unconditionally by
        # games/CMakeLists.txt because the bridge game can't be disabled
        # via a flag. Cloning it is enough — we never link the bridge
        # game target from our wrapper.
        _riftbound_clone_dep(
            ${open_spiel_SOURCE_DIR}/open_spiel/games/bridge/double_dummy_solver
            https://github.com/jblespiau/dds.git
            develop)

        add_subdirectory(${open_spiel_SOURCE_DIR}/open_spiel ${open_spiel_BINARY_DIR})
    endif()

    add_subdirectory(src/openspiel)
endif()

# ─── LibTorch (opt-in) ────────────────────────────────────────────────────────
# Off by default. Configure with -DRIFTBOUND_BUILD_LIBTORCH=ON to enable
# training (riftbound_train) and inference (LibTorchAgent) targets.
#
# Resolution order for the libtorch root:
#   1. LIBTORCH_DIR (CMake cache var) — point at an extracted libtorch tree
#   2. ${LIBTORCH_DIR} env var — same
#   3. FetchContent from download.pytorch.org (CUDA 12.1 build ~2.5 GB, or
#      CPU build ~200 MB if RIFTBOUND_LIBTORCH_VARIANT=cpu).
#
# Pinned at PyTorch 2.5.1.
#
# Default: CPU. To enable CUDA, pass -DRIFTBOUND_LIBTORCH_VARIANT=cu121
# AND install the matching Linux CUDA toolkit (not just the WSL driver):
#
#   sudo apt install nvidia-cuda-toolkit         # quick path, gives nvcc
#   # or follow NVIDIA's CUDA-for-WSL2 install guide for a newer toolkit
#
# The CUDA-flavored libtorch zip bundles its own runtime libs (cudart,
# cudnn, etc.) but its CMake config still calls find_package(CUDA) and
# requires nvcc + headers to be discoverable.
set(RIFTBOUND_LIBTORCH_VARIANT "cpu" CACHE STRING
    "LibTorch FetchContent variant: cpu (~200MB, default) or cu121 (~2.5GB, needs Linux CUDA toolkit)")
set_property(CACHE RIFTBOUND_LIBTORCH_VARIANT PROPERTY STRINGS cpu cu121)

if(RIFTBOUND_BUILD_LIBTORCH)
    if(NOT LIBTORCH_DIR AND DEFINED ENV{LIBTORCH_DIR})
        set(LIBTORCH_DIR $ENV{LIBTORCH_DIR})
    endif()
    if(NOT LIBTORCH_DIR)
        set(_libtorch_url
            "https://download.pytorch.org/libtorch/${RIFTBOUND_LIBTORCH_VARIANT}/libtorch-cxx11-abi-shared-with-deps-2.5.1%2B${RIFTBOUND_LIBTORCH_VARIANT}.zip")
        message(STATUS "Fetching LibTorch 2.5.1 ${RIFTBOUND_LIBTORCH_VARIANT} (cxx11-abi)...")
        message(STATUS "  URL: ${_libtorch_url}")
        if(RIFTBOUND_LIBTORCH_VARIANT STREQUAL "cu121")
            message(STATUS "  (CUDA build ~2.5 GB — one-time download cached in build/_deps/)")
        endif()
        FetchContent_Declare(libtorch URL "${_libtorch_url}")
        FetchContent_MakeAvailable(libtorch)
        set(LIBTORCH_DIR ${libtorch_SOURCE_DIR})
    endif()
    set(Torch_DIR ${LIBTORCH_DIR}/share/cmake/Torch)
    find_package(Torch REQUIRED)
    message(STATUS "LibTorch found at ${LIBTORCH_DIR}")
    message(STATUS "  Torch CXX flags: ${TORCH_CXX_FLAGS}")
    set(RIFTBOUND_HAVE_LIBTORCH ON CACHE INTERNAL "")
endif()

# nlohmann_json: only call find_package if OpenSpiel didn't already provide it.
# (When RIFTBOUND_BUILD_OPENSPIEL=ON, OpenSpiel's bundled json subproject
#  defines both `nlohmann_json` and `nlohmann_json::nlohmann_json`.)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json REQUIRED)
endif()

# ─── JSON Schema validator ────────────────────────────────────────────────────
# Header-mostly library for JSON Schema draft 7 validation. Used by the config
# infra (Phase 6a). Vendored via FetchContent rather than system-installed
# because the package isn't reliably available across distros; v2.3.0 is small
# (~2k LOC) and stable.
FetchContent_Declare(json_schema_validator
    GIT_REPOSITORY https://github.com/pboettch/json-schema-validator.git
    GIT_TAG 2.3.0
)
set(JSON_VALIDATOR_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JSON_VALIDATOR_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json_schema_validator)

# ─── Core library ─────────────────────────────────────────────────────────────

add_library(riftbound_core
    src/core/types.cpp
    src/core/intent.cpp
    src/core/game_state.cpp
    src/core/card_db.cpp
    src/rules/deck_validator.cpp
    src/engine/game_engine.cpp
    src/engine/chain_manager.cpp
    src/engine/effect_executor.cpp
    src/engine/trigger_manager.cpp
    src/io/state_renderer.cpp
    src/io/replay_writer.cpp
    src/cards/card.cpp
    src/cards/card_registry.cpp
    src/cards/generated/unit_cards.cpp
    src/cards/generated/spell_cards.cpp
    src/cards/generated/gear_cards.cpp
    src/cards/generated/legend_cards.cpp
    src/cards/generated/battlefield_cards.cpp
    src/cards/generated/rune_cards.cpp
    src/cards/generated/registry_init.cpp
    src/cards/manual/equip_cards.cpp
    src/cards/manual/weaponmaster_cards.cpp
    src/cards/manual/deck_cards.cpp
    src/engine/game_runner.cpp
    src/engine/batch_runner.cpp
    src/engine/step_driver.cpp
    src/ml/feature_extractor.cpp
    src/ml/entity_tokens.cpp
    src/openspiel/action_vocab.cpp
)

target_include_directories(riftbound_core PUBLIC src)
target_link_libraries(riftbound_core PUBLIC
    Boost::boost
    Boost::system
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ─── Engine library ──────────────────────────────────────────────────────────
# (will grow as we add engine, rules, agents, io modules)

# add_library(riftbound_engine
#     src/engine/game_engine.cpp
#     ...
# )
# target_link_libraries(riftbound_engine PUBLIC riftbound_core)

# ─── Main executable (legacy CLI — flag-driven) ───────────────────────────────
# Stays in tree during Phase 6. New work goes through the unified
# riftbound_runner binary below.

add_executable(riftbound src/main.cpp)
target_link_libraries(riftbound PRIVATE riftbound_core Boost::program_options)

# ─── Config infra (Phase 6a) ──────────────────────────────────────────────────
# Static library so config parsing is shareable between riftbound_runner,
# tests, and future training/play binaries without duplicating the
# json-schema-validator link.

add_library(riftbound_config STATIC
    src/config/schema.cpp
    src/config/config.cpp
)
target_include_directories(riftbound_config PUBLIC src)
target_link_libraries(riftbound_config PUBLIC
    riftbound_core
    nlohmann_json_schema_validator
)

# ─── Unified runner (Phase 6a) ────────────────────────────────────────────────
# The new entry binary. CLI is just --config + --dry-run; everything else
# goes through the config file. Play and training dispatch are stubbed in
# Phase 6a — Phases 6b–6i wire them up.

add_executable(riftbound_runner src/runner/riftbound_runner.cpp)
target_link_libraries(riftbound_runner PRIVATE
    riftbound_config
    Boost::program_options
)
# Wire the V2 training pipeline into the runner when LibTorch is built.
# Without these, `mode=train` configs print a "not wired" stderr and
# exit non-zero. The training-mode dispatch is gated behind the
# compile definition so CPU-only minimal builds still link cleanly.
if(RIFTBOUND_HAVE_LIBTORCH AND RIFTBOUND_BUILD_OPENSPIEL)
    target_sources(riftbound_runner PRIVATE
        src/openspiel/riftbound_game.cpp
        src/openspiel/riftbound_state.cpp
    )
    target_link_libraries(riftbound_runner PRIVATE
        riftbound_training
        open_spiel_core
        $<TARGET_OBJECTS:game_transforms>
    )
    target_include_directories(riftbound_runner PRIVATE
        ${open_spiel_SOURCE_DIR}
        src/openspiel
    )
    target_compile_definitions(riftbound_runner PRIVATE RIFTBOUND_HAVE_LIBTORCH)
endif()

# ─── ML library (LibTorch-dependent) ─────────────────────────────────────────
# Separate static library so the LibTorch include path / link list don't
# leak into riftbound_core's compile units. Only built when
# RIFTBOUND_BUILD_LIBTORCH=ON.
if(RIFTBOUND_HAVE_LIBTORCH)
    # The V2 evaluator (libtorch_evaluator.cpp) reaches into RiftboundState
    # via engineState() / RiftboundGame::cardDb() to pull entity tokens —
    # it needs the openspiel headers on the include path but does NOT
    # link those .cpp into riftbound_ml (riftbound_game.cpp's
    # REGISTER_SPIEL_GAME initializer would be stripped from a static
    # lib). The final executable (riftbound_train etc.) links the
    # openspiel .cpp files directly.
    set(RIFTBOUND_ML_SOURCES
        src/ml/v2_model.cpp
        src/ml/v3_model.cpp
    )
    if(RIFTBOUND_BUILD_OPENSPIEL)
        list(APPEND RIFTBOUND_ML_SOURCES
            src/ml/libtorch_evaluator.cpp
        )
    endif()
    add_library(riftbound_ml STATIC ${RIFTBOUND_ML_SOURCES})
    target_include_directories(riftbound_ml PUBLIC src)
    target_link_libraries(riftbound_ml PUBLIC
        riftbound_core
        "${TORCH_LIBRARIES}"
    )
    # libtorch_evaluator.cpp uses the OpenSpiel Evaluator interface
    # (mcts.h). Pull in the OpenSpiel include path AND the bundled
    # abseil-cpp headers when building the ML library — mcts.h
    # transitively pulls in absl::random which lives one directory
    # deeper than the open_spiel include root.
    if(RIFTBOUND_BUILD_OPENSPIEL)
        target_include_directories(riftbound_ml PUBLIC
            ${open_spiel_SOURCE_DIR}
            ${open_spiel_SOURCE_DIR}/open_spiel/abseil-cpp
        )
        target_link_libraries(riftbound_ml PUBLIC
            open_spiel_core
            absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
            absl::optional absl::random_random absl::str_format absl::strings absl::time
        )
    endif()
    target_compile_options(riftbound_ml PUBLIC "${TORCH_CXX_FLAGS}")

    # ─── Training library + driver ──────────────────────────────────
    # Self-play + replay + trainer. Only built when both LibTorch AND
    # OpenSpiel are on (self-play uses MCTSBot, trainer uses LibTorch).
    if(RIFTBOUND_BUILD_OPENSPIEL)
        add_library(riftbound_training STATIC
            src/training/replay_buffer.cpp
            src/training/self_play.cpp
            src/training/trainer.cpp
            src/training/trainer_v3.cpp
            src/training/config_driver.cpp
            src/training/ring_logger.cpp
            src/training/watchdog.cpp
            src/training/heuristic_evaluator.cpp
        )
        target_link_libraries(riftbound_training PUBLIC
            riftbound_ml
            riftbound_config
            # OpenSpiel MCTSBot lives in the `algorithms` OBJECT lib
            # — pulled in by name since it isn't directly linkable.
            $<TARGET_OBJECTS:algorithms>
            $<TARGET_OBJECTS:utils>
        )

        # The training driver needs RiftboundGame's REGISTER_SPIEL_GAME
        # to fire (so LoadGame("riftbound(...)") finds the game). Pull
        # in those two source files directly — they aren't built into a
        # standalone library because they depend on open_spiel headers
        # that aren't on riftbound_core's include path.
        add_executable(riftbound_train
            src/training/riftbound_train.cpp
            src/openspiel/riftbound_game.cpp
            src/openspiel/riftbound_state.cpp
        )
        target_link_libraries(riftbound_train PRIVATE
            riftbound_training
            open_spiel_core
            Boost::program_options
            $<TARGET_OBJECTS:game_transforms>
        )
        target_include_directories(riftbound_train PRIVATE
            ${open_spiel_SOURCE_DIR}
            src/openspiel
        )
    endif()

    # ─── Value-head probe (Phase 6k-4 fallback diagnostic) ───────────
    # Standalone binary that loads a V2 checkpoint and prints value +
    # top-5 policy logits on hand-crafted GameStates with known
    # correct-value answers. Doesn't pull in OpenSpiel — uses raw
    # GameState construction.
    add_executable(probe_v2_model src/runner/probe_v2_model.cpp)
    target_link_libraries(probe_v2_model PRIVATE
        riftbound_ml
        nlohmann_json::nlohmann_json
    )
    target_compile_options(probe_v2_model PRIVATE "${TORCH_CXX_FLAGS}")

    # Phase 6v — V3 probe binary. Mirror of probe_v2_model for the V3
    # flat-feature MLP architecture.
    add_executable(probe_v3_model src/runner/probe_v3_model.cpp)
    target_link_libraries(probe_v3_model PRIVATE
        riftbound_ml
        nlohmann_json::nlohmann_json
    )
    target_compile_options(probe_v3_model PRIVATE "${TORCH_CXX_FLAGS}")
endif()

# ─── Tests ────────────────────────────────────────────────────────────────────

enable_testing()

set(RIFTBOUND_TEST_SOURCES
    tests/test_types.cpp
    tests/test_card_db.cpp
    tests/test_deck_validator.cpp
    tests/test_chain.cpp
    tests/test_action_vocab.cpp
    tests/test_observation_tensor.cpp
    tests/test_resume_resolution.cpp
    tests/test_legal_targets_gate.cpp
    tests/test_config.cpp
    tests/test_entity_tokens.cpp
    tests/cards/test_counter_spells.cpp
    tests/cards/test_stacked_deck.cpp
    tests/cards/test_champions_legends.cpp
    tests/cards/test_spells_misc.cpp
    tests/cards/test_equipment.cpp
    tests/cards/test_xp_units.cpp
    tests/cards/test_cost_payment.cpp
    tests/cards/test_baron_nashor.cpp
    tests/cards/test_sabotage.cpp
    tests/cards/test_ivern_deck.cpp
    tests/cards/test_karthus_deathknell.cpp
    tests/cards/test_tokens_cease_to_exist.cpp
    tests/cards/test_burn_out_and_scoring.cpp
    tests/cards/test_targeting_and_combat_invariants.cpp
    tests/cards/test_combat_damage_allocation.cpp
    tests/cards/test_combat_showdown_dispatch.cpp
    tests/cards/test_jhin_deck.cpp
)
if(RIFTBOUND_HAVE_LIBTORCH)
    list(APPEND RIFTBOUND_TEST_SOURCES
        tests/test_v2_model_forward.cpp
        tests/test_v3_model_forward.cpp
    )
endif()

add_executable(riftbound_tests ${RIFTBOUND_TEST_SOURCES})
# Per-card test fixture lives in tests/cards/. Bring tests/ onto the include
# path so #include "tests/cards/card_test_fixture.h" resolves.
target_include_directories(riftbound_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(riftbound_tests PRIVATE
    riftbound_core
    riftbound_config
    GTest::gtest_main
)
if(RIFTBOUND_HAVE_LIBTORCH)
    target_link_libraries(riftbound_tests PRIVATE riftbound_ml)
endif()

include(GoogleTest)
gtest_discover_tests(riftbound_tests)
