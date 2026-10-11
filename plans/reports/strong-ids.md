# strong-ids — final report (5.5, 9.KK1, and the event-bus measurement)

Branch `fix/strong-ids`, head `b9442bf`. **Verified against integration `3fa08be`**, merged in before the clean build (it brought plan and status files only).

## 1. Result

Both items are closed. Ids are distinct types and no id mix-up was found; a `GameState` copy costs 6.2 µs instead of 14.9 µs; `mcts:sims=50` is 13% faster.

Clean rebuild at `9e07320` (`cmake --build build --target clean`, seven targets, `-j6`, both lane locks, `scripts/ccache_pch_safe.sh`, `CCACHE_NAMESPACE=strong-ids`, tests at `-O0`): **0 warnings**. The head commit after it changes one line inside `#ifdef RIFTBOUND_CHECK_BOARD_INDEX`; I rebuilt incrementally and re-ran the first seven rows below at `b9442bf`. The fault-injection and ISMCTS rows were run at `9e07320` only.

| Gate | Result |
|---|---|
| `riftbound_tests` | **3018 passed, 0 failed** (2998 at `fcfa93b` + 20 new) |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions** (unchanged) |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | byte-identical to the binary at `fcfa93b`, after every tranche and at the end |
| `ctest -R riftbound_strong_id` | 26/26 (case 0 compiles, cases 1–25 are rejected) |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | **10/10 clean** |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | **10/10 clean** (no "did not reach the snapshotted decision") |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build) | 3018 passed; OpenSpiel 25; clone-equivalence 10/10; smoke split the same |
| history tokens and action slots | five tokens and three slots equal the numbers printed by a build of `fcfa93b` |

Trace set: the perf stream's script with only the worktree path changed — `--trace --debug`, `--debug` and the HTML replay of fiora / yi_rebuild in both seatings at seeds 1, 7 and 20261010, plus a three-game batch replay at seed 5.

`kStateFeatureDim`, `kCardVocabSize`, the feature layout, the action vocabulary and history tokens are untouched. No test was deleted or weakened.

## 2. Items closed

| Item | Outcome |
|---|---|
| 5.5 | FIXED |
| 9.KK1 | FIXED |

Both are ticked and moved to `plans/completed/`; `plans/remaining/05-integrity.md` had no item left and is deleted; `plans/remaining/README.md` lists three items.

### 5.5 — strong id types

**Design** (`src/core/types.h`):

- `StrongId<Tag>`: one `uint32_t`, trivially copyable, same size and numbers. Ordered and hashed by the number (`std::hash` returns `std::hash<uint32_t>` of it), so container order is what it was. Prints with `<<` and `toString(id)`.
- To and from an integer only by name: `id.raw()`, `X::fromRaw(n)`. No arithmetic, no indexing, no `bool`, no `static_cast` either way.
- `kInvalidId` is the one "none" of every kind and the default-constructed value. It converts to each id kind and to nothing else; in an integer slot none is `kInvalidId.raw()`.
- `CardDefId` converts from an integer **literal** (consteval; not from a variable, not from 0), as confirmed. No other kind takes a number.
- **One decision beyond what was confirmed:** `BattlefieldLocation{0}` still compiles. The location type has an explicit consteval constructor from a slot literal; `BattlefieldId` itself takes no number. Without it 139 test lines and no source line would have changed. Say if it should go.
- `GameState::battlefields` is indexed by position and by `BattlefieldId` alike, so `state.battlefields[bf_id]` is unchanged.
- **`ZoneId` deleted**: nothing used it. `AbilityId` kept and made strong: `Intent::ability` holds one (compared and fingerprinted, never set by the engine).

**Compile errors: about 1,300 sites, no real mix-up.** Per kind:

| Kind | Sites | Real mix-ups |
|---|---|---|
| `CardDefId` | 79 | 0 |
| `BattlefieldId` | about 580 (476 in tests) | 0 |
| `ChainItemId`, `AbilityId` | 8 | 0 |
| `GameObjectId` | about 650 | 0 |
| `kInvalidId` used as a number | 11 | 0 |

- No object id was used as a card id or the reverse, and no battlefield slot as an object id.
- No "none" test was against the wrong sentinel. The 32 places that wrote 0 for "no card" each tested their own field against 0.
- Every site was one of: an explicit cast into or out of an integer slot; `std::to_string(id)`; 0 for "no card"; id arithmetic in an allocator or a loop; a test naming an id by a literal.
- The compiler cannot see a slot written as one kind and read as another. I read all 23 non-object `fromRaw` sites against their writers; each pair matches. The 79 `GameObjectId::fromRaw` sites I read in the script's output but did not pair one by one.

