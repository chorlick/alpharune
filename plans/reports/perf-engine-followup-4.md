# perf-engine — fourth follow-up report (9.MM1, 9.MM2, judge rulings of 2026-10-11)

## 1. Result

- Branch `fix/perf-engine`, head `9c439a0`, working tree clean. Verified against `fix/integration` `04155c7` (merged in).
- 9.MM1 and 9.MM2 are fixed and ticked. The six judge rulings are applied: 9.BB4, 9.W2, 9.I7, 9.Z4 and 9.R5 are ticked.
- Clean build (lane A lock, `-j3`, PCH-safe launcher), 0 warnings:
  - `riftbound_tests` 2964 passed, 0 failed (2943 before; 21 new).
  - `riftbound_openspiel_tests` 25 passed; clone-equivalence 10/10.
  - Smoke 200 games seed 20261010: **P1 139 / P2 61 / 0 draws, 17 turns, 293 decisions — unchanged.**
  - Search-microbench observation checksum `94739f84565d4d16` — unchanged.
  - Fixed-seed traces (stdout trace/debug, HTML replay, batch replay): byte-identical to the baseline, so there is no diff to explain.
  - MCTS batch rc 0; fault injection rc 1 as wanted.
  - Checked build (`-DRIFTBOUND_CHECK_BOARD_INDEX=ON`): suite 2964 passed, smoke 139/61/0, no throw.
  - ISMCTS: `sims=50` ×2 games 10/10 clean; `sims=20` ×8 games 10/10 clean.
- The split and traces did not move although rulings 5 and 6 and the simultaneous-death fix change play. The Fiora / Yi decks of the fixed-seed runs contain no Twisted Fate, Void Hatchling or Emperor's Divide, and no game in them had a "another unit dies" watcher die in the same cleanup as the unit it watched.

Commits: `efe579f` (9.MM1 + 9.MM2, one commit because the tests share a file), `9c439a0` (judge rulings). The two merge commits carry no co-author trailer.

## 2. 9.MM1 — what reads a card after it left the board

### What changed

- **`GameObject::LastKnown` has `controller` and `died`.** `controller` is who controlled the object when it left; `died` is false for a return to hand, banish or recycle.
- **Every leave path takes the snapshot first**, not only a kill: `bounceToHand`, `banishObject` (from the board), `recycleCards` (board members), `putIntoDeckFromBoard`, the recycle-self activation cost, as well as `killObject` and `GameEngine::killUnit`. Before, a bounced or banished unit had no snapshot, or still had the one from the last time it died.
- **Every announcement of a leave is made before the object is reset.** `leftBoard` is now the last call in `bounceToHand`, `banishObject`, `recycleCards` and the recycle-self cost. Before, `UnitReturnedToHandEvent`, `"banished_owned"` and `"recycled_main"` were emitted with the new object.
- **`controllerWhenItLeft(state, id)`** in `card_helpers.h` is the reader for "its controller". CR basis: 359.3.e.13 ("can look back at its characteristics before it changes zones"), 383.2.c.3 for the leaving object's own abilities, 124.1 for the new object.
- `CLAUDE.md` and `core/board_state.h` describe the rule.

### Audit table

Scope read: 227 calls of the six leave operations in `src/cards` and `src/engine`; 47 card files with a death, leave, recycle, banish or burn trigger; the `TriggerManager` handlers; `beginDeathBatch`.

