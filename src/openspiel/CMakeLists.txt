# Riftbound OpenSpiel out-of-tree game wrapper.
#
# Built only when RIFTBOUND_BUILD_OPENSPIEL=ON in the parent CMakeLists.txt.
# The game registration (riftbound_game.cpp) MUST be compiled directly into
# each executable rather than into a static lib — otherwise the linker
# strips the TU and the OPEN_SPIEL_REGISTER_GAME static initializer never
# runs, leaving the game unfindable via LoadGame("riftbound").

# Unified match runner. Supersedes the legacy `openspiel_rollout` (random
# vs random) and `openspiel_mcts` (MCTS vs random) executables — a single
# binary with --agent1/--agent2 flags covers both. Future neural agents
# (LibTorch when C++ AlphaZero training lands) will be added here.
# Pulls in OpenSpiel's `algorithms` + `game_transforms` + `utils` OBJECT
# libraries via $<TARGET_OBJECTS:..> so MCTSBot links (those targets
# aren't directly linkable; their object files must be pulled in by name).
add_executable(riftbound_openspiel
    openspiel_match.cpp
    riftbound_game.cpp
    riftbound_state.cpp
    $<TARGET_OBJECTS:algorithms>
    $<TARGET_OBJECTS:game_transforms>
    $<TARGET_OBJECTS:utils>
)
target_link_libraries(riftbound_openspiel PRIVATE
    riftbound_core open_spiel_core
    # OpenSpiel sets up abseil deps via a directory-level `link_libraries()`
    # in its CMakeLists, which doesn't propagate to consumers via
    # target_link_libraries. We replicate the list explicitly here.
    Boost::program_options
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
# Model-based agents (model:/rebel:) live in the riftbound-trainer repo.
# This binary supports only random/mcts/ismcts.
target_include_directories(riftbound_openspiel PRIVATE
    # open_spiel_core only exports its own subdir as a PUBLIC include, but
    # OpenSpiel source uses `#include "open_spiel/spiel.h"` — that
    # resolution needs the parent of the open_spiel subdir on the include
    # path. OpenSpiel does this with a directory-level include_directories
    # which doesn't propagate to downstream consumers, so we add it here.
    ${open_spiel_SOURCE_DIR}
)

# Clone-equivalence test (Phase 11 C-1 DoD #5). Standalone executable —
# clones a RiftboundState mid-game, drives original + clone forward with
# the same action sequence, asserts terminal/Returns parity. Validates
# correctness of Clone(); throughput is a separate DoD.
add_executable(riftbound_clone_equiv_test
    clone_equivalence_test.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)
target_link_libraries(riftbound_clone_equiv_test PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_clone_equiv_test PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# Clone microbench. Times N Clone() calls at a checkpoint without advancing
# any clone — simulates MCTS leaves that get cloned but not descended into.
# Lazy-clone target: 100 clones at decision 100 < 100 ms total.
add_executable(riftbound_clone_microbench
    clone_microbench.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)
target_link_libraries(riftbound_clone_microbench PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_clone_microbench PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# Clone+advance microbench. Distinct from clone_microbench — measures the
# Clone()+first-ApplyAction pair, which is what MCTS/ISMCTS actually pays
# per expanded tree node. Surfaces the hidden ensureLive() / engine
# spawn / replay cost that pure-snapshot benches don't.
add_executable(riftbound_clone_advance_microbench
    clone_advance_microbench.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)
target_link_libraries(riftbound_clone_advance_microbench PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_clone_advance_microbench PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# Clone-cost breakdown — A/B compares full Clone() vs raw GameState copy
# to surface how much of the Clone time is wrapper overhead vs the
# GameState deep-copy floor.
add_executable(riftbound_clone_breakdown
    clone_breakdown.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)
target_link_libraries(riftbound_clone_breakdown PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_clone_breakdown PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# parity_baseline: runs GameEngine + RandomAgent without the OpenSpiel wrapper.
# Diagnostic — confirms whether the engine itself produces BatchRunner-like
# results when invoked through the same RiftboundGame resource container.
add_executable(riftbound_parity_baseline
    parity_baseline.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)
target_link_libraries(riftbound_parity_baseline PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_parity_baseline PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# Kuhn Poker CFR validation moved to riftbound-trainer alongside cfr_tabular.
