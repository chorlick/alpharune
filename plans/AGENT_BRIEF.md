# Brief for stream agents

You are one stream in an orchestrated fix of every finding from the 2026-10
engine code review. Read this whole file, then the plan files your task names.
The orchestrator's rules are in [README.md](README.md); you do not need them.

## The project in one paragraph

C++20 engine simulating 1v1 Riftbound TCG matches, used by MCTS agents and a
trainer in a sibling repo. Read `CLAUDE.md` at the repo root before touching
code: it has the architecture, the conventions and a list of pitfalls. The
rules text is `rules/core-rules.md` (grep by keyword or CR number). The core
principle: **card behaviour lives in the card's class, the engine only
dispatches**. Never add card-name or `ability_text` matching to the engine.

## Where the plan items are

Open items are in `plans/remaining/<file>`, closed ones in `plans/completed/<file>`
(same file names and sections; card batches under `cards/`). When you close an
item, tick it, write the outcome under it and MOVE the item from the
`remaining/` file to the same section of the `completed/` file. A new finding
is reported in your report, not written into a plan file, unless your task says
otherwise. A plan file named anywhere without a directory means "under
`completed/` or `remaining/`".

## Where you work

- Your worktree and branch are given in your task (`.worktrees/<stream>`,
  branch `fix/<stream>`). **Work only inside that directory.** Never edit,
  build or run git commands in the main checkout
  (`/home/pirateporkchop/code/riftbound-engine` itself): another session is
  working there.
- You own the files and plan items listed in your task. Do not edit files owned
  by another stream. If a fix needs a change outside your files:
  - a small, self-contained change: make it in a **separate commit** whose
    subject starts with `shared:` and mention it in your report;
  - anything larger: do not make it. Report it under "Needs another stream".

## The plans are stale by design — re-verify first

The review was written against commit `b05e51d`. Master has moved since, and
earlier stages of this fix have changed the code again. So for every item:

1. Find the code by symbol or text, not by line number.
2. Confirm the defect still exists by reading the current code. Items marked
   `[plausible]` or "not confirmed" have **not** been proven: prove or disprove
   them before changing anything.
3. If it is already fixed, obsolete, or was never a defect, do not invent a
   change. Close it as such (see "Closing an item").
4. Check the card text in `def()` and the rules before deciding what "correct"
   is. If the plan's proposed fix is wrong, do the right thing and say why.

Fix directions in the plans are suggestions from the reviewer, not decisions.

## How to fix

- Fix the root cause. Do not special-case a card in the engine, and do not
  change engine behaviour just to make a test pass.
- Match the surrounding code: naming, comment density, idiom. Helpers shared by
  more than one card go in `src/cards/card_helpers.h`, never in a per-set file.
  File-scope names in a card file must be unique within its directory (unity
  builds); a reprint's class carries its card id.
- Reprints: when you fix a card, check whether another id has the same name
  and fix both.
- Cards must not act on an object just because `objectExists(id)` is true: that
  holds for every id ever created. Check it is on board / in the zone the text
  requires.
- Choices at finalization: follow the contract in `CLAUDE.md` exactly
  (`finalizingChoices`, `costDueNow`, `costAlreadyPaid`).
- Do not change `kStateFeatureDim`, `kCardVocabSize`, the feature layout or the
  action vocabulary unless your task says you are the checkpoint stream:
  changing them invalidates trainer checkpoints.
- No drive-by refactors, no reformatting, no unrelated cleanups.

## Tests

- Every behavioural fix gets a regression test, unless the fix is purely data
  (a keyword, tag or number in `def()`), where one assertion on the def is
  enough.
- Tests must drive the **real engine path**: play the card, let the trigger go
  through `TriggerManager` and the chain, and assert on game state. A test that
  calls `onTrigger` / `onResolve` directly with hand-fed targets does not show
  the card works (that is how 26 broken cards passed their tests). Use the
  fixture in `tests/cards/card_test_fixture.h`; if it cannot express what you
  need, say so in your report rather than bypassing the engine.
- Existing tests that encode the wrong behaviour should be corrected, with a
  one-line comment citing the card text or CR number. Never delete or weaken a
  test to get green.

## Building — two lanes, one build per lane

The machine has 8 cores and other work is running. Builds run in two lanes of
`-j3` each (user decision, 2026-10-11; before that it was one lane at `-j6`).
Your spawn prompt names your lane: lane A locks `.worktrees/.build.lock`, lane
B locks `.worktrees/.build-b.lock`. Take only your own lane's lock, for every
configure, build and test run, and never build with more than `-j3`. The
commands below show lane A; lane B substitutes its lock file.

```bash
REPO=/home/pirateporkchop/code/riftbound-engine
cd "$REPO/.worktrees/<stream>"
# configure once (reuses the downloaded dependency sources). The ccache
# namespace keeps your cache private to your stream: a cache shared between
# worktrees restored precompiled headers built against another stream's headers.
flock "$REPO/.worktrees/.build.lock" cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DRIFTBOUND_TEST_OPT_LEVEL=0 \
  -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_NAMESPACE=<stream>;CCACHE_SLOPPINESS=pch_defines,time_macros;$REPO/.worktrees/<stream>/scripts/ccache_pch_safe.sh" \
  -DFETCHCONTENT_SOURCE_DIR_OPEN_SPIEL="$REPO/build/_deps/open_spiel-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$REPO/build/_deps/googletest-src"
# build + test
flock "$REPO/.worktrees/.build.lock" cmake --build build --target riftbound_tests -j3
flock "$REPO/.worktrees/.build.lock" env RIFTBOUND_ROOT=. ./build/riftbound_tests --gtest_brief=1
```