**Where an id meets a plain integer**, by kind of slot (165 `raw()` and 104 `fromRaw` in `src/`):

| Slot | Where |
|---|---|
| `resume_data` / reflexive-trigger data | `card_helpers.h` (the pick, select and split-damage helpers), `card.cpp`, 37 card files; `objectIdsFromRaw` (new, `card_helpers.h`) reads a run of slots |
| `card_counters` values | `trigger_manager.cpp` (`__defend_attacker_id`, `__defend_bf`, `__combat_participant`), `game_engine.cpp` (`__facedown_play_bf`), 9 card files (id + 1 encodings) |
| table index | `CardRegistry`, `GameState::createObject`, `ChainState::allocateId`, battlefield slot allocation in `setupBattlefields` / `addBattlefieldToken` |
| features | `safeCardId`, `safeBfId`, the per-zone histogram (`feature_extractor.cpp`) |
| action vocabulary | `defIdOf`, the named-card and battlefield slots (`action_vocab.cpp`) |
| history token / fingerprints | `historyToken`, the Intent hash, the cleanup checksum and `costStateFingerprint` |
| JSON | `play_server.cpp`, `state_editor.cpp` |
| log and replay text | `toString(id)` on 113 lines |

**Values that must not change:**

- History tokens fed `granted_ability_def` as a number and 0 meant "own ability". The field is `kInvalidId` now and the token still feeds 0, with a comment.
- Closing the gap from the interim report: I built `fcfa93b` in a scratch copy with a probe and recorded five history tokens and three action slots (an own-ability activation, a granted one, a named-card `MakeChoice`, a `ChooseBattlefield`, an end turn). `StrongIds.HistoryTokensAndSlotsAreTheNumbersOfTheBinaryBeforeIdsWereTypes` asserts those constants.

**Behaviour that does change:**

- `DeckValidator` given a legend of `kInvalidId` used to throw from the card lookup; it now reports "no legend".
- A default-constructed id is none, not 0. I checked every value-initialized id in `src/` (event structs, locals, `resize` calls); none relied on 0.

**Tests:** `tests/test_strong_ids.cpp` (11 tests, `static_assert`s over every pair of kinds), `tests/compile_fail/strong_id_mix.cc` (26 cases as ctests).

### 9.KK1 — the per-object copy cost

**Measured first.** `riftbound_clone_breakdown` has a new section: each member of every object copied and destroyed in a steady-state loop, allocations counted by a replaced `operator new`. Decision 100 of fiora vs yi_rebuild, 108 objects.

| Per `GameState` copy | Before | After |
|---|---|---|
| time | 14.9 µs | **6.2 µs** |
| heap allocations | 348 | 141 |
| `sizeof(GameObject)` | 744 | 592 |
| `card_counters` (10 objects had any) | 1.3 µs / 54 allocations | 0.19 µs / 10 |
| `domains` | 1.2 µs / 106 | 0.10 µs / 0 |
| `tags` | 1.1 µs / 40 | 0.11 µs / 0 |
| `name` | 0.75 µs / 17 | 0.10 µs / 0 |
| `string_state` | 0.7 µs / 0 | 0.15 µs / 0 |

With `glibc.malloc.mxfast=0` (the allocator artefact switched off) the copy is 14.2 → 6.4 µs, so the gain is not that artefact.

**What changed:**

- `name`, `tags`, `domains` are `Interned<T>` (`core/interned.h`): one pointer to a shared constant value in a process-wide pool. Equal values are one value; an edit makes or finds another and never shows in a second object.
- `card_counters`, `string_state` are `KeyedSlots<V>` (`core/keyed_slots.h`): names interned to small keys, one null pointer when empty, entries four to a block.
- I did not read these from the `CardDef`. They do differ from the def at run time: tokens with no def, `copyUnit`, Shady Spectacles 924 restoring a copied unit, Experimental Hexplate 396 adding and removing "Mech".
- Counters keep the map's guarantee that a reference to an entry survives later insertions. Tornado Warrior 886 and Evelynn 703 hold `auto& flag = counters["a"]`, add another counter and then write through it; a flat vector would have broken both.
- Nothing iterated the counters, so their order was never observable.

**Numbers** (medians of three, binaries alternated, both lane locks held, nothing else compiling, load 1.1–1.2):

