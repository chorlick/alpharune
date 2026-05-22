cmake_minimum_required(VERSION 3.20)
project(riftbound-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RIFTBOUND_BUILD_OPENSPIEL "Build the OpenSpiel out-of-tree game wrapper (~adds OpenSpiel + abseil to _deps)" OFF)

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

# ONNX Runtime (for ML agent inference)
# Download pre-built binary for Linux x64
FetchContent_Declare(onnxruntime
    URL https://github.com/microsoft/onnxruntime/releases/download/v1.21.0/onnxruntime-linux-x64-1.21.0.tgz
)
FetchContent_MakeAvailable(onnxruntime)
set(ONNXRUNTIME_DIR ${onnxruntime_SOURCE_DIR})
set(ONNXRUNTIME_INCLUDE ${ONNXRUNTIME_DIR}/include)
set(ONNXRUNTIME_LIB ${ONNXRUNTIME_DIR}/lib/libonnxruntime.so)

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

# nlohmann_json: only call find_package if OpenSpiel didn't already provide it.
# (When RIFTBOUND_BUILD_OPENSPIEL=ON, OpenSpiel's bundled json subproject
#  defines both `nlohmann_json` and `nlohmann_json::nlohmann_json`.)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json REQUIRED)
endif()

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
    src/io/data_serializer.cpp
    src/io/binary_data_serializer.cpp
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
    src/agents/model_agent.cpp
    src/engine/game_runner.cpp
    src/engine/batch_runner.cpp
    src/engine/step_driver.cpp
    src/ml/feature_extractor.cpp
    src/openspiel/action_encoding.cpp
)

target_include_directories(riftbound_core PUBLIC src ${ONNXRUNTIME_INCLUDE})
target_link_libraries(riftbound_core PUBLIC
    Boost::boost
    Boost::system
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${ONNXRUNTIME_LIB}
)

# ─── Engine library ──────────────────────────────────────────────────────────
# (will grow as we add engine, rules, agents, io modules)

# add_library(riftbound_engine
#     src/engine/game_engine.cpp
#     ...
# )
# target_link_libraries(riftbound_engine PUBLIC riftbound_core)

# ─── Main executable ─────────────────────────────────────────────────────────

add_executable(riftbound src/main.cpp)
target_link_libraries(riftbound PRIVATE riftbound_core Boost::program_options)

# ─── Tests ────────────────────────────────────────────────────────────────────

enable_testing()

add_executable(riftbound_tests
    tests/test_types.cpp
    tests/test_card_db.cpp
    tests/test_deck_validator.cpp
    tests/test_chain.cpp
    tests/test_action_encoding.cpp
    tests/test_observation_tensor.cpp
)

target_link_libraries(riftbound_tests PRIVATE
    riftbound_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(riftbound_tests)
