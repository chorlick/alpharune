# perf-engine — follow-up 3 (plans/09 item 9.LL4)

Branch `fix/perf-engine`, head `e18fa2d`. **Verified against integration `7f4d92d`** (merged in before the work; integration has not moved since).

## 1. Result

Clean rebuild (`cmake --build build --target clean`, all six targets, `-j3`, lane A lock, `scripts/ccache_pch_safe.sh`, fresh PCH): **0 warnings**.

| Gate | Result |
|---|---|
| `riftbound_tests` | **2943 passed, 0 failed** |
| `riftbound_openspiel_tests` | 25 passed |
| `riftbound_clone_equiv_test` | 10/10 |
| smoke, 200 games, seed 20261010, 6 threads | rc 0, **P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions** (unchanged) |
| MCTS batch, 4 games, seed 7 | rc 0 |
| same with `RIFTBOUND_STEP_FAULT_AT=40` | rc 1, "injected engine fault at decision 40", 0 draws |
| search microbench observation checksum | `94739f84565d4d16` (unchanged) |
| trace / debug / HTML replay with a listener, 19 files | byte-identical to the binary at `1e453fa` |
| `ismcts:sims=50`, 2 games, seed 20261010, ten runs | 10/10 clean |
| `ismcts:sims=20`, 8 games, seed 1, ten runs | 10/10 clean |
| checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`, same sources) | suite 2943 passed; OpenSpiel 25; clone-equivalence 10/10; smoke split the same; MCTS and ISMCTS batches clean |

The traces did not change, so there is no diff to explain: nothing in those games reads the state that is now dropped earlier.

## 2. Item closed: 9.LL4 — FIXED

**The rule.** `forgetBoardState` in `src/core/board_state.h` is CR 124.1 for a `GameObject`. Every path off the board ends in `EffectExecutor::leftBoard(id)`, which sheds attachments and then calls it. It runs after the move's events are emitted, so abilities that trigger on the move are queued from the object as it was.

**Paths found.** I read every write that clears a location in `src/` (46 sites). Thirteen take an on-board object off the board:

| Path | Where |
|---|---|
| kill by an effect, unit | `EffectExecutor::killObject` |
| kill by an effect, gear | `EffectExecutor::killObject` |
| kill by lethal damage / death batch | `GameEngine::killUnit` |
| return to hand, card | `EffectExecutor::bounceToHand` |
| return to hand, token (ceases to exist) | `EffectExecutor::bounceToHand` |
| banish | `EffectExecutor::banishObject` (banish-self as a cost comes here too) |
| recycle: card, token, rune | `EffectExecutor::recycleCards` |
| recycle-self activation cost | `GameEngine::payAndQueueAbility` |
| rune recycled for Power | `resource::payIn`, `GameEngine::advanceCostPayment`, the effect-play cost in `effect_executor.cpp`, `recycleRuneForPower` in `card_helpers.h` |
| god-mode move | `StateEditor::moveObject` |
| **card code by hand: Keeper's Verdict 760** | now `EffectExecutor::putIntoDeckFromBoard` |

- Keeper's Verdict was the only card that took an on-board object off the board itself. The other 19 card sites move a card between two off-board zones (trash, deck or banishment to hand, and the like).
- Not leave paths: facedown removal (a facedown card has no location), `replaceBattlefieldWithToken` (a battlefield card has none), and spells leaving the chain.

**What is dropped at the move.**
- Statuses: Stunned, Exhausted, Empowered and its end-of-turn flip, the combat designation.
- Damage, who marked it, and the per-turn damage tally.
- Buffs; "this turn" Might, Assault, Shield, Deflect and keywords; keywords given "for as long as it stays". The printed keywords and values are restored from the card.
- "Can't move" and "can't be chosen" this turn; pending death replacements; granted abilities.
- Control: the controller becomes the owner and both reversion records are cleared.
- Every continuous-effect field and the attachment bonus. Might is the printed Might (CR 711).

**What is kept, and why.**
- `last_known` and `last_location`: the snapshot taken before the kill (and by a death batch before the first of its kills). It is what "When I die" abilities read. The kill tests assert it survives with the stun, buff, Empowered, Might, keywords and attachments of the unit as it died.
- `card_counters`, `string_state`, `tracked_objects`: the cards' own memory, read by abilities that resolve after the move. They are not counters in the rules' sense.
- Identity, the token bit, and facedown state.

**Tests.** One per path, reading the object immediately after the move with no cleanup in between:
`PerfEngineLeaveBoard.{KilledByAnEffect, KilledByLethalDamage, AGearKilledByAnEffect, ReturnedToHand, ATokenReturnedToHandCeasesToExist, Banished, Recycled, PutIntoItsOwnersDeck, ARuneRecycledForPower, MovedOffTheBoardByTheStateEditor, ACleanupFindsNothingLeftToClear}` and `PlayCombatFixTest.RecycleSelfCost_TheCardInTheDeckIsANewObject`.

- Red first: built against the sources before the change, every one of them failed except `PutIntoItsOwnersDeck`, whose executor method did not exist yet. Keeper's Verdict's hand-made move was therefore not run red.
- What each path had forgotten, from that red run: banish forgot Stunned, Exhausted, damage, buffs, Empowered, this-turn Might and keywords, the designation, control and printed Might; kill forgot all of those but damage and the designation; bounce and recycle of a card forgot all but damage, the designation and exhaustion.
- Rune paths: the test drives `resource::pay` and `payAdditionalCost`. The three other rune sites (`advanceCostPayment`, the effect-play cost, `recycleRuneForPower`) make the same one-line call and have no test of their own.

**The aura-pass clearing is removed.** Nothing is left for it. In the checked build `recalculateAuras` throws if, in a game the engine plays, an off-board object still carries board state.
- Under it the suite, the smoke, the MCTS batch and an ISMCTS batch are clean.
- It has teeth: with the call taken out of `killUnit`, the first smoke game fails with the assertion naming the unit.
- It covers engine-played games only, not positions a test builds by hand. Ten existing card tests put an exhausted rune or an attached Equipment in a trash on purpose (to show a card did not ready or count it) and I left them as they are.

## 3. Items not closed

None.

## 4. Needs another stream / trainer notes

- No `shared:` commits.
- Behaviour change: an object that leaves the board loses its buffs, exhaustion, this-turn modifiers and any control change at the move, and its controller becomes its owner. Before, most of that stayed on the card in the hand, deck or trash until something else cleared it, and some of it never was.
- New: `EffectExecutor::leftBoard`, `EffectExecutor::putIntoDeckFromBoard`, `core/board_state.h`.

## 5. New defects found

One risk I did not close, rather than a finding:

- **Card code that reads a card's live fields after it left the board.** A card that kills or bounces something and then reads that object's controller, buffs or Might from the object itself now sees the new object: owner, no buffs, printed Might. It should read `last_known`. No test in the suite broke and the fixed-seed games did not change, but I did not audit the card files for this pattern. `last_known` has no controller field, so "its controller" after a kill of a stolen unit now means its owner.

Otherwise: no new findings.

## 6. How the work was run

- Lane A lock for every build, full-suite run and the checked-build runs.
- Run without the lock: trace captures, single microbench runs, filtered runs of a few tests.
- The red run and the mutation check swapped source files out and back from backup copies; nothing of them is committed.
- No `rm`, no `sh -c`, no `eval`.