| Site | Reads after the move | Verdict |
|---|---|---|
| Kog'Maw, Caustic 190 (Deathknell) | its own live `location`, falling back to `last_location` | **Wrong, fixed.** A Kog'Maw back on the board elsewhere when the ability resolves dealt its 4 there. Now reads `last_known.location`. |
| `bounceToHand`, `banishObject`, `recycleCards`, `putIntoDeckFromBoard`, recycle-self cost | no snapshot taken | **Wrong, fixed.** "Its Might" or "its controller" after a non-death leave read nothing or a stale death snapshot. |
| `bounceToHand` / `banishObject` / `recycleCards` / recycle-self announcements | listeners got the reset object | **Wrong, fixed** (order). |
| `TriggerManager::onUnitDied`, watchers in a death batch | whether the watcher is on the board at that kill | **Wrong, fixed** — see "Units that die together". |
| `TriggerManager::onUnitDied`, "kill a unit with a spell" fan-out | skipped the killed unit itself | **Wrong, fixed.** CR 383.2.c.1's own example: Immortal Phoenix 37 triggers from the trash "even if the unit you killed with a spell was that Immortal Phoenix". |
| Hidden Blade 213, Detonate 328 ("its controller draws 2") | controller, captured before the kill | Right. Converted to `controllerWhenItLeft` so the read is after the move, as asked. |
| Arcane Shift 515, Thrill of the Hunt 745, Temporal Breach 853 | reset damage, buffs, statuses by hand after `banishObject` | Right but redundant; the resets are removed with a comment. Owner is captured before. |
| Decree of Discord 894, Baited Hook 242, Deathgrip 484, Strike Down 429, Fox-Fire 253 | Might, read before the move | Right. |
| Retreat 104, Pack of Wonders 181, Keeper's Verdict 760, Ekko 110, Rift Herald 741, Scuttle Crab 615 | `owner` | Right: owner does not change. |
| Baited Hook 242, Deathgrip 484, Wild Claw 876 | `location` after the kill, as "did it die" | Right: that is the question asked. |
| Zero Drive 412, Reinforce 62, Noxian Guillotine 251, Void Rush 505, Virtuoso 782, `effect_executor.cpp` Smite path | `name`, for a log line | Right: identity is kept. |
| Promising Future 115, Rek'Sai 491, Wild Claw 876 | `isUnit()` | Right: card type is kept. |
| Ashe 731 | `card_counters` | Right: kept by `forgetBoardState`. |
| Blitzcrank 67 | `battlefieldId` | Right: read in another branch, before the bounce. |
| Solari Shrine 72, Vanguard Helm 228, Draven 469, Unsung Hero 488, Loyal Poro 718, Lonely Poro 777, Baccai 865, Noxian Emissary 915 | `last_known` (stunned, killed_by, buffs, designation, Might, empowered, company) | Right. |
| Lonely Poro 359 | `last_location` first | Right. |
| Viktor 246 | dead unit's tags | Right: tags are kept. |
| Altar of Memories 490, Battle Mistress 517, Spectral Centaur 630, Pyke 707, Nasus 850, Treasure Trove 186, Wraith of Echoes 118, Shard of Undoing 736 | the watcher's own on-board state | Right: the watcher did not move. |
| The remaining death-trigger cards (Scrapheap 182, Tasty Faefolk 75, Watchful Sentry 96, …) | only `ctx.controller` | Right: it comes from the event, captured before the move, so a taken unit's Deathknell is its taker's. |
| `TriggerManager::onObjectStateChanged` (`banished_owned`, `burned`, `recycled_main`) | `owner` | Right. |
| `TriggerManager::onUnitDied` equipped Deathknell | `last_known.attachments` | Right. |
| `queueOwnDeathTrigger` | live `hasKeyword(Deathknell)` | Right: runs inside the death event, before `leftBoard`. |
| `GameEngine::killUnit` and its three callers | nothing after the kill | Right. |

### Units that die together (`beginDeathBatch`)