| | `fcfa93b` | After 5.5 | After 9.KK1 |
|---|---|---|---|
| one `GameState` copy | — | 14.9 µs | 6.2 µs |
| clone + `ApplyAction`, replayed (Clone / Apply / teardown, µs) | — | 7.9 / 89.6 / 37.7 | 4.8 / 80.3 / 34.8 |
| clone + `ApplyAction`, resumed | — | 8.6 / 70.3 / 34.8 | 5.5 / 61.8 / 31.6 |
| `mcts:sims=50`, 5 games, seed 20261010 | 3.355 s | 3.315 s | **2.879 s** (−13%) |
| `ismcts:sims=20`, 8 games, seed 1 | 2.365 s | 2.305 s | **2.090 s** (−9%) |
| random, 1000 games, 1 thread | 1.307 s | 1.291 s | **1.227 s** (−5%) |
| random, 200 games, 6 threads | 0.052 s | 0.051 s | 0.050 s |

The strong ids cost nothing at run time.

**Is the copy still the top cost in search?** No. In `mcts:sims=50` it is now about 4% (19,316 advanced clones × 6.2 µs of 2.88 s). The larger cost is building and tearing down an engine per advanced clone, of which the event bus alone is roughly 12% (section 3). Recommendation, not work:

1. Stop rebuilding the bus and its subscriptions for every clone. That is worth about three times what copy-on-write of the state could save.
2. If the copy is taken further: of the 6.2 µs, the 109 map nodes and their 64 KB are about half, `last_known` 0.95 µs, six usually-empty vectors 1.6 µs. A contiguous object store removes the node allocations; copy-on-write per object would remove most of the rest.

**Tests:** `tests/test_object_copy_cost.cpp` (8 tests, with a `static_assert` that `sizeof(GameObject) <= 600`), `ReviewFixC1Test.ExperimentalHexplate_OnlyTheBearerBecomesAMech` (engine path: units with equal tags share a value, and the tag one gets shows on neither the other nor an earlier state copy).

### Files changed and how

| | Files | How |
|---|---|---|
| Card files, 5.5 | 72 of 955 | 70 by a script driven by the compiler's error list; Shady Spectacles 924 and Fallen Feline 919 by hand (two lines each) |
| Card files, 9.KK1 | 3 of 955 | by hand: Hard Bargain 457 and Lunar Boon 687 (`name.get()` next to a literal in `?:`), Experimental Hexplate 396 (`tags.edit`) |
| Card files in total | 73 of 955 | 882 not touched |
| Tests, 5.5 | 70 existing files, 801 lines | scripted: `fromRaw` around a literal, `toString`, a loop counter. No expectation changed |
| Tests, 9.KK1 | none changed | one test added to `test_review_fix_c1.cpp` |

The script made three rewrites and printed every line for reading: `static_cast<T>(id)` → `static_cast<T>(id.raw())`, `static_cast<XId>(n)` → `XId::fromRaw(n)`, `std::to_string(id)` → `toString(id)`. In each the author had already written an explicit conversion.

Rebuilds: 5.5 took more than one per kind (a keep-going build shows only the first layer of errors: headers, then sources, then tests). 9.KK1 took three.

## 3. Event bus: measurement only, nothing changed in the tree

**The dummy-mutex variant is not measurably faster.** Built in a scratch copy of `0773639` with all 35 event signals declared with `mutex_type<dummy_mutex>`; timed against the normal binary, alternating, both locks held.

| Batch | Normal | Dummy mutex |
|---|---|---|
| random, 1000 games, 1 thread — 3 runs | 1.197 / 1.242 / 1.252 s | 1.227 / 1.234 / 1.240 s |
| same — 5 more runs, median | 1.240 s | 1.242 s |
| `mcts:sims=50`, 5 games — 3 runs | 2.859 / 2.898 / 2.917 s | 2.810 / 2.812 / 2.852 s |
| same — 5 more runs, median | 2.893 s | 2.883 s |

The first MCTS set suggests 3%; the second set does not repeat it (one dummy run was 3.79 s). Load was 2–5 during these runs, left over from the variant builds.

**Share spent in signals2 itself**, from two sources that agree:

