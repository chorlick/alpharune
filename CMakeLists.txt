cmake_minimum_required(VERSION 3.20)
project(riftbound-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Dependencies ─────────────────────────────────────────────────────────────

find_package(Boost REQUIRED COMPONENTS program_options filesystem system)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# Google Test
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
    src/ml/feature_extractor.cpp
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
)

target_link_libraries(riftbound_tests PRIVATE
    riftbound_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(riftbound_tests)