- **Snapshots.** `beginDeathBatch` snapshots every listed unit that is on the board before the first kill and stamps it. Each kill then keeps that snapshot if the stamp still matches. So `leftBoard` for the first unit runs after every unit's snapshot, and a later unit's death ability reads the earlier ones from `last_known`, not from the reset objects. This was already right; it now includes the controller.
- **Defect found: they triggered on each other by table order.** The kills are sequential, so a unit killed second was still on the board when the first died and put its "when another friendly unit dies" ability on the chain. CR 383.2.c.2 says it must not, with this exact example: Viktor, Leader "does *not* trigger if Viktor and another non-Recruit unit you control die during the same game action". Killed in the other order, nothing triggered.
- **Fix.** `beginDeathBatch` returns a mark; `endDeathBatch(units, mark)` takes back the pending "another unit dies" abilities (`WhenAFriendlyUnitDies`, `WhenAnEnemyUnitDies`, `WhenAnEnemyUnitDiesHere`) queued during the batch whose source and subject both left in it. A unit's own "When I die" stays (CR 383.2.c.3). A listed unit that survived keeps its triggers. If that empties a chain that was empty before the batch, the Open state is restored.
- `[Temporary]` kills at the start of the Beginning Phase are deliberately not a batch: Temporary is a triggered ability of each unit (CR 816.1), so each is its own game action. Commented at the site.

### Tests

All in `tests/test_review_fix_perf_engine.cpp`, fixture `PerfEngineLookBack`, on the engine path, unless noted.

Red before (one build with the engine and card fixes reverted, the new field kept so it compiles):
- `ATakenUnitThatIsKilled_ItsControllerIsWhoHadItThen`
- `HiddenBlade_TheControllerOfATakenUnitDraws`
- `EveryWayOffTheBoardRecordsTheUnitAsItLeft` (buffed, stunned, armed, taken unit; return to hand, banish, recycle, onto deck; includes the stale-snapshot case)
- `KogMaw_BackOnTheBoardElsewhere_StillDealsWhereItDied`
- `UnitsThatDieTogether_EachDeathAbilityReadsBothAsTheyWere` — the batch test asked for: one unit taken from P2, one buffed, stunned and carrying a Long Sword; run through a cleanup and through a card-style `DeathBatch`; asserts what each ability saw of itself and of the other.
- `UnitsThatDieTogetherDoNotSeeEachOtherDie`, `UnitsThatDieTogether_NothingIsLeftOnTheChain`
- `Viktor_DyingWithAnotherUnitPlaysNoRecruit` (red in one of the two kill orders, as expected)
- `ImmortalPhoenix_KilledByItsOwnersSpellTriggersFromTheTrash`
- `ALeaveIsAnnouncedBeforeTheObjectIsANewOne`
- `PlayCombatFixTest.RecycleSelfCost_IsAnnouncedBeforeTheCardIsANewObject` (in `test_review_fix_play_combat.cpp`)

Pin (green before): `AUnitKilledAtABattlefield_ThereIsWhereItWas` — a kill already recorded the location.

`HiddenBlade_…` is red only because the converted card was run against an engine that did not record the controller; the original card captured it before the kill and was right.

## 3. 9.MM2 — rune-recycle sites and Keeper's Verdict

Each test was red with the call removed at its site.

| Site | Test | Note |
|---|---|---|
| `GameEngine::advanceCostPayment` | `ARuneRecycledToPlayACardIsANewObject` | A card with a Power cost played from hand, paid from a stunned, damaged rune. |
| effect-play cost (`plainPay` in `effect_executor.cpp`) | `ARuneRecycledByAnEffectPlayWithoutAnEngineIsANewObject` | **Not reachable from an engine**: `playPayingCost` uses it only when no engine has given the executor its payment callbacks. The test builds a bare executor. |
| `recycleRuneForPower` in `card_helpers.h` | none | **It had no caller** anywhere in `src/` or `tests/` (cards pay through `payOnePower` → `resource::pay`). Removed, so there is no such site. |
| Keeper's Verdict 760 | `KeepersVerdict_TheUnitOnTheDeckIsANewObject` | Played through the engine; red with `leftBoard` removed from `putIntoDeckFromBoard` (the existing `PutIntoItsOwnersDeck` went red with it too). |

## 4. Judge rulings

The user-decision reverts you sent first (9.BB4 a, 9.W2) were in progress and uncommitted when the stop arrived; I restored the files, so nothing of them is in the history.

