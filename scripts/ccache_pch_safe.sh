#!/usr/bin/env bash
# Compiler launcher: ccache, made safe for precompiled headers.
#
#   -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_SLOPPINESS=pch_defines,time_macros;$PWD/scripts/ccache_pch_safe.sh"
#
# Why plain `ccache` is not safe here. CMake creates a PCH with
#     c++ ... -x c++-header -include <dir>/cmake_pch.hxx -o <dir>/cmake_pch.hxx.gch -c cmake_pch.hxx.cxx
# and when a header changes, Ninja reruns that command with the OLD .gch still
# in place. ccache sees `-include X` next to an existing `X.gch`, concludes the
# compile USES that precompiled header, and keys the result on the old .gch's
# bytes instead of on the headers. Two rebuilds that start from the same .gch
# (two worktrees that share a cache, or one tree that got the same .gch back
# from the cache after a clean build) then share one result: the second gets a
# .gch built from the first one's headers. GCC accepts it (-Winvalid-pch does
# not check header contents) and every unit using the PCH is compiled against
# the wrong declarations.
#
# The fix: delete the stale .gch before a PCH is created. Everything that is
# not a PCH goes to ccache as is.
out=""
is_pch=0
prev=""
for arg in "$@"; do
    if [ "$prev" = "-o" ]; then out=$arg; fi
    if [ "$prev" = "-x" ]; then
        case "$arg" in c++-header|c-header) is_pch=1 ;; esac
    fi
    prev=$arg
done
#
# A PCH is also never TAKEN from the cache. ccache can hit on the preprocessed
# text, which ignores comments: after a comment-only edit to a header it handed
# back a .gch made from the previous version of that file. GCC records each
# `#pragma once` header's size and time in the .gch, the record no longer
# matched the file, and the header was included a second time ("redefinition
# of kInvalidId" in every unit). So the PCH itself is always compiled; the
# units that use it are still cached, keyed on the .gch they were built with.
if [ "$is_pch" = 1 ]; then
    if [ -n "$out" ] && [ -f "$out" ]; then
        rm -f -- "$out"
    fi
    exec "$@"
fi
exec ccache "$@"
