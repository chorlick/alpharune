#!/usr/bin/env bash
# Checks scripts/ccache_pch_safe.sh against the two ways a cached precompiled
# header goes wrong, on scratch copies of this repository with a cache of
# their own. Nothing in your build directory or your ccache is touched.
#
#   scripts/check_ccache_pch_launcher.sh [--deps <dir>] [--work <dir>] [--launcher <path>]
#
#   --deps      a build directory's `_deps` (open_spiel-src, googletest-src), so
#               the scratch copies do not clone them. Default: build/_deps.
#   --work      where the copies go. Default: a new directory from mktemp. It
#               is left in place (logs for a failed step are in it); delete it
#               when you are done.
#   --launcher  the launcher to test. Default: this checkout's
#               scripts/ccache_pch_safe.sh. `ccache` shows the failures the
#               script exists to prevent (expect FAIL lines).
#
# Run it after upgrading ccache or the compiler, and after any change to the
# launcher. It compiles the PCH about ten times and a few small objects: a few
# minutes. Exit status 0 = every check passed.
#
# What is checked
#   A. Two copies sharing one cache change core/types.h differently and rebuild
#      the PCH: each copy's .gch must contain its own change and not the
#      other's.
#   B. One copy, the same unit each time:
#        1 first build                                   unit compiled
#        2 clean, nothing changed                        unit from the cache
#        3 a declaration added to core/types.h           unit compiled
#        4 a comment reworded in place, clean            unit compiled, and a
#          unit never built before compiles against that PCH (with a cached
#          PCH this is "redefinition of kInvalidId")
#        5 the original bytes again, clean               unit from the cache
set -u

here=$(cd "$(dirname "$0")/.." && pwd)
deps="$here/build/_deps"
work=""
launcher="$here/scripts/ccache_pch_safe.sh"
while [ $# -gt 0 ]; do
    case "$1" in
        --deps) deps=$2; shift 2 ;;
        --work) work=$2; shift 2 ;;
        --launcher) launcher=$2; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
[ -d "$deps/open_spiel-src" ] && [ -d "$deps/googletest-src" ] || {
    echo "no open_spiel-src / googletest-src under $deps (pass --deps <build>/_deps)" >&2; exit 2; }
[ -n "$work" ] || work=$(mktemp -d "${TMPDIR:-/tmp}/ccache_pch_check.XXXXXX")
mkdir -p "$work/cc" || exit 2
echo "launcher: $launcher"
echo "work dir: $work"

failures=0
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; failures=$((failures + 1)); }

GCH=build/CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch
UNIT=CMakeFiles/riftbound_core.dir/src/core/card_db.cpp.o
FRESH_UNIT=CMakeFiles/riftbound_core.dir/src/core/types.cpp.o
COMMENT="(the card class's def().id)"

# The copies are configured the way that lets ccache share between checkouts
# (CCACHE_BASEDIR + CCACHE_NOHASHDIR): that is the setup in which check A's
# failure was first seen.
copy() { # name
    mkdir -p "$work/$1" &&
    git -C "$here" archive HEAD | tar -x -C "$work/$1" &&
    ( cd "$work/$1" && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_DIR=$work/cc;CCACHE_BASEDIR=$work/$1;CCACHE_NOHASHDIR=1;CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime;$launcher" \
        -DFETCHCONTENT_SOURCE_DIR_OPEN_SPIEL="$deps/open_spiel-src" \
        -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$deps/googletest-src" > "$work/configure_$1.log" 2>&1 )
}
build() { # copy target log-name
    cmake --build "$work/$1/build" --target "$2" > "$work/$3.log" 2>&1
}
clean() { cmake --build "$work/$1/build" --target clean > /dev/null; }
counters() { # -> "hits misses"
    env CCACHE_DIR="$work/cc" ccache --print-stats | awk -F'\t' \
        '$1=="direct_cache_hit"||$1=="preprocessed_cache_hit"{h+=$2} $1=="cache_miss"{m=$2} END{print h+0, m+0}'
}
# Build $UNIT in copy A and say whether that one compile hit or missed.
unit_result() { # log-name -> prints hit|miss|error
    local before after
    # The PCH first, so that only the unit's own compile is counted.
    build A "CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch" "$1_pch" || { echo error; return; }
    before=$(counters)
    build A "$UNIT" "$1" || { echo error; return; }
    after=$(counters)
    if [ "${after% *}" -gt "${before% *}" ]; then echo hit
    elif [ "${after#* }" -gt "${before#* }" ]; then echo miss
    else echo error; fi
}
expect() { # description wanted got
    if [ "$2" = "$3" ]; then ok "$1 ($3)"; else fail "$1: expected $2, got $3"; fi
}

echo "A. two copies, one cache"
copy A && copy B || { echo "configure failed (see $work/configure_*.log)" >&2; exit 2; }
build A "CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch" a1 && build B "CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch" b1 ||
    { echo "the first PCH build failed (see $work/a1.log, b1.log)" >&2; exit 2; }
cp "$work/A/src/core/types.h" "$work/types_orig.h"
grep -qF "$COMMENT" "$work/types_orig.h" ||
    { echo "core/types.h no longer has the comment this script rewords: update COMMENT" >&2; exit 2; }
printf '\nnamespace riftbound { inline int pch_check_marker_a() { return 10; } }\n' >> "$work/A/src/core/types.h"
printf '\nnamespace riftbound { inline int pch_check_marker_b() { return 11; } }\n' >> "$work/B/src/core/types.h"
build A "CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch" a2
build B "CMakeFiles/riftbound_pch.dir/cmake_pch.hxx.gch" b2
if grep -q pch_check_marker_a "$work/A/$GCH"; then ok "copy A's PCH has copy A's change"; else fail "copy A's PCH lacks copy A's change"; fi
if grep -q pch_check_marker_b "$work/B/$GCH"; then ok "copy B's PCH has copy B's change"; else fail "copy B's PCH lacks copy B's change"; fi
if grep -q pch_check_marker_a "$work/B/$GCH"; then fail "copy B's PCH has copy A's change"; else ok "copy B's PCH does not have copy A's change"; fi

echo "B. one copy, what is taken from the cache"
cp "$work/types_orig.h" "$work/A/src/core/types.h"
clean A
expect "1 first build" miss "$(unit_result s1)"
clean A
expect "2 clean, nothing changed" hit "$(unit_result s2)"
printf '\nnamespace riftbound { inline int pch_check_marker_c() { return 12; } }\n' >> "$work/A/src/core/types.h"
expect "3 a declaration added" miss "$(unit_result s3)"
cp "$work/types_orig.h" "$work/A/src/core/types.h"
sed -i "s|$COMMENT|(reworded in place: same line count, other bytes)|" "$work/A/src/core/types.h"
clean A
expect "4 a comment reworded in place, clean" miss "$(unit_result s4)"
if build A "$FRESH_UNIT" s4b; then ok "4 a unit never built before compiles against that PCH"
else fail "4 a unit never built before does not compile: $(grep -m1 'error' "$work/s4b.log" | cut -c1-160)"; fi
cp "$work/types_orig.h" "$work/A/src/core/types.h"
clean A
expect "5 the original bytes again, clean" hit "$(unit_result s5)"

if [ "$failures" = 0 ]; then
    echo "all checks passed (work dir left at $work)"
    exit 0
fi
echo "$failures check(s) failed (logs in $work)"
exit 1