| # | Ruling | Done |
|---|---|---|
| 1 | Bare "runes" may be an opponent's | No code change. There is no shared predicate helper, so the comment is at each of the six `readiable` predicates (315, 284, 666, 932, 642, 372). 9.BB4 ticked. |
| 2 | Disposal Order 665, "up to 3" with zero cards | No change; comment at the mode check. 9.W2 ticked. |
| 3 | Curator 932 may exhaust to ready zero | No change; comment at the site. |
| 4 | Hidden Guards! 475 where units can't be played | No change; comment at the site. The existing `Guards_HiddenWhereUnitsCannotBePlayed_PlaysNoSandSoldier` already pinned no prompt and no [Y] spent; it now also asserts nothing is left on the chain. 9.I7 ticked. |
| 5 | Rune Deck is "a deck" for Void Hatchling | **Changed.** `lookBeforeReveal(player, deck = MainDeck)`; for the Rune Deck the look and "may recycle it" come before the reveal. Twisted Fate, Gambler 200 calls it. 9.R5 ticked. |
| 6 | Emperor's Divide 366 from facedown choosing zero | **Changed**, in one place: `generateHiddenPlayActions` offers a hidden spell whose requirement is `optional` once with no targets when its battlefield has none. No card is special-cased. 9.Z4 ticked. |

Ruling 5 details:
- Twisted Fate is the only card that reveals from a Rune Deck. Mobilize 134 and Catalyst of Aeons 138 channel; nothing else in `src/cards` reads the Rune Deck.
- There was no earlier Twisted Fate + Hatchling pin test to flip; I found none under any name.
- Tests in `test_review_fix_verify_cards.cpp`: `TwistedFateGambler_VoidHatchlingLooksAtTheTopRuneFirstAndMayRecycleIt`, `TwistedFateGambler_VoidHatchlingMayLeaveTheTopRune` (both red before), pin `Pin_TwistedFateGambler_NoVoidHatchling_NoLook`.
- The look at a rune is not recorded in the knowledge tracker, which tracks the Main Deck only. Twisted Fate's own reveal is not recorded there either.

Ruling 6 details:
- [Hidden] spells whose text says "any number of" / "up to N" (the 23 spell files with the Hidden keyword were searched): Emperor's Divide 366, Fox-Fire 253, Guerilla Warfare 260.
- Only 366 was refused. 253 and 260 declare no play-time targets and were already offered.
- Tests: `EmperorsDivide_FromFacedownWithNoFriendlyUnitThereIsPlayedChoosingNone` (the old `Pin_EmperorsDivide_FromFacedownNeedsAFriendlyUnitAtItsBattlefield`, first two expectations flipped by the ruling, the rest kept), `EmperorsDivide_PlayedFromFacedownChoosingNone_MovesNothing` (both red before), pins `Pin_FoxFire_PlayedFromFacedownWithNoUnitAtItsBattlefield_KillsNothing` and `Pin_GuerillaWarfare_PlayedFromFacedownWithNoHiddenCardInTheTrash`.
- As from hand, the empty play is offered only when no target set qualifies; with a friendly unit at the battlefield the plays are as before.

## 5. New findings (not fixed, for plans/09)

1. **Replacement effects on simultaneous deaths are applied in kill order.** CR 373.1.a: the replacements' actions "are performed before any simultaneous unmodified events". The engine applies each unit's replacements as that unit is killed, so a unit that died earlier in the batch is already in the trash when a later unit's replacement is checked (the CR 370 example of a trash card that replaces a death shows the difference). A fix cannot go in `beginDeathBatch` as it stands: 21 of the 28 card batches list every damaged unit and kill only those with lethal damage, so the batch does not know who will die.
2. **One snapshot per object.** A card that dies, is played again and dies a second time before its first death ability resolves has its `last_known` overwritten; the first ability then reads the second death. The complete fix is to capture the snapshot on the chain item when the ability triggers.
3. **Rune-recycle sites reset the rune before `LeftBoardEvent`** (`resource::payIn`, `advanceCostPayment`, `plainPay`), the opposite order to the other leave paths, and take no snapshot. Nothing reads a recycled rune's board state (the only listener, "When you recycle a rune", reads none), so no behaviour is wrong today.