| | random 1000×1 | `mcts:sims=50` |
|---|---|---|
| gprof, samples in signals2-named symbols | 7.7% (about 117 samples in all, so ±1%) | 24% |
| emissions (gprof call count of the emission lock) | 1.59 million | 2.09 million |
| direct timing of one emission (scratch probe) | 19 ns with no slot, 46 ns with one, 60 ns with two; a `std::function` call is 1.3 ns | same |
| **emission, estimated** | 0.07–0.10 s of 1.23 s: **6–8%** | 0.09–0.13 s of 2.88 s: **3–4%** |
| buses built | 1,000 | 19,316 |
| direct timing of one empty `EventBus` built and destroyed | 7.2 µs, plus about 0.15 µs per connection | same |
| **bus construction and teardown, estimated** | under 2% | about 0.35 s: **12%** |

- The mutex is not the cost. Emission is dominated by the slot-list snapshot and the type-erased call; construction by allocations (each of the 35 signals allocates its implementation, its slot list and its mutex).
- The construction estimate assumes about 70 connections per engine (two per signal: gprof counts 38,632 connections of a type over 19,316 buses). I did not time a real engine's subscriptions.
- Threads: `GameRunner` builds its bus on the game's stack and `RiftboundState` owns one per state, each used by the thread that plays that game. I did not establish which of the play server's threads emits.

## 4. Needs another stream / trainer notes

No `shared:` commits. Every API a caller of the engine must change:

- **Ids are classes.** `id.raw()` for the number, `X::fromRaw(n)` from a number, `toString(id)` for text. `static_cast` to or from an integer, `std::to_string(id)`, `id + 1`, `vec[id]`, `if (id)` and `id == 0` no longer compile.
- **`kInvalidId` is not a number.** `auto x = kInvalidId;` needs its type named; in an integer slot write `kInvalidId.raw()`.
- **A default-constructed id is none**, not 0.
- **"No card" is `kInvalidId`** in `DeckSubmission::legend` / `chosen_champion`, `Intent::granted_ability_def`, `ChainItem::granted_ability_def`, `GrantedAbilityRef::source_def_id` and a default `CardDef::id`.
- A card id still converts from a literal. `BattlefieldLocation{0}` still compiles; a bare `BattlefieldId` needs `fromRaw`.
- An id cannot be a non-type template parameter.
- `ZoneId` is gone.
- `GameState::battlefields` is a `BattlefieldList` (derives from the vector).
- `ChainState::next_id++` and `GameState::next_object_id++` do not compile: use `allocateId()` / `createObject()`.
- **`GameObject::name` / `tags` / `domains` are `Interned<T>`.** Reading is unchanged in almost every form. `std::string& n = obj.name` (non-const) does not compile; with a literal in `?:` write `obj.name.get()`; `obj.tags.erase(...)` becomes `obj.tags.edit(fn)`.
- **`GameObject::card_counters` / `string_state` are `KeyedSlots<V>`.** `[]`, `find`, `end`, `count`, `erase`, `at`, `empty`, `size`, `clear` work; range-for does not (`forEach` visits in an unspecified order); `it->first` is a `SlotKey`, with `slotKeyName(key)` for the name.
- `sizeof(GameObject)` is 592.
- JSON through nlohmann works for ids and interned values (`to_json` found by ADL).

## 5. New defects found

- **`Intent::chosen_battlefield` has two meanings.** In `HideCard` it is a battlefield slot; in the setup decision `ChooseBattlefield` it is the position of a card in the player's battlefield list, before any battlefield exists. Not a behaviour defect: `setupBattlefields` writes and reads it, and the play server and renderer treat that decision separately. It is written at the field now. A field of its own would change the action slot and the history token, so it is stage 5's if wanted.
- **The per-clone event bus** (section 3) is the largest single cost I can now name in search. Not a defect; reported because it changes which step is next.

Play UI, deferred: none noticed.

## 6. How the work was run

- Both lane locks for every configure, build, full-suite run and timing run. Nothing else was building; load during the 9.KK1 timings was 1.1–1.2.
- Run without the locks: the first baseline trace capture, filtered runs of a few tests, and `g++ -fsyntax-only` on the compile-fail file.
- Scratch copies of the source (`git archive` into the session scratchpad, built there): `fcfa93b` for the token probe and the bus probe, `0773639` for the dummy-mutex variant and the gprof build. The checked-index build also lives in the scratchpad. None is in the tree.
- `ChainItemId` and `AbilityId` share one commit (eight errors between them) rather than one each.
- Commits from `5f325e4` on carry a `Claude-Session:` trailer after `Co-Authored-By:`, as the session's attribution instruction asked from that point.
- No `rm`. One file was removed with `git rm` (`plans/remaining/05-integrity.md`, as the brief requires for an emptied file).
