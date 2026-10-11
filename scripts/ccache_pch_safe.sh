#!/usr/bin/env bash
# Compiler launcher: ccache, made safe for precompiled headers.
#
#   -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_SLOPPINESS=pch_defines,time_macros;$PWD/scripts/ccache_pch_safe.sh"
#
# What it does
#   * A compile that CREATES a precompiled header (`-x c++-header`) never goes
#     to ccache. The old .gch and its .gch.sum are deleted, the compiler runs
#     directly, and on success a checksum file `<pch>.gch.sum` is written.
#   * Every other compile goes to ccache with `pch_external_checksum` on: for
#     a unit that uses the PCH, ccache keys on the .gch.sum instead of on the
#     .gch's bytes.
#
# What the .gch.sum is
#   A SHA-256 over everything that determines the PCH: the compiler's
#   `--version` text, the whole command line of the PCH compile, and the name
#   and RAW BYTES of every file the compiler read to make it (the list is the
#   dependency file the same compile writes, -MF). Raw bytes on purpose: a
#   comment-only edit moves line numbers, which reach object code through
#   __LINE__, assert() and debug info, so it must change the key. Two PCHs
#   with the same .sum were built by the same compiler, with the same flags,
#   from byte-identical files at the same paths.
#
# Why a PCH is never cached (two ways plain ccache gets it wrong)
#   1. CMake rebuilds a PCH with the OLD .gch still in place:
#        c++ ... -x c++-header -include <dir>/cmake_pch.hxx -o <dir>/cmake_pch.hxx.gch ...
#      ccache sees `-include X` next to an existing `X.gch`, takes the compile
#      for one that USES that header and keys the result on the old .gch, not
#      on the headers. Two rebuilds that start from the same .gch share one
#      result: the second gets a .gch built from the first one's headers.
#      Reproduced (two copies of this repository, one cache).
#   2. From a clean build directory there is no old .gch, so ccache keys the
#      PCH compile normally and can hit on the PREPROCESSED text, which has no
#      comments. After a comment-only edit to a header it hands back the .gch
#      made from the previous version of that file. GCC records every
#      `#pragma once` header's size and time in the .gch; the record no longer
#      matches, so a unit that includes the header gets it a second time
#      ("redefinition of ..."). Reproduced with a comment reworded in place
#      (same line count: a comment that adds a line changes the preprocessed
#      text and misses). The error shows at the first unit that is really
#      compiled; units that are cache hits themselves hide it.
#
# Why the .gch.sum (cost)
#   GCC does not write byte-identical .gch files for identical inputs, so with
#   ccache keying on the .gch itself every PCH rebuild made every unit that
#   uses it a cache miss: a no-change clean rebuild of riftbound_tests was
#   2 hits in 108 compiles and 222 s at -j3. Keyed on the .sum the same
#   rebuild was 109 hits in 109 and 14 s (2026-10-11).

out=""
depfile=""
is_pch=0
prev=""
for arg in "$@"; do
    if [ "$prev" = "-o" ]; then out=$arg; fi
    if [ "$prev" = "-MF" ]; then depfile=$arg; fi
    if [ "$prev" = "-x" ]; then
        case "$arg" in c++-header|c-header) is_pch=1 ;; esac
    fi
    prev=$arg
done

if [ "$is_pch" != 1 ]; then
    export CCACHE_PCH_EXTSUM=1
    exec ccache "$@"
fi

# ── Creating a PCH: compile it for real, then describe what it was made from.
if [ -n "$out" ]; then
    rm -f -- "$out" "$out.sum"
fi
"$@"
rc=$?
if [ "$rc" != 0 ] || [ -z "$out" ] || [ ! -f "$out" ]; then
    exit "$rc"
fi

# Without the dependency list there is nothing trustworthy to hash: leave no
# .sum, and ccache falls back to hashing the .gch itself (correct, only slow).
if [ -z "$depfile" ] || [ ! -f "$depfile" ]; then
    exit 0
fi

sum_tmp="$out.sum.tmp.$$"
{
    "$1" --version 2>&1
    printf '%s\0' "$@"
    # The depfile is Make syntax: "target: dep dep \" continued over lines.
    # Drop the target and the continuations, one path per line, sorted so the
    # checksum does not depend on the order the compiler listed them in.
    sed -e 's/\\$//' -e '1s/^[^:]*://' "$depfile" | tr -s ' \t' '\n\n' | grep -v '^$' | LC_ALL=C sort -u |
    while IFS= read -r dep; do
        # A listed file that cannot be read makes the checksum unusable.
        sha256sum -- "$dep" || echo "UNREADABLE $dep $$ $(date +%s%N)"
    done
} | sha256sum > "$sum_tmp" && mv -f -- "$sum_tmp" "$out.sum"
exit 0