`-DRIFTBOUND_TEST_OPT_LEVEL=0` compiles the test sources without optimisation
and leaves the engine and cards at `-O3` (user decision, 2026-10-11: test code
is half of a full rebuild and its optimisation buys nothing). The engine binary
is the optimised one, so timings taken with it are valid. Do not configure with
`-DRIFTBOUND_OPT_LEVEL=0` (everything unoptimised): the suite then runs three
times slower and timings mean nothing.

- If the `FETCHCONTENT_SOURCE_DIR_*` overrides fail to configure, drop them
  and configure plainly (it will clone the dependencies, about a minute).
- Build the test target only, unless your stream changes a CLI or the
  OpenSpiel utilities. Batch header edits before building: `card.h`,
  `game_state.h` and friends rebuild everything.
- Use `--gtest_filter=` while iterating; run the full suite before you finish.
- Never build in the main checkout's `build/`.
- The launcher must be `scripts/ccache_pch_safe.sh`, never plain `ccache`: plain
  ccache can return a precompiled header built from older headers, and every
  file is then compiled against stale declarations. The script always compiles
  a precompiled header itself and caches only the files that use it.
- Do not use `CCACHE_BASEDIR` / `CCACHE_NOHASHDIR` (the cross-worktree launcher
  in `CLAUDE.md`): it is unsafe while several worktrees edit headers. Before
  your final full-suite run, do `cmake --build build --target clean` once so
  nothing stale is linked, and say so in your report.
- The session scratchpad directory is shared by every stream: keep your scripts
  and logs in a subdirectory named after your stream.
- CMake caps concurrent jobs at 6 (`RIFTBOUND_BUILD_JOBS`); that cap is per build
  directory, so with two lanes the limit that matters is your own `-j3`.
- Test files are unity-built (`RIFTBOUND_UNITY_TESTS`, see `CLAUDE.md`): a new
  test file must not define file-scope names that clash with other test files.
  Wrap the whole file body in a namespace named after the file, as the existing
  `test_review_fix_*.cpp` files do.
- **Shell commands must not prompt the user.** No `rm`, no `sh -c` / `bash -c` /
  `eval` wrappers, no commands assembled from a variable (`$CMD`), no busy-wait
  loops. For several steps under the lock, write a script file and run
  `flock "$REPO/.worktrees/.build.lock" bash <script>`. Do not start a
  background wait loop that polls for other builds to finish: run the locked
  command in the foreground and let `flock` do the waiting.

## Closing an item

When an item is done, edit the plan file **in your worktree** and change its
checkbox, appending the resolution on the same line:

- `- [x] ... — FIXED: <what changed>; test: <TestSuite.Name>`
- `- [x] ... — ALREADY FIXED: <where/commit>`
- `- [x] ... — NOT A DEFECT: <evidence: file and symbol, card text or CR number>`
- `- [x] ... — MOVED: handled by <stream> (<reason>)` only when the
  orchestrator told you so.

Rules:

- Tick only items you own. An item with several sub-bullets or several cards
  is ticked only when **every** one is resolved; note per-card outcomes in
  sub-bullets if they differ.
- "NOT A DEFECT" needs evidence a reviewer can check. "Could not reproduce" is
  not evidence. It also still changes the code: the finding was raised because
  the code misled a careful reader. Add a test that pins the behaviour the
  finding questioned, and fix what misled them (a comment at the code site
  citing the card text or CR number, a rename, or removing the dead or
  contradictory construct). Write the line as
  `NOT A DEFECT: <evidence>; pinned by <Test.Name>; clarified in <file>`. The
  only exception is a finding about code that no longer exists.
- Nothing is optional. Do not close an item as "partial by design",
  "documented approximation", "low priority" or "future work". The only
  deferral allowed is a fix that needs a new action-vocabulary slot or a
  feature-layout change, which goes to stage 5 by name.
- If you cannot finish an item, leave it `- [ ]` and add an indented
  `- BLOCKED: <what is needed>` sub-bullet. Do not tick it.

## Commits

- Commit on your branch as you go, in logical units; one concern per commit.
- Subject line: `<area>: <what>` plus the plan item ids, e.g.
  `auras: remove text-parsed aura pass (2.1, 8.1)`.
- End every commit message with:

  ```
  Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
  ```

- Do not push, do not merge, do not touch other branches or worktrees.
- Before you report, rebase onto the current integration branch
  (`git fetch . && git rebase fix/integration` from your worktree), rebuild and
  re-run the full test suite. Resolve conflicts in your own files; if a
  conflict is in another stream's file, stop and report it.

## Your final report

Return, in this order:

1. **Result:** branch name, head commit, "full test suite: N passed, M failed"
   with the failing test names if any. Say plainly if you did not run it.
2. **Items closed:** each id with FIXED / ALREADY FIXED / NOT A DEFECT.
3. **Items not closed:** each id and what blocks it.
4. **Needs another stream:** changes you needed outside your files and did not
   make, or `shared:` commits you did make.
5. **New defects found** while working that are not in any plan (file, symbol,
   one sentence each). Do not fix unrelated ones; report them.

Report what actually happened. A stream that returns red tests or unfinished
items honestly is useful; one that claims done and is not costs a full cycle.
