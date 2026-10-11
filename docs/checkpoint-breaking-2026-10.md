# Checkpoint-breaking changes of the 2026-10 review fix run (stage 5)

Everything in the engine that changes what a model reads or writes: the state
feature vector, the action ids, history tokens and one `Intent` field. Nothing
recorded or trained against an earlier engine (a checkpoint, a replay of action
ids, a table keyed by `InfoSetId`) is valid after these commits. The other
engine changes of the run are in `docs/trainer-notes-2026-10.md`.

Review items: 1.9, 1.12, 9.RR2 (`plans/completed/`). The v6 interface
(`core/legal_action.h`, `ml/token_layout.h`) is not built yet; these fixes are
in the encoding the search agents run on today.

| | Before | After |
|---|---|---|
| `ml::kStateFeatureDim` | 6188 | **6192** |
| `ml::kCardVocabSize` / `openspiel::kNumCardDefIds` | 1100 | 1100 |
| `RiftboundGame::NumDistinctActions()` | 13,371 (`kVocabSize`) | **69,988,352** (`kNumActions` = 68,348 slots x 1024 twin planes) |
| search-microbench observation checksum | `94739f84565d4d16` | `efc2aa14b545a08e` (`366dde83e658847e` after the feature change alone) |

## 1. State features (`src/ml/feature_extractor.h`)

### The one ordering convention

**Everything is relative to the perspective player.** Wherever a block has one
entry per player, the perspective player ("self") is first and the opponent
second. A field that names a player is 1 = self, 2 = opponent, 0 = nobody (or
a yes/no "is it self"). No field says Player1 or Player2.

Consequence, tested over the whole vector at every decision of real games
(`FeaturePerspectiveGames.SeatSwapGivesTheSameVectorAtEveryDecision`): the
vector of a position seen from one seat equals the vector of the seat-swapped
position seen from the other seat.

- **The absolute seat is not a feature.** Nothing in the rules depends on it.
- **Who goes first is a feature**: `layout::kGlobalSelfStarts` (index 3), as
  before.
- **Battlefield slots are not reordered**: slot i is battlefield i for both
  players, the same number the action slots use. Which player brought each
  slot to the game is the new block `kBfContributedBy`.

### What changed

| Block | Before | After |
|---|---|---|
| battlefield controller (field 1 of each battlefield header) | 0 none, 1 = Player1, 2 = Player2 | 0 none, 1 = self, 2 = opponent |
| the two 20-float side blocks of each battlefield | Player1, then Player2 | self, then opponent |
| the 96-float per-unit extended block | per battlefield: Player1, then Player2 | per battlefield: self, then opponent |
| `kBfContributedBy`, 4 floats, appended | did not exist | per slot: 0 nobody (token battlefield, empty slot), 1 self, 2 opponent |

Every other block was already relative and is unchanged, value for value.
With the reorder, one (battlefield, side, unit) slot is the same unit in the
base side block, the extended block and the Tier-2 block.

### Layout (offsets are `ml::layout::*`; use the names)

| Offset | Size | Block | Content |
|---|---|---|---|
| 0 | 10 | `kGlobals` | turn number, phase, self is turn player, self is the starting player, score difference (self - opp), chain size, showdown, closed state, additional turn, delayed-ability count |
| 10 | 8 | `kChainItems` | 4 chain slots x {source card id, controlled by self} |
| 18 | 70 | `kSelfPlayer` | the perspective player's block |
| 88 | 70 | `kOppPlayer` | the opponent's block; hand identities and costs are 0 |
| 158 | 196 | `kBattlefields` | 4 slots x 49: header (9), **self side (20), opponent side (20)** |
| 354 | 8 | `kChainTargets` | 4 chain slots x 2 target card ids |
| 362 | 4 | `kCostModTypes` | {next spell only, next unit only}: self, opp |
| 366 | 8 | `kScoredFlags` | 4 slots x {scored by self, by opp} this turn |
| 374 | 96 | `kUnitExt` | 4 slots x **{self, opp}** x 3 units x {combat designation, attachment card id, Might delta, temporary Might} |
| 470 | 5500 | `kZones` | 5 x 1100 card counts: self Main Deck, self trash, self banishment, opp trash, opp banishment |
| 5970 | 2 | `kPlayLockout` | can't play cards this turn: self, opp |
| 5972 | 192 | `kUnitT2` | 4 slots x {self, opp} x 3 units x {assault, shield, deflect, buffs, temporary buffs, Tank, Backline, Ganking} |
| 6164 | 24 | `kExtGlobals` | ready base units (self, opp); cost-modifier sum (self, opp); additional turns (self, opp); contested by self x4; combat staged x4; showdown staged x4; facedown age x4; focus holder (1 self / 0 opp / -1 none); battlefield count |
| 6188 | 4 | `kBfContributedBy` | **new**: who brought each battlefield slot |

