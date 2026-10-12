#!/usr/bin/env bash
# Configure and build the engine binary for the test-decks stream (lane B, -j3).
set -euo pipefail
REPO=/home/pirateporkchop/code/riftbound-engine
WT="$REPO/.worktrees/test-decks"
cd "$WT"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRIFTBOUND_TEST_OPT_LEVEL=0 \
  -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_NAMESPACE=test-decks;CCACHE_SLOPPINESS=pch_defines,time_macros;$WT/scripts/ccache_pch_safe.sh" \
  -DFETCHCONTENT_SOURCE_DIR_OPEN_SPIEL="$REPO/build/_deps/open_spiel-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$REPO/build/_deps/googletest-src"
cmake --build build --target riftbound -j3
