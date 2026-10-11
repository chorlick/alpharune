# perf-engine — follow-up 2 (plans/09 section LL: 9.LL1, 9.LL2, 9.LL3)

Branch `fix/perf-engine`, head `3290742`. **Verified against integration `d59fb14`** (merged in before the work; integration has not moved since).

## 1. Result

Clean rebuild (`cmake --build build --target clean`, all six targets, `-j3`, lane A lock, `scripts/ccache_pch_safe.sh`, fresh PCH): **0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **2931 passed, 0 failed** |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions** (unchanged) |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| trace / debug / HTML replay with a listener, 19 files | byte-identical to the binary at `1e453fa` |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | 10/10 clean |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | 10/10 clean |
| suite with `-DRIFTBOUND_CHECK_BOARD_INDEX=ON` (separate build, same sources) | 2931 passed; OpenSpiel 25; clone-equivalence 10/10; smoke split the same |

The traces are still identical because no stunned unit leaves the board in those games; the Stunned fix below changes behaviour only where one does.

No timings this round: nothing here is a performance change.

## 2. Items closed

| Item | Outcome |
|---|---|
| 9.LL1 Monch 597 | FIXED, with an engine defect behind it |
| 9.LL1 Emperor of the Sands 512 | FIXED |
| 9.LL1 Purifier 502 | NOT A DEFECT, pinned and clarified |
| 9.LL2 | FIXED |
| 9.LL3 | DONE: nothing had to change |

**9.LL1 — Monch, and Stunned off the board (engine defect).**
- Nothing cleared Stunned when a unit left the board. Each leave path resets damage, exhaustion and the combat designation; none reset the stun. A unit that was stunned and then returned to hand or killed stayed "a stunned unit" until the Ending Step.
- Rules: CR 124.1 (a card that changes zones to a Non-Board Zone is a new object; all temporary modifications stop being tracked) and CR 424.1 (stunning acts on "Units on the Board").
- A game could reach a wrong result: Monch got its [2] discount and ready entry from a stunned unit sitting in the opponent's hand or trash.
- Fix, in the engine and not in the card: `recalculateAuras` drops Stunned together with Empowered for every object that is neither on the board nor in the Legend Zone.
- Why there and not in each leave path: it is the one place every object passes after any route off the board, before a generator or a cost looks at it again. There are about ten leave paths and each already forgets something. The cost is that the status is cleared at the next cleanup, not at the instant of the move. If you want it at the move itself, that is a change to every leave path and I have not made it.
- Monch's walk is over the board.
- Tests, red first: `PerfEngineOffBoard.AStunnedUnitThatLeavesTheBoardIsNoLongerStunned` (engine path: stun, bounce / kill, cleanup), `PerfEngineOffBoard.MonchLooksAtTheBoardOnly`.

**9.LL1 — Emperor of the Sands.**
- A Sand Soldier is a token; off the board it has ceased to exist (CR 186.1). Its object is kept, and the hook gave it [Weaponmaster] on every aura pass, so `hasKeyword` was true on a dead token.
- I could not construct a game result that depended on it. The walk is over the board now.
- Test, red first: `PerfEngineOffBoard.EmperorOfTheSandsGivesNothingToASandSoldierThatHasCeasedToExist`.

**9.LL1 — Purifier.**
- An Equipment cannot be off the board and attached. Bounce and kill, the two routes the pin test drives, call `EffectExecutor::unattachGear` on the way (CR 719.5), and a unit that leaves sheds its Equipment the same way. I did not open banish and recycle for this.
- Pinned by `PerfEngineOffBoard.PurifierGivesAssaultOnlyThroughEquipmentOnTheBoard` (green before and after). The walk is over the board, also requires the unit on the board, and the comment at the site says why.

**9.LL2 — legends in test fixtures.**
- `addLegend` in `tests/cards/test_rules_compliance_cards.cpp` sets the Legend Zone. Ten other places built a legend in the default zone and are fixed the same way: `test_wave_a_batch1.cpp` ×2, `test_wave_a_batch4.cpp` ×2, `test_audit_fixes_1.cpp` ×4, `test_jhin_deck.cpp`, `test_wave_b_triggers.cpp`. The other 29 already set it. No assertion changed.
- The four "find my legend" loops (317, 502, 512, 784) walk `objects.inPlay()`.
- Card-file loops now: **344 converted of 359; 15 kept in 14 files**, each on the source scan's allow-list with its reason.

**9.LL3 — the helpers called in the converted loop bodies.** I opened every one. None moves, kills or changes the zone or location of an object.

| Helper | Where | What it does |
|---|---|---|
| `auraKeyword`, `auraMight`, `auraKeywordIfAbsent` | `card_helpers.h` | push one `AuraEffect` onto the object |
| `hasTag`, `hasAuraFrom`, `hasKeywordInAuraPass`, `isMighty`, `isEquipment` | `card_helpers.h` | read the object |
| `otherFriendlyUnitsHere` | `card_helpers.h` | counts units at a location |
| `isUnit`, `isGear`, `isRune`, `isToken`, `isEmpowered`, `isAtBase`, `isAtBattlefield`, `battlefieldId`, `hasKeyword` | `GameObject` | read the object |
| `cardDefId`, `maxEmpower`, `suppressesTemporaryTriggersHere` | `Card` | return a constant of the card class |
| `canPayEquip` | `Card` | takes a `const GameState&`; asks `equipEndsOnBoard`, `netEquipCost`, `resource::canPay`, and the `const` hook `canPayEquipExtraCost` |
| `grantMech` | Experimental Hexplate 396 | adds the "Mech" tag to the bearer and a counter to the gear |
| `moversTo` | Forgotten Signpost 607 | builds a list of unit ids from a `const` state |
| `legalTargets` | Azir, Ascendant 373 | builds a list of unit ids from a `const` state |
| `destinationMask` | Call to Battle 663 | builds a bit mask of battlefields |
| `hasDomain`, `qualifies` | Decree of Focus 827 | read domains, designations and the chain |
| `weaponmasterTerms` | `weaponmaster_base.h` | returns a constant |
| `buff_if_unbuffed` | Peak Guardian 223 | calls `EffectExecutor::buffUnit` |

The executor operations called from the 21 loops I had read one by one were opened too:

| Operation | What it does |
|---|---|
| `buffUnit` | +1 buff, recompute Might, emit "buffed" |
| `readyObject` | clears exhausted, emits "readied" |
| `giveTemporaryMight` | this-turn Might, emit; a negative amount can be replaced by the target's `replaceHostileEffect` |
| `giveTemporaryKeyword` | this-turn keyword, emit |
| `replaceHostileEffect` | one override: Gangplank, Naval 873 gives itself +3 [M] this turn instead |

The events these emit reach `TriggerManager`, which queues chain items and moves nothing. The checked build, where a loop that moves an object under its range is an error, passes the whole suite.

## 3. Items not closed

None.

## 4. Needs another stream / trainer notes

- No `shared:` commits.
- Behaviour change for the trainer to know: a stunned unit that leaves the board is no longer stunned from the next cleanup on. Games where that happens can differ from before; the fixed-seed smoke and trace games do not contain the case.

## 5. New defects found

No new findings.

## 6. How the work was run

- Lane A lock for every build, full-suite run and the checked-index run.
- Run without the lock: the trace captures, single microbench runs and filtered runs of a few tests.
- No `rm`, no `sh -c`, no `eval`.