- Battlefield header, 9 fields: card id, controller, contested, combat in
  progress, showdown in progress, facedown count, self is attacker (-1 outside
  combat), combat phase, token battlefield.
- Side block, 20 fields: unit count, total Might, total damage, exhausted,
  stunned, Tank, Backline, Ganking, Assault, Shield, Deflect counts (11); then
  the top-3 units by Might: card ids (3), Might (3), damage (3).
- Helpers: `layout::bf(slot)`, `layout::bfSide(slot, side)`,
  `layout::unitExt(slot, side, unit)`, `layout::unitT2(slot, side, unit)`,
  `layout::zone(k)`; side 0 is self.
- `extractStateFeatures` throws `std::logic_error` if a block does not start
  at its `layout` offset.

### Info-set keys

`RiftboundState::InfoSetId(p)` is built as before: the hash of p's own vector,
mixed with p's seat, with who is deciding and, for the decider only, with the
action ids on offer. `ObservationString(p)` is its hex. Every value changed
(new vector, new action ids). The key is a function of what p knows: unchanged
by any re-deal of the cards p has not been shown
(`RiftboundStateTest.InfoSetKeyIsAFunctionOfWhatThePlayerKnows`).

## 2. Action ids (`src/openspiel/action_vocab.h`)

### What an id is now

```
id   = slot + kNumSlots * twin          kNumSlots = 68348, kTwinPlanes = 1024
slot = actionIdSlot(id)                 what the action is made of
twin = actionIdTwin(id)                 which of the legal intents with that description
```

- **No two distinct legal intents of a decision share an id, and every legal
  intent has one.** Before, an id was a (coarser) slot and only the first
  intent of a slot could be chosen: in random games 39% of the decisions lost
  at least one option and 44% of the legal intents were unreachable.
- **The slot** (`actionSlot(intent, state)`) needs no legal list and means the
  same in every position: "play card 250 to battlefield 1", "choose the
  opponent's card 300", "answer 1".
- **The twin number** tells apart intents with one slot: two copies of a card,
  a spell offered once per target, a prompt offered once per rune. The intents
  of a decision that share a slot are ordered by `compareIntents` (field by
  field; objects by object id) and numbered 0, 1, 2, ... The first has
  `id == slot`.
- **What the order does not depend on**: the order the generator listed the
  options in, and anything the deciding player cannot know. So in every deal
  of the hidden cards that the player cannot tell apart, the same ids are on
  offer and each means the same choice. Both are checked at every decision of
  146 random games (`tests/test_action_ids.cpp`).
- **What a twin number is not**: stable across positions. It says "another
  action with this description"; two units of one card that swap places swap
  twin numbers. A policy over ids should treat plane k > 0 that way, or score
  slots and break ties itself. Per-legal-action scoring (v6) has no such limit.
- `kTwinPlanes` bounds the intents that can share a slot. The most the walk
  meets is 64. Past the bound `actionIds` gives `kNoActionId` and
  `RiftboundState::LegalActions()` throws `std::logic_error`.

### Slot ranges

`kCardKeys` = 1101: key 0 is "an object with no card id" (a token without a
card class), keys 1..1100 are card ids. `kPlaceKeys` = 8: 0 = none or a base,
1..7 = battlefield slot + 1.

