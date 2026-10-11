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

## Building — one build at a time

The machine has 8 cores and other work is running; build with `-j3`, never more (user limit). All builds and test runs go
through a shared lock so only one runs at once:

```bash
REPO=/home/pirateporkchop/code/riftbound-engine
cd "$REPO/.worktrees/<stream>"
# configure once (reuses the already-downloaded dependency sources)
flock "$REPO/.worktrees/.build.lock" cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_SLOPPINESS=pch_defines,time_macros;ccache" \
  -DFETCHCONTENT_SOURCE_DIR_OPEN_SPIEL="$REPO/build/_deps/open_spiel-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$REPO/build/_deps/googletest-src"
# build + test
flock "$REPO/.worktrees/.build.lock" cmake --build build --target riftbound_tests -j3
flock "$REPO/.worktrees/.build.lock" env RIFTBOUND_ROOT=. ./build/riftbound_tests --gtest_brief=1
```

- If the `FETCHCONTENT_SOURCE_DIR_*` overrides fail to configure, drop them
  and configure plainly (it will clone the dependencies, about a minute).
- Build the test target only, unless your stream changes a CLI or the
  OpenSpiel utilities. Batch header edits before building: `card.h`,
  `game_state.h` and friends rebuild everything.
- Use `--gtest_filter=` while iterating; run the full suite before you finish.
- Never build in the main checkout's `build/`.

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
  not evidence.
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
