cmake_minimum_required(VERSION 3.20)
project(riftbound-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Build parallelism cap ────────────────────────────────────────────────────
# At most RIFTBOUND_BUILD_JOBS compile and link jobs run at once, whatever -j
# the build is started with (`cmake --build build -j16` still runs 6). One
# Ninja job pool is shared by compiling, precompiling headers and linking. It
# is set here, before any target exists, so it also covers the FetchContent
# projects (OpenSpiel, abseil, GoogleTest). Ninja generators only; "0" = no cap.
set(RIFTBOUND_BUILD_JOBS "6" CACHE STRING
    "Maximum concurrent compile+link jobs (Ninja job pool); 0 = no cap")
if(NOT RIFTBOUND_BUILD_JOBS MATCHES "^[0-9]+$")
    message(FATAL_ERROR "RIFTBOUND_BUILD_JOBS must be a non-negative integer, got '${RIFTBOUND_BUILD_JOBS}'")
endif()
if(CMAKE_GENERATOR MATCHES "Ninja" AND RIFTBOUND_BUILD_JOBS GREATER 0)
    set_property(GLOBAL PROPERTY JOB_POOLS riftbound_jobs=${RIFTBOUND_BUILD_JOBS})
    set(CMAKE_JOB_POOL_COMPILE riftbound_jobs)
    set(CMAKE_JOB_POOL_PRECOMPILE_HEADER riftbound_jobs)
    set(CMAKE_JOB_POOL_LINK riftbound_jobs)
endif()

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

# ─── Warnings ─────────────────────────────────────────────────────────────────
# -Wall -Wextra on the engine's own targets (never on OpenSpiel, abseil or
# GoogleTest). Warnings are reported, not errors. The flags must be the same on
# riftbound_pch and on every target that reuses its PCH, so they are applied
# through this one function. A target that includes OpenSpiel gets its source
# root as a SYSTEM include directory as well, which keeps OpenSpiel's own
# header warnings out of the engine's build output.
# The board index of the object table (src/core/object_table.h) is rebuilt
# when an object's zone or location is assigned. With this ON every board
# query also recomputes the lists and compares them with the index, and a
# loop over a board range that moves an object is an error: a slow build for
# running the test suite against the index, not for games.
option(RIFTBOUND_CHECK_BOARD_INDEX "Verify the object table's board index on every query (slow)" OFF)
if(RIFTBOUND_CHECK_BOARD_INDEX)
    add_compile_definitions(RIFTBOUND_CHECK_BOARD_INDEX)
endif()

# Optimisation level for every target (engine, cards, tests, third-party),
# whatever the build type says. Empty keeps the build type's own level (-O3 for
# Release). "0" is for development: a full rebuild is mostly the optimiser's
# time, and the test suite does not need optimised code to be right. Timings,
# search agents and anything that ships are built with the default.
set(RIFTBOUND_OPT_LEVEL "" CACHE STRING
    "Optimisation level for all targets: 0, 1, 2, 3, g or s; empty = the build type's default")
set_property(CACHE RIFTBOUND_OPT_LEVEL PROPERTY STRINGS "" 0 1 2 3 g s)
if(NOT RIFTBOUND_OPT_LEVEL STREQUAL "")
    if(NOT RIFTBOUND_OPT_LEVEL MATCHES "^[0123gs]$")
        message(FATAL_ERROR "RIFTBOUND_OPT_LEVEL must be 0, 1, 2, 3, g or s (got '${RIFTBOUND_OPT_LEVEL}')")
    endif()
    # Appended after the build type's flags; the last -O on a command line wins.
    add_compile_options(-O${RIFTBOUND_OPT_LEVEL})
    message(STATUS "riftbound: every target is compiled with -O${RIFTBOUND_OPT_LEVEL}")
endif()

option(RIFTBOUND_WARNINGS "Compile engine targets with -Wall -Wextra" ON)
function(riftbound_enable_warnings)
    if(RIFTBOUND_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        foreach(_target IN LISTS ARGN)
            target_compile_options(${_target} PRIVATE -Wall -Wextra)
            get_target_property(_incs ${_target} INCLUDE_DIRECTORIES)
            if(open_spiel_SOURCE_DIR AND "${open_spiel_SOURCE_DIR}" IN_LIST _incs)
                target_include_directories(${_target} SYSTEM PRIVATE ${open_spiel_SOURCE_DIR})
            endif()
        endforeach()
    endif()
endfunction()

# OpenSpiel is mandatory — the riftbound binary's MCTS / ISMCTS agents
# (src/agents/mcts_agent.cpp) are the engine's first-class AI baselines and
# the integration surface that lets external tools plug their own agents into
# Riftbound games.
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
        # system-package step entirely. abseil and json are pinned to the tags
        # v1.6.14's install.sh uses; install.sh takes pybind11_json and DDS
        # from a moving branch, so those two are pinned to the commit that
        # branch pointed at when this was written (configure is reproducible).
        #
        # GIT_REF is a tag, or a full commit hash (fetched directly: a shallow
        # clone cannot name a commit with --branch).
        function(_riftbound_clone_dep TARGET_DIR GIT_URL GIT_REF)
            if(NOT EXISTS "${TARGET_DIR}")
                message(STATUS "  Cloning ${GIT_URL} @ ${GIT_REF}")
                if(GIT_REF MATCHES "^[0-9a-f]+$")
                    string(LENGTH "${GIT_REF}" _ref_len)
                else()
                    set(_ref_len 0)
                endif()
                if(_ref_len EQUAL 40)
                    execute_process(
                        COMMAND git init --quiet ${TARGET_DIR}
                        COMMAND_ERROR_IS_FATAL ANY
                        OUTPUT_QUIET ERROR_QUIET)
                    execute_process(
                        COMMAND git -C ${TARGET_DIR} fetch --quiet --depth 1 ${GIT_URL} ${GIT_REF}
                        RESULT_VARIABLE _rc
                        OUTPUT_QUIET ERROR_QUIET)
                    if(_rc EQUAL 0)
                        execute_process(
                            COMMAND git -C ${TARGET_DIR} -c advice.detachedHead=false
                                    checkout --quiet FETCH_HEAD
                            RESULT_VARIABLE _rc
                            OUTPUT_QUIET ERROR_QUIET)
                    endif()
                else()
                    execute_process(
                        COMMAND git clone --depth 1 --branch ${GIT_REF} ${GIT_URL} ${TARGET_DIR}
                        RESULT_VARIABLE _rc
                        OUTPUT_QUIET ERROR_QUIET
                    )
                endif()
                if(NOT _rc EQUAL 0)
                    # Leave nothing behind, or the next configure would take a
                    # half-made directory for a finished clone.
                    file(REMOVE_RECURSE "${TARGET_DIR}")
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
            d0bf434be9d287d73a963ff28745542daf02c08f)  # master, 2025-05-24
        # DDS (double-dummy-solver) is referenced unconditionally by
        # games/CMakeLists.txt because the bridge game can't be disabled
        # via a flag. Cloning it is enough — we never link the bridge
        # game target from our wrapper.
        _riftbound_clone_dep(
            ${open_spiel_SOURCE_DIR}/open_spiel/games/bridge/double_dummy_solver
            https://github.com/jblespiau/dds.git
            091ea94358a4016d4fb6069dea5c452cdc98d0bd)  # develop, 2020-10-30

        add_subdirectory(${open_spiel_SOURCE_DIR}/open_spiel ${open_spiel_BINARY_DIR})
    endif()

    # enable_testing() must come before this directory is added: a directory
    # only gets a CTestTestfile.cmake if testing was already on when it was
    # created. With it further down, add_test(riftbound_openspiel_tests) in
    # src/openspiel was silently dropped and `ctest` never ran that binary.
    enable_testing()
    add_subdirectory(src/openspiel)
    get_property(_riftbound_openspiel_targets DIRECTORY src/openspiel
        PROPERTY BUILDSYSTEM_TARGETS)
    riftbound_enable_warnings(${_riftbound_openspiel_targets})
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

# Libraries every engine translation unit compiles against. One list, so the
# card groups below get exactly the same compile definitions as riftbound_core
# (required for them to share one precompiled header).
set(RIFTBOUND_CORE_LIBS
    Boost::boost
    Boost::system
    Boost::context   # userspace fibers for the engine's step machine
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ─── Precompiled header ───────────────────────────────────────────────────────
# Every card is its own source file and each one parses the same engine
# headers. src/pch.h holds those headers; one small target owns the compiled
# PCH and riftbound_core plus every card group reuse it, so a core-header
# change rebuilds a single PCH.
#
# With ccache as the compiler launcher, GCC needs -fpch-preprocess and ccache
# needs `sloppiness = pch_defines,time_macros` or it will not cache
# compilations that use the PCH. The launcher must be scripts/ccache_pch_safe.sh
# rather than `ccache` itself:
#   -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_SLOPPINESS=pch_defines,time_macros;<checkout>/scripts/ccache_pch_safe.sh"
# The script never sends a PCH compile to ccache: a PCH is always compiled,
# never cached. It writes a checksum of what the PCH was made from next to it
# (<pch>.gch.sum) and the units that use the PCH are cached keyed on that
# (ccache's pch_external_checksum). Plain ccache gets a cached PCH wrong in
# two ways (the script's comments have the mechanisms; both reproduced):
#  - a PCH *rebuild* is keyed on the stale .gch still in the build directory,
#    not on the headers, so it can return a .gch built from other headers;
#  - from a clean build directory, a hit on the preprocessed text (which has
#    no comments) returns the previous .gch after a comment was reworded in
#    place; its `#pragma once` records no longer match the header, which is
#    then included twice ("redefinition of ...").
option(RIFTBOUND_USE_PCH "Precompile the common engine headers (src/pch.h)" ON)
set(RIFTBOUND_PCH_CCACHE_FLAG "")
if(RIFTBOUND_USE_PCH AND CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache"
        AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(RIFTBOUND_PCH_CCACHE_FLAG -fpch-preprocess)
    if(NOT CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache_pch_safe")
        message(WARNING
            "CMAKE_CXX_COMPILER_LAUNCHER runs ccache directly while precompiled "
            "headers are on, so ccache caches the PCH itself. After a header in "
            "the PCH changes it can restore a .gch built from different header "
            "contents, or (after a comment-only edit) one whose `#pragma once` "
            "records no longer match, which fails with redefinition errors. A "
            "clean build does not avoid this. Use scripts/ccache_pch_safe.sh as "
            "the launcher: it always compiles a PCH and caches the units that "
            "use it, keyed on a checksum of the PCH's inputs (see CLAUDE.md, "
            "\"ccache\").")
    endif()
endif()
if(RIFTBOUND_USE_PCH)
    add_library(riftbound_pch OBJECT src/pch_anchor.cpp)
    target_include_directories(riftbound_pch PRIVATE src)
    target_link_libraries(riftbound_pch PRIVATE ${RIFTBOUND_CORE_LIBS})
    target_compile_options(riftbound_pch PRIVATE ${RIFTBOUND_PCH_CCACHE_FLAG})
    riftbound_enable_warnings(riftbound_pch)
    target_precompile_headers(riftbound_pch PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h)
endif()
function(riftbound_use_pch TARGET)
    if(RIFTBOUND_USE_PCH)
        target_compile_options(${TARGET} PRIVATE ${RIFTBOUND_PCH_CCACHE_FLAG})
        target_precompile_headers(${TARGET} REUSE_FROM riftbound_pch)
    endif()
endfunction()

# ─── Card groups ──────────────────────────────────────────────────────────────
# One source file per card under src/cards/<type>/ (955 of the ~1,060 engine
# sources). Each directory is an OBJECT library folded back into riftbound_core:
# cards_init.cpp calls into the cards and the cards call back into card.cpp and
# the executor, so separate static libraries would link circularly. Consumers
# of riftbound_core are unaffected.
#
# RIFTBOUND_UNITY_CARDS compiles each group as a few combined sources
# (RIFTBOUND_CARD_UNITY_BATCH card files each) instead of one compile per
# card. Cards rarely change once written, so the usual unity trade-off (edit
# one file, recompile its batch) is seldom paid, while a core-header change
# recompiles ~60 units instead of ~955. Turn it OFF for a build directory that
# should give clangd a per-file compile command.
#
# Rule for card authors: file-scope names in a card file must be unique within
# its group (unity merges the files' anonymous namespaces). A reprint's class
# carries its card id (KarmaChanneler_235 / _548); shared helpers live in
# src/cards/card_helpers.h, not copied between cards.
option(RIFTBOUND_UNITY_CARDS "Unity-build each card group" ON)
set(RIFTBOUND_CARD_UNITY_BATCH 16 CACHE STRING
    "Card source files per unity compile (with RIFTBOUND_UNITY_CARDS)")

set(RIFTBOUND_CARD_GROUPS units spells gear legends battlefields runes)
set(RIFTBOUND_CARD_OBJECTS "")
foreach(_group IN LISTS RIFTBOUND_CARD_GROUPS)
    # CONFIGURE_DEPENDS re-globs at build time so a newly added card is picked
    # up without a manual reconfigure.
    file(GLOB _card_srcs CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/cards/${_group}/*.cpp)
    add_library(riftbound_cards_${_group} OBJECT ${_card_srcs})
    target_include_directories(riftbound_cards_${_group} PRIVATE src)
    target_link_libraries(riftbound_cards_${_group} PRIVATE ${RIFTBOUND_CORE_LIBS})
    riftbound_use_pch(riftbound_cards_${_group})
    riftbound_enable_warnings(riftbound_cards_${_group})
    if(RIFTBOUND_UNITY_CARDS)
        set_target_properties(riftbound_cards_${_group} PROPERTIES
            UNITY_BUILD ON
            UNITY_BUILD_BATCH_SIZE ${RIFTBOUND_CARD_UNITY_BATCH})
    endif()
    list(APPEND RIFTBOUND_CARD_OBJECTS $<TARGET_OBJECTS:riftbound_cards_${_group}>)
endforeach()

# ─── Core library ─────────────────────────────────────────────────────────────

add_library(riftbound_core
    src/core/types.cpp
    src/core/intent.cpp
    src/core/game_state.cpp
    src/core/card_db.cpp
    src/core/events.cpp
    src/core/keyed_slots.cpp
    src/rules/deck_validator.cpp
    src/rules/random_deck.cpp
    src/rules/deck_model.cpp
    src/engine/game_engine.cpp
    src/engine/chain_manager.cpp
    src/engine/effect_executor.cpp
    src/engine/trigger_manager.cpp
    src/engine/keyword_abilities.cpp
    src/engine/knowledge_tracker.cpp
    src/engine/deck_ops.cpp
    src/engine/prompt_description.cpp
    src/engine/observer_view.cpp
    src/io/state_renderer.cpp
    src/io/replay_writer.cpp
    src/io/state_editor.cpp
    # The web UI server. In the library (not only in the `riftbound` binary)
    # so tests link it instead of #including the .cpp files.
    src/io/play_server.cpp
    src/io/play_index_html.cpp
    src/cards/card.cpp
    src/cards/card_registry.cpp
    src/cards/cards_init.cpp
    ${RIFTBOUND_CARD_OBJECTS}
    src/engine/game_runner.cpp
    src/engine/batch_runner.cpp
    src/engine/step_driver.cpp
    src/agents/scripted_agent.cpp
    src/agents/determinizer.cpp
    src/ml/feature_extractor.cpp
    src/ml/cfr_util.cpp
    src/openspiel/action_vocab.cpp
)

target_include_directories(riftbound_core PUBLIC src)
riftbound_use_pch(riftbound_core)
riftbound_enable_warnings(riftbound_core)
target_link_libraries(riftbound_core PUBLIC ${RIFTBOUND_CORE_LIBS})

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
riftbound_enable_warnings(riftbound)

# tools/search_strength.cpp: a search agent against another over the deck
# lists of a directory, win rate with a 95% interval and seconds per decision
# (batch 5 item T3). Not part of `all`:
#   cmake --build build --target search_strength
add_executable(search_strength EXCLUDE_FROM_ALL
    tools/search_strength.cpp
    src/agents/mcts_agent.cpp
    src/openspiel/riftbound_game.cpp
    src/openspiel/riftbound_state.cpp
    $<TARGET_OBJECTS:algorithms>
    $<TARGET_OBJECTS:game_transforms>
    $<TARGET_OBJECTS:utils>
)
target_link_libraries(search_strength PRIVATE
    riftbound_core open_spiel_core Threads::Threads
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(search_strength PRIVATE ${open_spiel_SOURCE_DIR})
riftbound_enable_warnings(search_strength)

# tools/collision_census.cpp: plays random games and counts legal intents that
# share an action id. The trainer repo builds it against its own decks; it is
# a target here too (not part of `all`: `cmake --build build --target
# collision_census`) so a change to openspiel/action_vocab.h cannot leave it
# uncompiled.
add_executable(collision_census EXCLUDE_FROM_ALL tools/collision_census.cpp)
target_link_libraries(collision_census PRIVATE riftbound_core)
riftbound_enable_warnings(collision_census)

# tools/deck_fuzz.cpp: whole-pool fuzz. Random agents play random
# constructed-legal decks (rules/random_deck.h) or the lists of a directory;
# every failing game is recorded with the seeds that replay it alone, and
# per-card coverage is counted from engine events. Not part of `all`:
# `cmake --build build --target deck_fuzz`. The game loop it shares with the
# suite (tools/fuzz/fuzz_game.h) is exercised by tests/test_deck_fuzz.cpp, so
# it cannot rot uncompiled. How to run the large one: CLAUDE.md, Testing.
add_executable(deck_fuzz EXCLUDE_FROM_ALL tools/deck_fuzz.cpp)
target_link_libraries(deck_fuzz PRIVATE riftbound_core Threads::Threads)
target_include_directories(deck_fuzz PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
riftbound_enable_warnings(deck_fuzz)

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

# RIFTBOUND_UNITY_TESTS compiles the test sources as combined units of
# RIFTBOUND_TEST_UNITY_BATCH files. A test file costs 10-17 s on its own, nearly
# all of it instantiating the same GoogleTest and engine templates as its
# neighbours, so a core-header change rebuilds the suite several times faster.
# The trade-off is the usual one: editing one test file recompiles its batch
# (turn the option OFF in a build directory used for iterating on one file, or
# for clangd).
#
# Rule for test authors, the same as for card files: file-scope names must be
# unique across tests/ — unity merges every file's anonymous namespace and
# `using namespace`. Put a file's helpers, test cards and fixtures in a
# namespace of its own (see tests/test_review_fix_integrity.cpp) or give them
# file-specific names. A clash is a compile error (redefinition / ambiguous
# call) in the batch that pairs the two files, never a silent pick; a file
# that genuinely cannot share a unit goes in RIFTBOUND_TEST_UNITY_EXCLUDE.
option(RIFTBOUND_UNITY_TESTS "Unity-build the riftbound_tests sources" ON)
set(RIFTBOUND_TEST_UNITY_BATCH 8 CACHE STRING
    "Test source files per unity compile (with RIFTBOUND_UNITY_TESTS)")
# Compiled on their own even with RIFTBOUND_UNITY_TESTS (paths relative to the
# repository root). Empty: no test file needs it at present.
set(RIFTBOUND_TEST_UNITY_EXCLUDE
)
if(RIFTBOUND_UNITY_TESTS)
    set_target_properties(riftbound_tests PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE ${RIFTBOUND_TEST_UNITY_BATCH})
    foreach(_src IN LISTS RIFTBOUND_TEST_UNITY_EXCLUDE)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
            message(FATAL_ERROR "RIFTBOUND_TEST_UNITY_EXCLUDE names a missing file: ${_src}")
        endif()
        set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${_src}"
            PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    endforeach()
endif()
# GoogleTest's assertion macros expand __FILE__, and the test PCH and the unity
# units include their sources by absolute path. Mapping the source root away
# keeps the checkout's location out of the preprocessed text, so ccache can
# share test objects between worktrees (and failure messages print paths
# relative to the repository root).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(riftbound_tests PRIVATE
        "-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=")
endif()
# Per-card test fixture lives in tests/cards/. Bring tests/ onto the include
# path so #include "tests/cards/card_test_fixture.h" resolves.
target_include_directories(riftbound_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
riftbound_enable_warnings(riftbound_tests)

target_link_libraries(riftbound_tests PRIVATE
    riftbound_core
    GTest::gtest_main
)

# The tests parse the same engine headers plus GoogleTest and the shared card
# fixture; they get their own PCH (different include paths and definitions
# from riftbound_core, so the library's cannot be reused).
# The test sources alone, engine and cards left at the build type's level:
# measured 2026-10-11, test code is half of a full rebuild at -O3 and its
# optimisation buys nothing (the suite's time is spent in the engine).
set(RIFTBOUND_TEST_OPT_LEVEL "" CACHE STRING
    "Optimisation level for the riftbound_tests sources only: 0, 1, 2, 3, g or s; empty = as the rest")
set_property(CACHE RIFTBOUND_TEST_OPT_LEVEL PROPERTY STRINGS "" 0 1 2 3 g s)
if(NOT RIFTBOUND_TEST_OPT_LEVEL STREQUAL "")
    if(NOT RIFTBOUND_TEST_OPT_LEVEL MATCHES "^[0123gs]$")
        message(FATAL_ERROR "RIFTBOUND_TEST_OPT_LEVEL must be 0, 1, 2, 3, g or s (got '${RIFTBOUND_TEST_OPT_LEVEL}')")
    endif()
    target_compile_options(riftbound_tests PRIVATE -O${RIFTBOUND_TEST_OPT_LEVEL})
    message(STATUS "riftbound: riftbound_tests sources are compiled with -O${RIFTBOUND_TEST_OPT_LEVEL}")
endif()

if(RIFTBOUND_USE_PCH)
    target_precompile_headers(riftbound_tests PRIVATE
        <gtest/gtest.h>
        ${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/cards/card_test_fixture.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/game_engine.h)
    if(CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(riftbound_tests PRIVATE -fpch-preprocess)
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(riftbound_tests)

# Id kinds do not mix (plan item 5.5). Each case of
# tests/compile_fail/strong_id_mix.cc is compiled on its own, syntax only:
# case 0 must compile, every other case must be rejected by the compiler.
# RIFTBOUND_STRONG_ID_FAIL_CASES is the number of the last case.
set(RIFTBOUND_STRONG_ID_FAIL_CASES 25)
foreach(_case RANGE 0 ${RIFTBOUND_STRONG_ID_FAIL_CASES})
    add_test(NAME riftbound_strong_id_case_${_case}
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only
                -I${PROJECT_SOURCE_DIR}/src -DSTRONG_ID_CASE=${_case}
                ${PROJECT_SOURCE_DIR}/tests/compile_fail/strong_id_mix.cc)
    if(NOT _case EQUAL 0)
        # "error:" from the compiler and no "#error": the case was reached
        # and its own line was refused.
        set_tests_properties(riftbound_strong_id_case_${_case} PROPERTIES
            PASS_REGULAR_EXPRESSION "error: "
            FAIL_REGULAR_EXPRESSION "#error")
    endif()
endforeach()

# One card table per process (plan item 9.E3): a game with a search agent in
# the `riftbound` binary must build the registry + database once. main.cpp
# used to build its own next to openspiel::sharedCards(). `--debug` prints the
# count (CardDB::tablesBuilt).
add_test(NAME riftbound_one_card_table
    COMMAND riftbound --deck1 decks/yi_rebuild.txt --deck2 decks/fiora.txt
            --agent1 mcts:sims=2 --agent2 ismcts:sims=2 --seed 7 --debug
            --render-html off --web off
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(riftbound_one_card_table PROPERTIES
    ENVIRONMENT "RIFTBOUND_ROOT=${PROJECT_SOURCE_DIR}"
    PASS_REGULAR_EXPRESSION "Card tables built in this process: 1\n"
    TIMEOUT 300)

# tools/balance.cpp: first-player edge and deck balance (testing phase item
# T5). One agent kind in both seats over every ordered pairing of the deck
# lists of a directory, or over random constructed decks; one CSV line per
# game. Not part of `all`: `cmake --build build --target balance`.
add_executable(balance EXCLUDE_FROM_ALL
    tools/balance.cpp
    src/agents/mcts_agent.cpp
    src/openspiel/riftbound_game.cpp
    src/openspiel/riftbound_state.cpp
    $<TARGET_OBJECTS:algorithms>
    $<TARGET_OBJECTS:game_transforms>
    $<TARGET_OBJECTS:utils>
)
target_link_libraries(balance PRIVATE
    riftbound_core open_spiel_core Threads::Threads
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(balance PRIVATE ${open_spiel_SOURCE_DIR})
riftbound_enable_warnings(balance)
