# strong-ids — interim report 1 (5.5, first id kind: `CardDefId`)

Branch `fix/strong-ids`, head `390eabe`, on `fix/integration` at `fcfa93b` (not moved). One commit; it can be merged while the other kinds follow.

## 1. Result

Built with both lane locks, `-j6`, `scripts/ccache_pch_safe.sh`, `CCACHE_NAMESPACE=strong-ids`, `-DRIFTBOUND_TEST_OPT_LEVEL=0`. Not a clean rebuild (that is for the final report). 0 warnings.

| Check | Result |
|---|---|
| `riftbound_tests` | **3005 passed, 0 failed** (2998 at `fcfa93b` + 7 new `StrongIds.*`) |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions (unchanged) |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| fixed-seed traces | 19 files, 14,708,563 bytes, byte-identical to the binary at `fcfa93b` |
| `ctest -R riftbound_strong_id` | 10/10 (case 0 compiles, cases 1–9 are rejected) |

Trace set: the perf stream's script, unchanged apart from the worktree path — `--trace --debug`, `--debug` and the HTML replay of fiora/yi_rebuild in both seatings at seeds 1, 7 and 20261010, plus a three-game batch replay at seed 5.

## 2. Design

`StrongId<Tag>` in `src/core/types.h`; `CardDefId` is `StrongId<CardDefIdTag>`. The other five aliases are still `uint32_t` and follow one per commit.

- One `uint32_t`, trivially copyable, same size and numbers as before. Ordered and hashed by the number (`std::hash` gives `std::hash<uint32_t>` of it), so container order is what it was. Streams and `toString(id)` print the number.
- To and from an integer only by name: `id.raw()` and `CardDefId::fromRaw(n)`. No conversion to `bool`, no arithmetic, no indexing, no `static_cast` either way.
- `kInvalidId` is now a value of its own type (`InvalidId`) that converts to every id kind and is what a default-constructed id is. It still converts to `uint32_t`, so the kinds not yet converted and plain integer slots (`resume_data[2] == kInvalidId`) compile as before.
- **One decision to confirm.** A card id converts from an integer *literal*: `d.id = 235`, `registry.get(235)`, `addUnit(P1, 235, ...)`. The constructor is `consteval`, so an integer variable, a counter, a resume slot, 0 and `UINT32_MAX` do not compile. I chose this over a scripted `d.id = CardDefId{235}` in all 955 card files because of the instruction that most card files should not change. The cost: `std::is_convertible<int, CardDefId>` is true, so "a run-time integer is refused" is proven by the compile-fail ctests rather than by a trait. No other id kind will take a literal. If the stricter form is wanted, it is one scripted pass over the `d.id = N;` lines and about 2,400 test lines.

## 3. Compile errors and how they classified

79 error sites in all, over four keep-going builds. **No real mix-up of a card id with another id kind was found**: nothing passed an object id where a card id was expected or the reverse.

| Class | Sites | What was done |
|---|---|---|
| 0 used as "no card" | 32 | Now `kInvalidId`. 6 field defaults (`CardDef::id`, `DeckSubmission::legend` / `chosen_champion`, `Intent::granted_ability_def`, `ChainItem::granted_ability_def`, `GrantedAbilityRef::source_def_id`), 6 comparisons in `game_engine.cpp`, 11 in `deck_validator.cpp`, 3 locals in `feature_extractor.cpp`, 6 in tests. |
| id printed with `std::to_string` | 15 | `toString(id)` (6 in `src`, 9 in tests). |
| id stored in or read from an integer slot, or used as an index | 14 | `raw()` / `fromRaw()`; listed below. |
| tests: an integer where a card id is meant | 18 | Mechanical; listed below. |

Where a card id meets a plain integer, by kind:

- **Table index:** `CardRegistry::get` and `registerCard` (the by-id table).
- **`card_counters` value:** Shady Spectacles 924 (`__spectacles_def`, written and read back).
- **`resume_data` slot:** Fallen Feline 919 (the named spell).
- **Features:** `safeCardId` and the per-zone card histogram in `feature_extractor.cpp`.
- **Action vocabulary:** `defIdOf` and the `MakeChoice` named-card slot in `action_vocab.cpp`.
- **History token:** `chosen_card_def` and `granted_ability_def` in `historyToken`.
- **Log / replay text:** `card_registry.cpp`, `card_db.cpp`, `state_renderer.cpp`.

On the two "none" values:

- Every place that tested a 0-sentinel field tested it against 0, and every place that tested a `kInvalidId` field tested it against `kInvalidId`. I found no test against the wrong one.
- `recordReveal` filtered `kInvalidId` only. All 20 emitters of `CardRevealedEvent` pass an object's `card_def_id`, which is never 0, so the missing filter could not be reached. There is one none now; `StrongIds.ARevealOfNoCardIsNotRemembered` pins it.
- The history token fed `granted_ability_def` as a number, and 0 meant "own ability". It still feeds 0 for none, with a comment, so tokens keep their values. `StrongIds.TheHistoryTokenOfAnOwnAbilityKeptItsNumber` pins that none and 0 give the same token; I did not compare a token's number against the old binary.
- Behaviour that does change: `DeckValidator` with a legend of `kInvalidId` used to throw from the card lookup and now reports "no legend". A legend of 0 no longer compiles.

## 4. Files changed

- **Card files: 2 of 955, by hand** (Shady Spectacles, Fallen Feline: two lines each). 953 not touched.
- Engine and core: 12 files.
- Tests: 17 existing files, 33 mechanical edits, no expectation changed:
  - 9 × `std::to_string(id)` → `toString(id)`;
  - 6 × `0` → `kInvalidId` or `CardDefId::fromRaw(0)` (the test that asks the registry for id 0 still does);
  - 4 × a loop over every card id now counts a `uint32_t` and makes the id with `fromRaw`;
  - 2 × `first + i` → `fromRaw(first.raw() + i)`;
  - 5 × a braced list of numbers given its element type;
  - 5 × `EXPECT_EQ(id, 7u)` → `EXPECT_EQ(id, CardDefId{7})`;
  - 1 × a template that took a card id as a non-type parameter takes its number;
  - 1 × `auto late = kInvalidId` given its type.
- New: `tests/test_strong_ids.cpp`, `tests/compile_fail/strong_id_mix.cc`, ten ctests in `CMakeLists.txt`.

## 5. Trainer notes so far

- `CardDefId` is a class. `id.raw()` for the number, `CardDefId::fromRaw(n)` from a number, `toString(id)` for text. A literal still converts.
- `CardDefId` cannot be a non-type template parameter (its number is private).
- "No card" is `kInvalidId` in `DeckSubmission::legend` / `chosen_champion`, `Intent::granted_ability_def`, `ChainItem::granted_ability_def`, `GrantedAbilityRef::source_def_id` and a default `CardDef::id`. Code that tested any of them against 0 must test `kInvalidId`.
- `auto x = kInvalidId;` no longer gives an integer: name the type.
- Feature layout, `kStateFeatureDim`, `kCardVocabSize`, the action vocabulary and history tokens are untouched.

## 6. New defects found

No new findings.

## 7. Still to do

`BattlefieldId`, `ChainItemId`, `GameObjectId`; `AbilityId` and `ZoneId` (delete if nothing uses them); then 9.KK1. Plan files are not ticked yet.
