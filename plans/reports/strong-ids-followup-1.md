# strong-ids — follow-up 1 (9.RR1: every object id read from an integer slot, paired with its writer)

Branch `fix/strong-ids`, head `4654bcc`. **Verified against integration `2470299`.** The clean build and the whole gate set ran at `82cbe29`; the head is that commit plus a merge of integration that brought one line of `plans/STATUS.md` and no source.

## 1. Result

**No mismatch.** All 79 `GameObjectId::fromRaw` readers, in 40 files, read their slot as the kind it was written, with the encoding it was written in, and test "none" the way the writer stores it. No source behaviour changed.

Clean rebuild at `82cbe29` (`cmake --build build --target clean`, seven targets, `-j6`, both lane locks, `scripts/ccache_pch_safe.sh`, tests at `-O0`): **0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **3019 passed, 0 failed** (3018 + the new pin) |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, P1 139 / P2 61 / 0 draws, 17 turns, 294 decisions (unchanged) |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| fixed-seed traces, 19 files, 14,708,563 bytes | byte-identical to the binary at `fcfa93b` |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | 10/10 clean |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | 10/10 clean |
| `ctest -R "riftbound_strong_id\|riftbound_one_card_table"` | 27/27 |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build) | 3019 passed; OpenSpiel 25; clone-equivalence 10/10; smoke split the same |

One thing to know about the trace row: the gate script's first comparison reported a difference. It had written into the directory of my previous final run, so seven HTML files held two replays each. A capture into a fresh directory is byte-identical; that is the result in the table.

## 2. Item closed

| Item | Outcome |
|---|---|
| 9.RR1 | NOT A DEFECT, with evidence (the table below), a pin and a comment at the code site |

- **Pin:** `StrongIds.EveryObjectIdReadFromAnIntegerSlotIsOnTheAuditedList` scans `src/` for `GameObjectId::fromRaw(` and compares the count per file with the audited list. A new reader, or one more in a listed file, fails until it is paired and the count updated.
- **Code site:** the rule is written at `StrongId::fromRaw` in `src/core/types.h`.
- Ticked and moved to `plans/completed/09-found-during-fix-run.md` section RR; `plans/remaining/README.md` lists four items.

Correction to my final report: it said 78 sites in one place and 79 in another. It is 79; Arise 513 has two on one line.

## 3. The pairing

Three encodings are in use. Every reader uses its writer's.

- **plain / −1**: the id's number; none is `kInvalidId.raw()`, which is −1 in the signed slot and reads back as `kInvalidId`.
- **id + 1 / 0**: 0 is none; the reader subtracts 1.
- **list**: a run of ids with no none in it; the reader walks the same range.

### Helper pairs (each covers all its callers)

A card that calls one of these never touches the slot itself, so pairing the helper once covers every caller.

| Slot | Writer | Reader | Encoding | Verdict |
|---|---|---|---|---|
| `resume_data[2]` | `Card::pickTarget` (`store`; the mistarget path writes none) | same function, 2 reads | plain / −1 | match; read only once `kTgtDone` is set, which the write sets |
| `resume_data[3..4]` | `Card::pickTargetPair` (`finish`, step 1, mistarget) | same function, 5 reads | plain / −1 | match; read only at `kPairDone` or while waiting for the second, both after a write |
| `resume_data[slot + 1]` | `selectObject` (`answer`) | same function | plain / −1 | match; read only when `resume_data[slot] == 2`, set with the write |
| `resume_data[slot + 1]` | `selectObjectAs` (`answer`) | same function | plain / −1 | match, as above |
| `resume_data[slot + 1]` | `selectObject` | `selectSeveral` | plain / −1 | match; none ends the picks |
| `resume_data[2 + i]`, extras | `splitDamageAmong` (target insert, cease-to-be-target writes none, extra points) | same function, 4 reads | plain / −1 | match; `data[0]` is the count and is never read as an id |
| `resume_data[kTokenPlaySlot + 1 ..]` | `playTokensChoosingLocations` (`play_one`) | same function (`collect`) | list | match; the slot itself is the count |
| a run of slots | the caller | `objectIdsFromRaw` | plain / −1 | match for its three callers (Cull the Weak 209, Icathian Rain 247, Fox-Fire 253, below) |

The slots are padded with 0 before first use. No reader reads a padded slot as an id: each read is behind the state flag its write sets.

### Engine

| Slot | Writer | Reader | Encoding | Verdict |
|---|---|---|---|---|
| `GameState::next_object_id` | `createObject` | `createObject` | counter from 1 | match (2 sites) |
| JSON `object_id`, `order[]` | the play server's own frames (`static_cast<int>(id.raw())`) and the client | `play_server.cpp`, 3 reads through `idField` | plain; absent or out of range gives `kInvalidId.raw()` | match |

### Cards that keep their own slots