| Verb | First slot | Size | Key |
|---|---|---|---|
| `Reserved` | 0 | 1 | never an action |
| `EndTurn`, `PassPriority`, `PassFocus`, `Concede` | 1, 2, 3, 4 | 1 each | |
| `PlayFirstDecision` | 5 | 2 | 0 second, 1 first |
| `MulliganDecision` | 7 | 38 | 0 keep; 1 + p: hand position p; 9..36: a pair of positions (i < j), row by row; 37: anything else |
| `ChooseBattlefield` | 45 | 8 | position in the player's battlefield list |
| `AssignCombatDamage` | 53 | 16 | number of units given damage (15 = 15 or more) |
| `StandardMove` | 69 | 8808 | unit card x 8 + destination |
| `Play` | 8877 | 35232 | ((card x 8) + where it lands) x 4 + variant: 0 normal, 1 alternative cost, 2 location cost, 3 not from hand or the Champion Zone |
| `HideCard` | 44109 | 8808 | card x 8 + battlefield |
| `ActivateAbility` | 52917 | 4404 | source card x 4 + ability index (own ability, no target in the intent) |
| `ActivateAtTarget` | 57321 | 1101 | source card (the intent names its target: Equip) |
| `ActivateGranted` | 58422 | 1101 | the card that grants the ability |
| `ChooseOwnObject` | 59523 | 1101 | card of the first chosen object, controlled by the decider |
| `ChooseOpposingObject` | 60624 | 1101 | ... controlled by the opponent |
| `ChooseNamedCard` | 61725 | 1101 | the card named |
| `ChooseValue` | 62826 | 17 | the integer answer 0..15; 16 = any other value |
| `PlaceOptionalTrigger`, `DeclineOptionalTrigger`, `PayTriggeredCost`, `DeclineTriggeredCost` | 62843, 63944, 65045, 66146 | 1101 each | source card |
| `SideboardSwap` | 67247 | 1101 | card taken out |

### Every range that moved or was added

