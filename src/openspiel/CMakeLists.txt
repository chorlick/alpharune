# Riftbound OpenSpiel out-of-tree game wrapper.
#
# Built only when RIFTBOUND_BUILD_OPENSPIEL=ON in the parent CMakeLists.txt.
# The game registration (riftbound_game.cpp) MUST be compiled directly into
# the demo executable rather than into a static lib — otherwise the linker
# strips the TU and the OPEN_SPIEL_REGISTER_GAME static initializer never
# runs, leaving the game unfindable via LoadGame("riftbound").

add_executable(riftbound_openspiel_demo
    riftbound_openspiel_demo.cpp
    riftbound_game.cpp
    riftbound_state.cpp
)

target_link_libraries(riftbound_openspiel_demo PRIVATE
    riftbound_core
    open_spiel_core
    # OpenSpiel sets up abseil deps via a directory-level `link_libraries()`
    # in its CMakeLists, which doesn't propagate to consumers via
    # target_link_libraries. We replicate the list explicitly here.
    absl::algorithm
    absl::flags
    absl::flags_parse
    absl::flat_hash_map
    absl::optional
    absl::random_random
    absl::str_format
    absl::strings
    absl::time
)

# open_spiel_core only exports its own subdir as a PUBLIC include, but
# OpenSpiel source uses `#include "open_spiel/spiel.h"` — that resolution
# needs the parent of the open_spiel subdir on the include path. OpenSpiel
# does this with a directory-level `include_directories(..)` which doesn't
# propagate to downstream consumers, so we add it explicitly here.
target_include_directories(riftbound_openspiel_demo PRIVATE
    ${open_spiel_SOURCE_DIR}
)

# MCTS-vs-Random demo: drives OpenSpiel's MCTSBot from one seat and uniform
# random from the other. The decisive win rate validates that Clone() (replay
# based), bit-packed action encoding, and the information-set
# ObservationTensor all work end-to-end against a real algorithm — the
# Phase B definition-of-done check.
add_executable(riftbound_mcts_demo
    mcts_vs_random_demo.cpp
    riftbound_game.cpp
    riftbound_state.cpp
    # MCTSBot + RandomRolloutEvaluator live in OpenSpiel's `algorithms`
    # OBJECT library, which isn't exposed as a linkable target. Pull
    # the object files directly via TARGET_OBJECTS. `algorithms` itself
    # depends transitively on `game_transforms` (ConvertToTurnBased) and
    # `utils` (RandomMT), so we pull those too.
    $<TARGET_OBJECTS:algorithms>
    $<TARGET_OBJECTS:game_transforms>
    $<TARGET_OBJECTS:utils>
)
target_link_libraries(riftbound_mcts_demo PRIVATE
    riftbound_core open_spiel_core
    absl::algorithm absl::flags absl::flags_parse absl::flat_hash_map
    absl::optional absl::random_random absl::str_format absl::strings absl::time
)
target_include_directories(riftbound_mcts_demo PRIVATE
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