| Card | Slot | Writer | Encoding | Verdict |
|---|---|---|---|---|
| The Zero Drive 412 | `resume_data[1..]` | the units to play, pushed after index slot 0 | list | match; reader starts at 1 |
| Scryer's Bloom 698 | `resume_data[1..2]` | the cards looked at | plain | match; read only for `i < resume_data[0]` |
| Bottled Constellation 854 | `resume_data` | each pick pushed | list | match |
| Shady Spectacles 924 | counter `__spectacles_bearer` | bearer id + 1 | id + 1 / 0 | match; reader subtracts 1 and runs only when the entry exists |
| Void Burrower 553 | `resume_data[1..2]`, `[0]` | the two revealed cards; the banished card | plain / −1 (slots start at −1) | match |
| Deceiver 756 | reflexive `data[0]` | the token | plain | match (`data[1]` is a battlefield, id + 1 / 0, read as one) |
| Dr. Mundo 109 | `resume_data` | valid picks only | list | match |
| Kinkou Monk 141 | `resume_data[8..9]` | initialised to none, then a pick | plain / −1 | match |
| Rell 347 | reflexive `data[0]` | the Equipment | plain | match |
| Overzealous Fan 449 | counter `__defend_attacker_id` | `TriggerManager`, the attacker | plain | match; reader checks the object exists |
| Rek'Sai 491 | `resume_data[kPicked]`, `[kRest]` | the banished card; the other card or none | plain / −1 | match |
| Keeper of Masks 643 | reflexive data | the tokens | list | match |
| Reinforce 62 | `resume_data` | the cards looked at; the chosen unit pushed last and popped first | list | match (3 reads) |
| Promising Future 115 | `resume_data[0..1]` | each player's banished card or none | plain / −1 (slots start at −1) | match |
| Overt Operation 153 | `resume_data[1..]` | buffed units after index slot 0 | list | match; the index is incremented before the first read, so slot 0 is never read as an id |
| Stacked Deck 183 | `resume_data[1..n]` | the cards after count slot 0 | list | match (4 reads, all from 1) |
| Cull the Weak 209 | `resume_data` | valid picks only | list | match |
| Icathian Rain 247 | `resume_data` | each pick, or `kLost` | plain / −1 (`kLost` is defined as `kInvalidId.raw()`) | match; the reader tests `kLost` |
| Fox-Fire 253 | `resume_data` | while picking: the picks. After a re-choice: `[n, n originals, the subset]` | list, two layouts | match; each reader runs in the phase whose layout it reads |
| Dragon's Rage 255 | reflexive `data[0]` | the mover | plain | match |
| Flash 309 | `resume_data` | the picks | list | match |
| Hard Bargain 457 | `resume_data[0]` | the countered item's source | plain | match |
| Guards 475 | `resume_data[kTokenSlot]`; reflexive `data[0]` | token + 1; the token | id + 1 / 0; plain | match (reader tests `!= 0`, subtracts 1) |
| Arise 513 | reflexive data | the tokens | list | match (2 reads) |
| Hostile Takeover 516 | counter | the seized unit + 1 | id + 1 / 0 | match |
| Tricksy Tentacles 616 | `resume_data[1..]` | picks after a Might bound in slot 0 | list | match; readers start at 1 |
| Disposal Order 665 | `resume_data[kCount + 1 ..]` | the picks | list | match |
| Abandon 693 | `resume_data[0]` | the top card | plain | match |
| Bone Skewer 701 | reflexive `data[0]` | the chosen unit | plain | match |
| Heedless Resurrection 704 | `resume_data[kKilled]`; `[kRevived]` | victim + 1; pick + 1, or −1 for "nothing to play", 0 for "not asked" | id + 1 / 0 | match; both readers return before the subtraction unless the slot is positive |
| Undying Loyalty 730 | counter | pick + 1, 0 for none | id + 1 / 0 | match; reader tests `<= 0` |
| Mirror Image 757 | reflexive `data[0..1]` | the token; the model | plain | match |
| Wild Claw 876 | `resume_data[0]`; reflexive `data[0]` | the chosen card; the played unit | plain | match (3 reads) |
| Decree of Discord 894 | `resume_data[1..]` | picks after a Might bound in slot 0 | list | match |
| Shuriken Flip 927 | `resume_data[0]` | the unit | plain | match |
| Lightning Rush 943 | `resume_data` | the top three cards | list | match (3 reads) |

Also checked, although they are comparisons and not `fromRaw` reads: the five places that compare a counter with `id.raw()` (Experimental Hexplate 396, Brutalizer 365, Shady Spectacles 924 twice, Star Spring 771) use the same `+ 1` or plain form as their writers.

What the audit does not cover: a slot whose reader and writer are in different cards. I found one such pair, `__defend_attacker_id` (written by `TriggerManager`, read by Overzealous Fan 449), and it matches.

## 4. Needs another stream / trainer notes

No `shared:` commits. Nothing for the trainer: this follow-up adds one test and one comment.

## 5. New defects found

No new findings.

## 6. How the work was run

- Both lane locks for every build, full-suite run and gate run. Nothing else was building.
- The checked-index build is in the session scratchpad, not in the tree.
- No `rm`.