| Before (first slot, size) | After | What changed in meaning |
|---|---|---|
| singletons 0..4, `PlayFirstDecision` 5 (2) | same | nothing |
| `MulliganDecision` 7 (3): how many cards | 7 (38) | which hand positions |
| `ChooseBattlefield` 10 (8): position + 1 | 45 (8) | the position itself (`Intent::battlefield_pool_index`) |
| `AssignCombatDamage` 18 (128): a hash of targets and amounts | 53 (16) | number of units hit; the distribution is the twin |
| `StandardMove` 146 (8): destination | 69 (8808) | unit card and destination |
| `Play` 154 (1100): card | 8877 (35232) | card, location, variant |
| `HideCard` 1254 (1100): card | 44109 (8808) | card and battlefield |
| `ActivateAbility` 2354 (4400): card x 4 + index, the index replaced by a hash of the target's card for a gear | 52917 (4404) | the index is always the index |
| | `ActivateAtTarget` 57321 (1101) | new: Equip |
| | `ActivateGranted` 58422 (1101) | new: a granted ability, by granting card (it shared the source's slot) |
| `MakeChoice` 6754 (1117): 0 no object or no card id, 1..1100 card of the first object or the named card, then 16 values | `ChooseOwnObject` 59523, `ChooseOpposingObject` 60624, `ChooseNamedCard` 61725 (1101 each), `ChooseValue` 62826 (17) | whose object; a named card apart from an object; a slot for values outside 0..15 |
| trigger verbs 7871, 8971, 10071, 11171 and `SideboardSwap` 12271 (1100 each) | 62843 .. 67247 (1101 each) | key 0 added; none of these intents is generated today |

An object with no card id used to take the first slot of its verb's range
(card 1's slot for a move, a play or an activation; the "no object" slot of a
prompt). It now has key 0 of every card-keyed range.

### API

| Before | After |
|---|---|
| `encodeAction(intent, state)` -> the action id | `actionSlot(intent, state)` -> the slot only |
| | `actionIds(legal, state)` -> the id of every intent of a legal list, parallel to it |
| | `actionIdOf(intent, legal, state)`, `actionIdSlot(id)`, `actionIdTwin(id)`, `makeActionId(slot, twin)`, `slotVerb(slot)`, `compareIntents(a, b)` |
| `decodeAction(slot, legal, state)` -> the first intent with that slot | `decodeAction(id, legal, state)` -> the one intent with that id |
| `kVocabSize` | `kNumSlots`, `kTwinPlanes`, `kNumActions` |
| `RiftboundState::LegalActions()`: one id per slot | one id per distinct legal intent (an intent the engine lists twice is one) |
| `RiftboundState::actionHistory()`: the ids applied | the **slots** of the intents applied; the ids of a game driven through `ApplyAction` are OpenSpiel's `History()` |

To apply an intent chosen from `legalIntents()`: `ApplyAction(actionIds(legalIntents(), engineState())[i])`.

## 3. History tokens (`GameState::action_history`)

A token is still `slot | fingerprint << 20`:

- bits 0..19: the slot of the chosen intent (`historyTokenSlot`; always a
  slot now, there is no "no slot" value);
- bits 20..62: a 43-bit fingerprint of every field `Intent::operator==`
  compares, object ids included, now with `battlefield_pool_index`.

**Every token value changed** (new slots, one more fingerprinted field). A
token does not hold the twin number: the fingerprint is what identifies the
intent in its legal list, and working the twin out for every decision of every
game cost 4% of random play. Replay is unchanged: `findHistoryToken`, lazy
`Clone()` and `syncToHistory` match whole tokens.

## 4. `Intent`

- **New field `Intent::battlefield_pool_index`**, of the new id kind
  `BattlefieldPoolIndex` (`core/types.h`): the setup decision
  `ChooseBattlefield` says which card of the player's battlefield list
  (`PlayerState::battlefield_pool`) they bring. It used to travel in
  `chosen_battlefield`.
- `Intent::chosen_battlefield` now means one thing: the battlefield slot a
  card is hidden at (`HideCard`).
- `Intent::chooseBattlefield(player, BattlefieldPoolIndex)`; `operator==` and
  the token fingerprint include the field.
- `ml::featurizeAction` (25 floats) is unchanged in layout and values.

## 5. What did not change

- Games played by agents that use neither the features nor the action ids:
  the 200-game random smoke (fiora vs yi_rebuild, seed 20261010) is P1 139 /
  P2 61 / 0 draws, 17 turns, 294 decisions, and 19 fixed-seed trace and replay
  files are byte-identical to the build before the stage.
- Card behaviour and rules.

## 6. What does change for the search agents

`MctsAgent` and `IsMctsAgent` now see every legal option. Fixed-seed batches
play different games than before; each is still a function of its seeds (run
twice, same output).

| Batch (fiora vs yi_rebuild, agent is P1, opponent random) | Before | After |
|---|---|---|
| `mcts:sims=50`, 5 games, seed 20261010 | P1 4 / P2 1, 296 decisions per game, 2.9 s | P1 5 / P2 0, 232 decisions per game, 2.8 s |
| `ismcts:sims=20`, 8 games, seed 1 | P1 5 / P2 3, 274 decisions per game, 2.1 s | P1 4 / P2 4, 290 decisions per game, 2.9 s |

Clone and `ApplyAction` cost per call is unchanged
(`riftbound_clone_advance_microbench`); the batch times differ because the
games and the trees do. Five and eight games say nothing about strength.

## 7. Checklist for the trainer

1. Size nothing by `NumDistinctActions()` (70 million). Score legal actions,
   or use `kNumSlots` (68,348) for a per-description head and the twin number
   as a tie-break.
2. Replace `encodeAction` with `actionIds` (ids) or `actionSlot` (descriptions).
3. Replace feature offsets written as numbers with `ml::layout::*`.
4. Treat the battlefield controller and both per-side blocks as self / opponent.
5. Drop every stored `InfoSetId`, action id and history token.
6. `Intent::chooseBattlefield` takes a `BattlefieldPoolIndex`.
7. `tools/collision_census.cpp` compiles against the new API and should now
   report 0 collisions for any deck.
