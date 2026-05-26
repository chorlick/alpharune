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

# OpenSpiel is mandatory — the riftbound_openspiel binary's MCTS / ISMCTS /
# random agents are the engine's first-class AI baselines and the integration
# surface that lets external tools plug their own agents into Riftbound games.
# First-time configure clones OpenSpiel + abseil + DDS into build/_deps
# (~30 sec); subsequent reconfigures are fast.
set(RIFTBOUND_BUILD_OPENSPIEL ON CACHE INTERNAL "" FORCE)

# ─── Dependencies ─────────────────────────────────────────────────────────────

find_package(Boost REQUIRED COMPONENTS program_options filesystem system context)
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
    # libtorch 2.5.1's CUDA cmake refers to CUDA::nvToolsExt — a target
    # that newer CUDA toolkits (12.4+) dropped. Stub it before
    # find_package(Torch) so the include doesn't fail with "ALIAS target
    # missing". This is a no-op on older toolkits where the real target
    # is provided. Found on Threadripper PRO + CUDA 12.6 vast.ai box.
    if(NOT TARGET CUDA::nvToolsExt)
        add_library(CUDA::nvToolsExt INTERFACE IMPORTED)
    endif()
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

# JSON Schema validator dep moved to riftbound-trainer along with the
# config parsing it backed. Engine builds without it.

# ─── Core library ─────────────────────────────────────────────────────────────

# Per-card sources: one translation unit per card under src/cards/<type>/.
# CONFIGURE_DEPENDS re-globs at build time so newly scaffolded cards are picked
# up without a manual reconfigure.
file(GLOB_RECURSE RIFTBOUND_CARD_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/units/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/spells/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/gear/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/legends/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/battlefields/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/runes/*.cpp
)

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
    src/io/state_editor.cpp
    src/cards/card.cpp
    src/cards/card_registry.cpp
    src/cards/cards_init.cpp
    ${RIFTBOUND_CARD_SOURCES}
    src/engine/game_runner.cpp
    src/engine/batch_runner.cpp
    src/engine/step_driver.cpp
    src/ml/feature_extractor.cpp
    src/ml/cfr_util.cpp
    src/openspiel/action_vocab.cpp
)

target_include_directories(riftbound_core PUBLIC src)
target_link_libraries(riftbound_core PUBLIC
    Boost::boost
    Boost::system
    Boost::context   # userspace fibers for the engine's step machine
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

# ─── Unified riftbound binary ────────────────────────────────────────────────
# One entry point for all modes: random / mcts / ismcts / human (1 or 2 seats),
# single-game or batch via --games, optional Beast HTTP+WebSocket web UI via
# --web. Pulls in OpenSpiel + the riftbound_game/state TUs so MctsAgent /
# IsMctsAgent can construct cloneable OpenSpiel states internally.
#
# The riftbound_game.cpp + riftbound_state.cpp sources MUST be compiled into
# this executable directly (not into a static lib) — OpenSpiel's
# OPEN_SPIEL_REGISTER_GAME static initializer would otherwise be stripped by
# the linker and LoadGame("riftbound") would return null at runtime.

add_executable(riftbound
    src/main.cpp
    src/agents/mcts_agent.cpp
    src/io/play_server.cpp
    src/io/play_index_html.cpp
    src/openspiel/riftbound_game.cpp
    src/openspiel/riftbound_state.cpp
    $<TARGET_OBJECTS:algorithms>
    $<TARGET_OBJECTS:game_transforms>
    $<TARGET_OBJECTS:utils>
)
target_link_libraries(riftbound PRIVATE
    riftbound_core
    open_spiel_core
    Boost::program_options
    Boost::system
    Boost::filesystem
    # abseil — OpenSpiel uses a directory-level link_libraries() that doesn't
    # propagate to consumers, so we replicate the list here. Mirrors the set
    # in src/openspiel/CMakeLists.txt.
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
# OpenSpiel sources include "open_spiel/spiel.h" — needs the parent of the
# open_spiel/ subdir on the include path.
target_include_directories(riftbound PRIVATE ${open_spiel_SOURCE_DIR})

# Config parsing (src/config/) moved to riftbound-trainer along with the
# training driver — engine has no current consumers. A future engine
# binary (riftbound_play web UI) that wants config-file input will
# define its own focused schema.
#
# LibTorch-dependent model code (V2/V3/V4/V5, ReBeL inference, training
# driver, probe binaries) lives in the riftbound-trainer sibling repo.

# ─── Tests ────────────────────────────────────────────────────────────────────

enable_testing()

# Test sources are globbed (CONFIGURE_DEPENDS) so new per-card test files under
# tests/ and tests/cards/ are picked up automatically — no manual list to edit.
file(GLOB_RECURSE RIFTBOUND_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
# Model-forward + CFR util tests live in riftbound-trainer.

add_executable(riftbound_tests ${RIFTBOUND_TEST_SOURCES})
# Per-card test fixture lives in tests/cards/. Bring tests/ onto the include
# path so #include "tests/cards/card_test_fixture.h" resolves.
target_include_directories(riftbound_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(riftbound_tests PRIVATE
    riftbound_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(riftbound_tests)
