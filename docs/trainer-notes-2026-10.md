# Engine changes a trainer developer has to act on (2026-10 review fix run)

What changed in the engine between the trainer's last build (`895abfe`) and the
end of stage 3 of the review fix run, organised by what has to change on the
trainer side. Collected from the stream reports in `plans/reports/` and the
carry-over table in `plans/STATUS.md`; every statement about code was checked
against the tree this file was written on. Line numbers are left out on
purpose: look symbols up by name.

The trainer's own plan is `riftbound-trainer/plans/README.md`; its table
"Engine changes already merged that the trainer will meet" is a subset of this.

**Not in this document:** stage 4 (strong id types: `PlayerId`, `GameObjectId`
and friends become distinct types — mostly a recompile, with explicit casts
where the trainer mixes them with integers) and stage 5 (feature layout and
action vocabulary). Stage 5 writes its own note.

## 1. Build and link

| Change | What to do |
|---|---|
| `scripted_agent.cpp` is part of `riftbound_core`. | Drop it from the trainer's own source list (it is compiled twice today). |
| `src/openspiel/CMakeLists.txt` used `CMAKE_SOURCE_DIR`, which is the trainer root under `add_subdirectory`. Fixed (`PROJECT_SOURCE_DIR`); verified with a throwaway parent project (configure and generate only, not built). | Reconfigure in a fresh build directory. |
| New test binary `riftbound_openspiel_tests` (`src/openspiel/openspiel_tests.cpp`), registered with ctest. It is not part of `riftbound_tests`. | Build and run both when checking an engine commit. |
| `RiftboundGame` no longer owns a card table: every game shares one compiled-in table (`openspiel::sharedCards()`); `cardDb()` / `cardRegistry()` are unchanged. | Nothing, unless the trainer built its own `CardRegistry` + `CardDB` and expected them to be the game's. |
| `MctsAgent` / `IsMctsAgent` ignore their `registry_path` argument (there is no `registry.json`). | Drop the `"cards/registry.json"` argument's meaning from the trainer's agent factory; any string works. |
| `mcts_agent.cpp` is compiled by path into trainer targets. It is still one file; nothing was split out of it. | Nothing. |
| ccache: the launcher must be `scripts/ccache_pch_safe.sh`, never plain `ccache` (a cached precompiled header can be stale; see `CLAUDE.md`, "ccache"). CMake warns at configure time. | Use the script if the trainer build sets a compiler launcher for the engine subdirectory. |
| Decks: `decks/` holds only `fiora.txt` and `yi_rebuild.txt`. | The trainer's four copied deck lists must be re-checked for legality against current card data (tags, token super types and bans changed; see section 9). |

## 2. The step API (`GameEngine`)

Signatures as they are now (`src/engine/game_engine.h`):

```cpp
const StepResult& beginGame(const DeckSubmission& deck1, const DeckSubmission& deck2, uint64_t seed = 0);
const StepResult& resumeFromSnapshot(GameState snapshot_state, const std::mt19937_64& rng_state,
                                     const StepResult& snapshot_step);
const StepResult& currentStep() const;
const StepResult& applyChoice(int legal_index);
static bool isResumableDecision(const GameState& state, const std::vector<Intent>& legal);
const std::mt19937_64& rngState() const;
```

- **The four step functions return references**, not copies. The referenced
  `StepResult` is overwritten by the next applied choice. `auto step =
  engine.currentStep();` still copies; `const auto& step = ...` held across an
  `applyChoice` is stale. The same holds for `RiftboundState::legalIntents()`
  (`const std::vector<Intent>&`).
- **`StepResult::perspective` is the player being asked**, not the turn player
  (it used to be the turn player: wrong for the second mulligan, chain
  priority, showdown focus, combat damage and an opponent's trigger choices).
- **`applyChoice` throws `std::out_of_range`** on an index that is not one of
  the legal actions (it used to apply action 0), and `std::logic_error` before
  `beginGame`.
- **Only a top-level main-phase decision can be resumed.**
  `isResumableDecision` is true only when the legal list offers `EndTurn`.
  `resumeFromSnapshot` throws `std::invalid_argument` for anything else and
  `std::logic_error` if the resumed engine does not ask the snapshotted
  question. It now takes the snapshotted engine's RNG (`rngState()`): the
  shuffle stream is part of the position.
- **An engine exception is not a finished game.** The fiber keeps the
  exception; `beginGame` / `applyChoice` / `currentStep` rethrow it,
  `stepFailed()` reports it, `isStepDone()` is true and
  `stepResult().termination_reason == "engine_error"` with no winner.
  A trainer `terminationKind` that has no case for `"engine_error"` files such
  a game as a draw: add the case and count it as a failure.
- **Engine bounds throw.** A card that never stops asking hits a 1024-prompt
  bound in the chain's resume loop or in the finalize pass and throws
  `std::logic_error` naming the card. `--games N` batches exit 1 with the game
  number and seed.
- **Destroying an engine mid-game is cheap**: the parked fiber is unwound
  instead of the game being played out.
- **`GameState::action_history`** holds one history token per applied decision
  (`openspiel::historyToken`: the vocabulary slot plus a fingerprint of the
  whole Intent), not a bare slot. Replay matches tokens (`findHistoryToken`).
  Code that read it as a list of action ids must go through
  `RiftboundState::actionHistory()` (below).
- **New**: `ml::extractStateFeatures(state, perspective, db, out)` writes into
  a caller-owned buffer; the returning overload is unchanged.

## 3. The OpenSpiel wrapper (`RiftboundState`)

- `ApplyAction` throws `std::invalid_argument` for an action id no legal
  Intent encodes to (it used to apply action 0), and `std::logic_error` on a
  finished game.
- `LegalActions()` is sorted and deduplicated by slot. `legalIntents()` is in
  the engine's order; map a chosen Intent to its encoded id to apply it.
- `CurrentPlayer()` is the deciding player (see `perspective`, above).
- `actionHistory()` returns `std::vector<int64_t>` **by value**: one slot per
  decision, -1 where the Intent had no slot.
- `makeFromSnapshot(game, engine_seed, snap_state, snap_step, snap_result,
  rng_seed)` gained the last parameter.
- `Clone()` is lazy. A clone taken at a resumable decision resumes from the
  snapshot; any other clone is rebuilt from the nearest resumable ancestor by
  replaying history tokens, and the rebuilt engine is checked against the
  snapshot (a mismatch throws `std::runtime_error`).
  `decisionsReplayedToBuild()` reports the replay length.
- `InfoSetId` / `ObservationString` **values changed**: the id now folds in
  who decides and, for the decider, the legal action set
  (`ml::mixDecisionIntoInfoSetId`). Anything keyed on old ids is invalid.
  `ObservationTensor` values and the layout did not change.
- `IsTerminal()` is true at `MoveNumber() >= kMaxGameLength` (600,
  `riftbound_game.h`). The trainer's `kSearchMoveCap = 600` is a copy of it:
  use the constant.
- A failed engine makes `IsTerminal` / `Returns` / `Clone` rethrow instead of
  scoring a draw.

## 4. Search agents

- `MctsAgent` and `IsMctsAgent` keep a mirror engine built from the same decks
  and **the live engine's seed**, synced by action history
  (`RiftboundState::syncToHistory`). The `engine_seed` constructor argument
  must equal the seed given to `runGame`.
- If the live game cannot be reproduced from its history (wrong seed, a state
  edited outside the engine), the agent searches from an approximate snapshot
  at main-phase decisions and answers **uniformly at random** at every other
  decision (mulligan, chain priority, showdown, combat, payment, prompts) for
  the rest of that game. This is by design; a trainer that passes the wrong
  seed gets a much weaker agent, silently. Covered by
  `MctsAgentTest.StillPlaysLegalMovesWhenGivenTheWrongEngineSeed`.
- Both agents score a leaf with a score-difference heuristic
  (`RiftboundHeuristicEvaluator`), not a random rollout. The `--help` text of
  the `riftbound` binary still says `RandomRolloutEvaluator`.
- Earlier MCTS numbers were taken while the wrapper reported the turn player
  as the decider (review items 1.1, 1.2) and measured nothing. Whether MCTS now
  improves with more simulations is the first real test.
- **`IsMctsAgent` no longer runs OpenSpiel's `ISMCTSBot`.** The bot iterated
  absl hash maps for its tie-breaks and its final policy, and absl's iteration
  order depends on addresses and a per-thread counter, not on the bot's seed:
  with fixed seeds a batch played different games on every run, and two games
  in one process differed (review item 9.E2; a seeded 6-game batch gave four
  different results in four runs, and one result in four runs with
  address-space randomisation switched off). The agent now runs the same
  algorithm (`IsMctsSearch` in `agents/mcts_agent.cpp`: UCT, a Clone per
  simulation, nodes keyed by (player, `ObservationString`), final move sampled
  by visit count) on ordered containers. **Same decks, engine seed, search
  seed and opponent now give the same game**, for `ismcts` as for `mcts`
  (`IsMctsAgentTest.TheSameSeedsPlayTheSameGame`,
  `MctsAgentTest.TheSameSeedsPlayTheSameGame`). ISMCTS numbers recorded before
  this are not reproducible and its play is not move-for-move what the bot's
  was (same algorithm, different random stream).
- The trainer compiles `mcts_agent.cpp` by path: it no longer includes
  `open_spiel/algorithms/is_mcts.h`, and needs nothing new.
- If the trainer uses `ISMCTSBot` (or any absl-hash-map-iterating OpenSpiel
  algorithm) directly, it has the same irreproducibility.
- The search is still clairvoyant: each simulation's world is a clone of the
  real position, hidden cards included. Only the node key is masked.

## 5. Decisions per game: prompts

Nothing here changes the action vocabulary: every new question is a
`MakeChoice` in the existing int-coded and object-coded slots. What changes is
**how many decisions a game has, who answers them, and when** — every baseline
shifts, decision counts per game change, and anything that assumed "a play is
one decision" is wrong more often.

Classes of change:

1. **Choices at finalization (CR 355, 383.3).** A triggered ability's "you
   may", its "[cost] to [effect]" payment and its targets are asked when the
   ability is put on the chain, before the opponent can respond, not when it
   resolves. A target that is gone by resolution is a mistarget, not a new
   prompt.
2. **Triggers that choose.** 26 cards whose triggered effect silently did
   nothing (no target was ever supplied) now ask their controller.
3. **Additional costs before payment (CR 356.2).** "As an additional cost to
   play me, kill a friendly unit", "you may spend a buff ... ignore this
   spell's cost", "you may discard 1 ..." are asked after the card left its
   zone and before its cost is paid: 2, 146, 150, 207, 208, 231, 367, 440, 481,
   540, 728, 730, 732, 740.
4. **The opponent answers.** Several prompts are answered by a player who is
   not the spell's controller: King's Edict 237 (the opponent picks the unit to
   be killed), Cull the Weak 209 and Divine Judgment 244 (each player in turn),
   Shakedown 33 (the targeted unit's controller), Keeper's Verdict 760 (the
   unit's owner picks top or bottom), Whirlwind 187, Party Favors 71.
5. **Standard Move.** With other eligible units at the same origin, a move asks
   one follow-up `MakeChoice` per unit that may join (first option "Move no
   more units"). The move Intent is still one per (unit, destination).
6. **Where an effect-played unit or token goes.** Cards that play a unit or a
   token with no printed location ask (base or a battlefield the player
   controls); a battlefield where units can't be played is not an option. No
   prompt when the base is the only place.
7. **Keyword abilities.** [Vision] asks its controller once per instance
   (it used to recycle a spell automatically); [Weaponmaster] asks the "may"
   and the Equipment at finalization, also for a token that has the keyword
   from a continuous effect (Sand Soldiers); Zilean's token doubling asks the
   "may" and where the copy goes.
8. **Predict N** asks one ordering prompt per kept card beyond the first
   ("put on top").
9. **Repeat.** Square Up 579 and Curtain Call 743 now offer their printed
   Repeat costs (a discard; three separate costs). A granted Repeat (The
   Academy 772, Temporal Portal 401) charges the spell's Power as well as its
   Energy, so it is offered less often.
10. **Equip.** An Equip the player cannot pay is no longer offered; an attached
    Equipment's own Equip is not offered (CR 718.2).
11. **"Any number" prompts** are a sequence of object prompts with a first
    option that stops (value 0).
12. **Fewer no-op actions.** Sun Disc 21 puts nothing on the chain while
    unarmed; Sett 543 is not activatable with no buff to spend; a reaction play
    that adds no chain item is not offered again in that priority window; a
    spell with fewer legal targets than it needs is not offered.

Prompts are not labelled for the agent: `StepResult` has no `prompt` field
(the trainer's request, `plans/README.md` section 4). The human-readable label
is `Intent::choice_label`; the trace line that precedes a target prompt
(`TGT_PROMPT` / `TGT_PAIR_PROMPT`) can now be private to one player (section 7).

The per-card list is Appendix A.

## 6. Observation

`kStateFeatureDim` (6188), `kCardVocabSize` (1100), the feature layout and the
action vocabulary are **unchanged** through stage 3.

State the engine now tracks that is **not** in the flat feature vector and
not in `src/ml/token_layout.h` — candidates for the v6 token layout. None of it
is hidden information unless noted.

| Field | Type | Meaning |
|---|---|---|
| `TurnState::victory_score_bonus` | `int` | Continuous change to the Victory Score, rebuilt each aura pass. Read `GameState::effectiveVictoryScore()`. |
| `TurnState::turn_decisions_p1` / `_p2` | `int` | Decisions each player has been asked this turn (reset each turn). A resumed engine seeds its per-turn main-phase action count (cap 500) from the turn player's. |
| `PlayerState::units_enter_ready_this_turn` | `bool` | "Units you play this turn enter ready." |
| `PlayerState::unit_died_this_turn` | `bool` | A unit this player controlled died this turn. |
| `PlayerState::buff_might_bonus_this_turn` | `int` | "Buffs give an additional +N [M] this turn." |
| `PlayerState::hide_ignoring_costs_this_turn` | `bool` | "You can hide cards ignoring costs this turn." |
| `PlayerState::units_enter_ready_by` | `std::vector<GameObjectId>` | Sources of "other friendly units enter ready" (derived each aura pass). |
| `PlayerState::chosen_champion` | `GameObjectId` | The Chosen Champion object. |
| `PlayerState::enemy_choices_this_turn` | `int` | Times this player chose an enemy unit or gear with a spell or unit ability this turn. |
| `PlayerState::grant_units_lone_enemy_bf`, `grant_friendly_units_open_bf`, `units_play_base_only` | `bool` | Play-location grants and restrictions (derived). |
| `PlayerState::may_look_at_opp_facedown_turn` | `int` | Turn on which the player may look at the opponent's facedown cards. What they see is hidden information for the other observer. |
| `PlayerState::recall_all_on_attacker_tie` | `bool` | Combat-tie rule change (derived). |
| `RunePool::unit_energy`, `spell_energy`, `showdown_energy`, `spell_power`, `gear_power` | `int` | Restricted resources ("Use only to play spells", ...). Spent before unrestricted ones, only for the matching purpose. `unit_energy` predates the run and is not a feature either. |
| `BattlefieldState::multi_move_tax` | `int[2]` | [A] per unit beyond the first in one Standard Move, per player. |
| `BattlefieldState::conquered_from` | `std::optional<PlayerId>` | Who controlled it before this turn's conquer (with `conquered_on_turn`, `conquered_by_player`). |
| `BattlefieldState::opp_hidden_unrevealable` | `bool[2]` | Per player: hidden cards here can't be played (was one bool). |
| `BattlefieldState::blocks_unit_play`, `reaction_tax_power` | `bool`, `int` | "Units can't be played here"; Mystic Vortex's [A] tax (derived). |
| `GameObject::cant_move_this_turn` | `bool` | Stops Standard Moves and its controller's effect moves until the Expiration Step. |
| `GameObject::aura_cant_move_to_base` | `bool` | Derived each aura pass. |
| `GameObject::banish_if_would_die_this_turn` | `bool` | Smite. |
| `GameObject::temp_deflect_value` | `int` | [Deflect N] "this turn". |
| `GameObject::pending_death_replacements` | `std::vector<PendingDeathReplacement>` | One-shot "next time it would die this turn" replacements on the unit. |
| `GameObject::last_known` (`killed_by`, `other_friendly_units_here`, `text_copies`, ...) | struct | Last-known information of an object that left the board. |
| `GameObject::card_counters["__facedown_play_bf"]`, `["__facedown_play_turn"]`, `["__at_might_threshold"]`, `["__defend_attacker_id"]` | `int` entries | Engine bookkeeping stamped on objects. |
| `ChainItem::keyword_ability` | `std::optional<Keyword>` | The chain item is a keyword's triggered ability (Vision, Weaponmaster), not a card's. |
| `ChainItem::triggering_subject` | `GameObjectId` | The event's object; set for delayed abilities too. |
| `DelayedAbility::repeats`, `expires_on_turn` (`kThisTurn` = -1, `kUntilFired` = -2) | `bool`, `int` | Only `delayed_abilities.size()` is a feature. |

Also: `TriggerType` gained values **appended at the end** of the enum
(`effect_types.h`); anything that serialises trigger types by number keeps its
old numbers.

`GameState::knowledge` (the v6 tracker) is unaffected by the run except that
more paths report to it: cards put into a hand by an effect, reveals shown to
both players (`revealed_to_all`), and the three card files that edited a Main
Deck directly now go through `deck_ops`. The only direct `main_deck` edits left
outside `deck_ops.cpp` are in `game_engine.cpp` (setup), `io/state_editor.cpp`
and `rules/deck_validator.cpp` — the trainer's `test_knowledge_gate` allow-list
still matches.

## 7. Log and event stream

- **`LogEvent::audience`** (`LogAudience`, `core/events.h`) says which players
  may read a line: both, one, or neither. `e.visibleTo(player)` /
  `e.isPublic()`. Draws, mulligans, private looks at a deck and prompts whose
  options name hidden cards are private. **A per-observer action history built
  from the log must filter on it**; a consumer that ignores it leaks hands.
  This is the hook the trainer's "per-observer action history" request needs.
- `TGT_PROMPT` / `TGT_PAIR_PROMPT` trace lines can be private to the player
  asked.
- Tokens announce a play: `TokenCreatedEvent` now drives "When you play a
  unit / a gear" triggers, so more chain items follow a token's creation.
- New string-keyed `ObjectStateChangedEvent` values: `"recalled"`,
  `"buff_spent"`, `"recycled_main"` (also for the mulligan and recycle-self
  costs), `"attached"` / `"equipped"` from one attach path.
- `StateSettledEvent` is emitted at the end of every cleanup.
- A recall is not a move: `UnitMovedEvent` is no longer emitted for "recall
  it" effects and replaced deaths.

## 8. Failures

- An engine or agent exception in `GameRunner` / `BatchRunner` fails the batch
  (exit 1, game number and seed printed); it is not counted as a draw.
- The trainer's match runner aborting on any exception is consistent with
  that. Its `terminationKind` needs the `"engine_error"` case (section 2).
- `RIFTBOUND_STEP_FAULT_AT=<n>` makes the step machine throw at decision `n`:
  use it to test the trainer's failure path.

## 9. Card data

- **Keywords on a def are the card's own** (review item 8.8). Dozens of defs
  lost keywords that their text only mentions or that belong to an ability:
  `Keyword::Reaction` on the Seals, Gold, and every [Ambush] unit; `Action` on
  permanents with an [Action] ability; `Assault` / `Shield` / `Tank` /
  `Temporary` / `Deflect` / `Ganking` on spells, legends and battlefields that
  give the keyword to something else; and their `assault_value` /
  `shield_value` / `deflect_value`. `--dump-registry` output changes
  accordingly. Anything on the trainer side that derived card features from
  def keywords (rather than from the card id) sees different values.
- Token reference cards carry `SuperType::Token` (Gold 326 / 564, Reflection
  566, Bird 559, Sprite 569, Brush 561, Baron Pit 558).
- Karma, Channeler 235 has tags `{Karma, Ionia}` (it was `{Vi, Ionia}`, which
  made it a legal Chosen Champion for a Vi legend). The gallery data says "Vi"
  for that printing; the printed card was not checked — an open question for
  the owner.
- Allay, Eager Admirer 603 is not a champion.
- Hundreds of cards changed behaviour. No old baseline is comparable; the
  fiora-vs-yi random smoke split moved from 128 / 72 to about 137 / 63 over the
  run.

## 10. Checklist for the trainer

1. Fresh build directory against the final engine commit; build
   `riftbound_tests` and `riftbound_openspiel_tests` there once.
2. Drop `scripted_agent.cpp` from the trainer's sources; drop the registry
   path's meaning.
3. `terminationKind`: add `"engine_error"`; treat as failure.
4. Replace `kSearchMoveCap = 600` with `openspiel::kMaxGameLength`.
5. Audit every `const auto&` bound to `currentStep()` / `applyChoice()` /
   `legalIntents()`.
6. Pass the live engine seed to search agents; assert `MctsAgent` never falls
   back in a baseline run (wrong-seed games are silently weaker).
7. Filter log lines by `LogEvent::audience` wherever a log line reaches an
   observation or a per-seat history.
8. Re-check the four deck lists; re-record every baseline and the collision
   census; re-measure cap hits (600 moves, 500 main-phase actions).
9. Decide with the owner whether stage 5 targets the v6 token layout; section 6
   is the list of state to place.

## Appendix A. Cards whose prompt surface changed

Generated mechanically: for every card file, the prompt helpers its class
calls at the baseline of the fix run (`c7143f1`) and now, comments stripped.
A card is listed when the two differ. It shows **that** a card's decisions
changed and in which direction, not the order of its prompts — read the card
for that. `xN` is the number of call sites, not of prompts at run time.

Reading the columns: `confirmOptional` / `confirmNow` — a yes/no; `pickMode` —
a numbered choice; `pickXAmount` — an X; `pickTarget` / `pickTargetPair` /
`selectObject` / `selectObjectAs` / `chooseObjectNow` — an object (`...As`:
asked of another player; `...Now`: asked while the card is being played, before
payment); `selectAnyNumber` / `pickSeveral` / `splitDamageAmong` — a sequence
of object prompts; `pickEffectPlayLocation` / `pickTokenDest` /
`playTokensChoosingLocations` / `pickMoveDest` — where something goes;
`requestChoice` — a hand-built prompt; `predict` — Predict's keep / recycle
and ordering prompts; `kFinalizeTrigger` / `kFinalizeResolve` /
`kFinalizeActivate` — the choices are made at finalization; `additionalPlayCost`
— asked before the cost is paid; `repeatCosts` — printed Repeat offers;
`needsPlayTimeTarget(Pair)` / `needs_activation_time_target` — one Intent per
card, target asked as a follow-up; `WeaponmasterUnit` — the shared
Weaponmaster "may" and Equipment choice.

Not visible here: prompts that come from the engine or a shared helper without
a call in the card file (the Standard Move follow-ups, [Vision], Zilean's token
copy, the optional additional cost declared as data, Equip payment choices).

| Id | Type | Card | Before (`c7143f1`) | Now |
|---|---|---|---|---|
| 6 | units | Flame Chompers | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation |
| 16 | units | Dangerous Duo | - | kFinalizeTrigger, pickTarget |
| 21 | gear | Sun Disc | kFinalizeTrigger, pickTarget | - |
| 23 | gear | Unlicensed Armory | needs_activation_time_target, pickTarget | confirmNow, needs_activation_time_target, pickTarget |
| 33 | spells | Shakedown | - | requestChoice |
| 36 | units | Vi, Destructive | - | kFinalizeActivate, pickTarget |
| 37 | units | Immortal Phoenix | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation |
| 41 | units | Volibear, Furious | requestChoice | splitDamageAmong |
| 43 | spells | Charm | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickMoveDest, pickTarget |
| 48 | spells | Meditation | confirmOptional | confirmOptional, kFinalizeResolve, pickTarget |
| 51 | units | Solari Shieldbearer | - | kFinalizeTrigger, pickTarget |
| 62 | spells | Reinforce | pickTarget | requestChoice |
| 63 | gear | Spirit's Refuge | - | kFinalizeTrigger, pickTarget |
| 67 | units | Blitzcrank, Impassive | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 76 | units | Yasuo, Remorseful | - | kFinalizeTrigger, pickTarget |
| 82 | units | Whiteflame Protector | - | kFinalizeTrigger, pickTarget |
| 92 | units | Riptide Rex | - | kFinalizeTrigger, pickTarget |
| 94 | spells | Sprite Call | - | pickTokenDest |
| 97 | units | Blastcone Fae | - | kFinalizeTrigger, pickTarget |
| 99 | gear | Garbage Grabber | - | kFinalizeActivate, selectObject |
| 109 | units | Dr. Mundo, Expert | - | requestChoice |
| 112 | units | Kai'Sa, Evolutionary | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 130 | units | Crackshot Corsair | - | kFinalizeTrigger, pickTarget |
| 132 | units | First Mate | - | kFinalizeTrigger, pickTarget |
| 136 | units | Pit Rookie | - | kFinalizeTrigger, pickTarget |
| 146 | spells | Wallop | - | additionalPlayCost, chooseObjectNow, confirmNow |
| 147 | units | Wildclaw Shaman | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, selectObject |
| 150 | units | Kraken Hunter | - | additionalPlayCost, chooseObjectNow, confirmNow |
| 153 | spells | Overt Operation | confirmOptional | requestChoice |
| 157 | units | Udyr, Wildman | pickMode, pickTargetx2 | kFinalizeActivate, pickMode, pickTarget |
| 164 | units | Sett, Brawler | - | kFinalizeActivate |
| 173 | spells | Ride the Wind | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickMoveDest, pickTarget |
| 187 | spells | Whirlwind | requestChoice | offerableChoices, requestChoice |
| 188 | units | Zaunite Bouncer | - | kFinalizeTrigger, pickTarget |
| 191 | units | Maddened Marauder | - | kFinalizeTrigger, pickTarget |
| 194 | units | Nocturne, Horrifying | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickMode |
| 196 | units | Soulgorger | confirmOptional, kFinalizeTrigger, pickTarget | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation, pickTarget |
| 199 | units | Tideturner | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 206 | spells | Back to Back | - | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair |
| 207 | spells | Call to Glory | - | additionalPlayCost, chooseObjectNow, confirmNow |
| 208 | units | Cruel Patron | kFinalizeTrigger, pickTarget | additionalPlayCost, chooseObjectNow |
| 209 | spells | Cull the Weak | pickTarget | requestChoice |
| 212 | gear | Forge of the Future | - | kFinalizeActivate, selectObject |
| 226 | units | Spectral Matron | confirmOptional, kFinalizeTrigger, pickTarget | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation, pickTarget |
| 230 | units | Albus Ferros | kFinalizeTrigger, pickXAmount | pickSeveral |
| 231 | units | Commander Ledros | - | additionalPlayCost, chooseObjectNow, confirmNow |
| 234 | units | Harnessed Dragon | - | kFinalizeTrigger, pickTarget |
| 237 | spells | King's Edict | - | requestChoice |
| 238 | units | Leona, Determined | - | kFinalizeTrigger, pickTarget |
| 242 | gear | Baited Hook | pickTarget | selectObject |
| 244 | spells | Divine Judgment | - | requestChoice |
| 247 | spells | Icathian Rain | - | needsPlayTimeTarget, offerableChoices, requestChoice |
| 248 | spells | Stormbringer | pickTarget | pickMoveDest, pickTarget |
| 250 | spells | Super Mega Death Rocket! | - | confirmOptional, kFinalizeTrigger, pickTarget |
| 253 | spells | Fox-Fire | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, offerableChoices, requestChoice |
| 255 | spells | Dragon's Rage | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, offerableChoices, pickMoveDest, pickTarget, requestChoice |
| 256 | legends | Unforgiven | - | pickMode |
| 257 | spells | Last Breath | - | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair |
| 258 | spells | Zenith Blade | confirmOptional, needsPlayTimeTarget, pickTarget | confirmOptional, needsPlayTimeTarget, pickTarget, requestChoice |
| 260 | spells | Guerilla Warfare | - | requestChoice |
| 263 | spells | Bullet Time | needsPlayTimeTarget, pickTarget, pickXAmount | kFinalizeResolve, pickMode, pickXAmount |
| 264 | legends | The Boss | - | confirmNow |
| 265 | spells | Showstopper | - | pickMoveDest |
| 276 | battlefields | Hallowed Tomb | - | confirmOptional, kFinalizeTrigger |
| 277 | battlefields | Monastery of Hirana | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, selectObject |
| 278 | battlefields | Navori Fighting Pit | - | kFinalizeTrigger, pickTarget |
| 282 | battlefields | Sigil of the Storm | - | selectObject |
| 284 | battlefields | Targon's Peak | - | selectObjectx2 |
| 297 | legends | Radiant Dawn | - | kFinalizeTrigger, pickTarget |
| 309 | spells | Flash | kFinalizeResolve, pickTargetPair | kFinalizeResolve, requestChoice |
| 313 | spells | Recruit the Vanguard | - | requestChoice |
| 325 | spells | Blood Rush | - | repeatCosts |
| 330 | units | Gem Jammer | - | kFinalizeTrigger, pickTarget |
| 331 | units | Sentinel Adept | confirmOptional, kFinalizeTrigger, pickTarget | WeaponmasterUnit |
| 339 | gear | Recurve Bow | - | kFinalizeTrigger, pickTarget |
| 342 | gear | Assembly Rig | - | kFinalizeActivate, selectObject |
| 346 | spells | Piercing Light | - | repeatCosts |
| 349 | units | Rumble, Hotheaded | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation, pickTargetPair |
| 354 | spells | Desert's Call | - | pickTokenDest, repeatCosts |
| 357 | spells | Feral Strength | - | repeatCosts |
| 361 | units | Ribbon Dancer | - | kFinalizeTrigger, pickTarget |
| 363 | spells | Thwonk! | - | needsPlayTimeTarget, pickTarget, repeatCosts |
| 367 | units | Legion Quartermaster | kFinalizeTrigger, pickTarget | additionalPlayCost, chooseObjectNow |
| 369 | gear | Poro Snax | - | kFinalizeActivate |
| 372 | units | Aphelios, Exalted | kFinalizeTrigger, pickMode, pickTarget | kFinalizeTrigger, pickMode, pickTarget, selectAnyNumber |
| 373 | units | Azir, Ascendant | kFinalizeActivate, pickTargetPair | confirmOptional, kFinalizeActivate, pickTarget, pickTargetPair |
| 376 | units | Janna, Savior | pickTarget | confirmOptional, kFinalizeTrigger, pickTarget |
| 381 | units | Ornn, Blacksmith | - | confirmOptional, pickTarget |
| 384 | units | Aspiring Engineer | - | kFinalizeTrigger, pickTarget |
| 385 | units | Bubble Bot | - | kFinalizeTrigger, pickTarget |
| 389 | spells | Frigid Touch | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickTarget, repeatCosts |
| 400 | spells | Rocket Barrage | pickMode | kFinalizeResolve, pickMode, pickTarget, repeatCosts |
| 402 | spells | Bellows Breath | - | repeatCosts |
| 403 | units | Card Sharp | - | confirmOptional, requestChoice |
| 406 | units | Jayce, Man of Progress | confirmOptional, pickTarget | confirmOptional, kFinalizeTrigger, pickTarget |
| 407 | units | Ornn, Forge God | - | WeaponmasterUnit |
| 413 | units | Buhru Captain | - | kFinalizeTrigger, pickMode |
| 423 | units | Fae Dragon | - | pickSeveral |
| 429 | spells | Strike Down | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair | kFinalizeResolve, needsPlayTimeTargetPair, pickTarget, pickTargetPair |
| 431 | units | Akshan, Mischievous | - | WeaponmasterUnit, pickTarget |
| 433 | spells | Here to Help | requestChoice | confirmOptional, pickMode, pickTarget |
| 435 | units | Lucian, Merciless | - | WeaponmasterUnit |
| 436 | spells | Marching Orders | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair, repeatCosts |
| 440 | units | Jax, Unrelenting | confirmOptional, requestChoice | WeaponmasterUnit, confirmOptional |
| 441 | units | Sivir, Ambitious | - | confirmOptional, kFinalizeTrigger, pickTarget |
| 443 | spells | Called Shot | requestChoice | repeatCosts, requestChoice |
| 444 | units | Corrupt Enforcer | requestChoicex2 | - |
| 449 | units | Overzealous Fan | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 450 | spells | Temptation | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair | kFinalizeResolve, pickMode, pickTarget, repeatCosts |
| 453 | units | Beast Below | - | kFinalizeTrigger, pickTargetx2, pickTargetPair |
| 457 | spells | Hard Bargain | requestChoice | repeatCosts, requestChoice |
| 460 | gear | Edge of Night | - | kFinalizeTrigger, pickTarget |
| 461 | units | Fizz, Trickster | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 471 | gear | Last Rites | confirmOptional, pickTarget | chooseObjectNow, confirmOptional, kFinalizeTrigger, pickTarget |
| 472 | spells | Bonds of Strength | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair | kFinalizeResolve, needsPlayTimeTargetPair, pickTargetPair, repeatCosts |
| 475 | spells | Guards! | confirmOptional | confirmOptional, pickMode |
| 479 | units | Sandshifter | - | kFinalizeTrigger, pickTarget |
| 481 | units | Zaun Punk | confirmOptional, kFinalizeTrigger, pickTargetPair | additionalPlayCost, chooseObjectNow, confirmNow, kFinalizeTrigger, pickTarget |
| 486 | units | Glasc Mixologist | confirmOptional, kFinalizeTrigger, pickTarget | confirmOptional, kFinalizeTrigger, pickEffectPlayLocation, pickTarget |
| 489 | gear | Vanguard Armory | - | requestChoice |
| 490 | gear | Altar of Memories | confirmOptional, pickMode, pickTarget | confirmOptional, kFinalizeTrigger, pickMode, pickTarget |
| 491 | units | Rek'Sai, Swarm Queen | requestChoice | confirmOptional, kFinalizeTrigger, pickMode, requestChoice |
| 497 | units | Azir, Sovereign | - | selectAnyNumber |
| 498 | gear | Blade of the Ruined King | pickTarget | chooseObjectNow |
| 501 | spells | Danger Zone | - | repeatCosts |
| 503 | spells | Relentless Pursuit | - | confirmOptionalx2, pickMode, pickTarget |
| 505 | spells | Void Rush | - | confirmOptional, pickMode, pickTarget |
| 513 | spells | Arise! | requestChoice | requestChoicex2 |
| 515 | spells | Arcane Shift | pickTarget | pickMode, pickTarget |
| 517 | legends | Battle Mistress | - | confirmOptional, kFinalizeTrigger |
| 522 | battlefields | Forge of the Fluft | kFinalizeActivate, needs_activation_time_target, pickTargetPair | kFinalizeActivate, pickTargetPair |
| 535 | battlefields | Veiled Temple | confirmOptional | confirmOptional, kFinalizeTrigger, pickMode, pickTarget |
| 539 | units | Ahri, Inquisitive | - | kFinalizeTrigger, pickTarget |
| 540 | units | Bard, Mercurial | confirmOptional | additionalPlayCost, confirmNow, kFinalizeTrigger, pickMode, selectAnyNumber |
| 543 | units | Sett, Brawler | - | kFinalizeActivate |
| 544 | units | Yone, Blademaster | - | WeaponmasterUnit, pickTarget |
| 548 | units | Karma, Channeler | - | kFinalizeTrigger, pickTarget |
| 553 | legends | Void Burrower | confirmOptional, requestChoice | confirmOptional, kFinalizeTrigger, requestChoice |
| 554 | legends | Grandmaster at Arms | needs_activation_time_target, pickTarget | kFinalizeActivate, pickTargetPair |
| 571 | spells | Upstage Comedy | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickTarget, repeatCosts |
| 579 | spells | Square Up | - | repeatCosts |
| 582 | spells | Dancing Grenade | - | needsPlayTimeTarget, pickTarget, requestChoice |
| 583 | units | Grim Apothecary | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 589 | units | Inviolus Vox | - | kFinalizeTrigger, pickTarget |
| 594 | spells | Double Trouble | - | confirmOptional, pickTarget, repeatCosts |
| 600 | spells | Skyward Strike | needsPlayTimeTarget, pickTarget | kFinalizeResolve, pickMode, pickTargetPair |
| 606 | spells | Flurry of Feathers | pickMode | pickMode, playTokensChoosingLocations |
| 607 | gear | Forgotten Signpost | needs_activation_time_target, pickTargetx2 | kFinalizeActivate, pickTargetPair |
| 613 | units | Ivern, Nurturer | confirmOptional, kFinalizeTrigger | pickTarget, selectObject |
| 614 | units | Nami, Headstrong | kFinalizeTrigger, pickTargetx2 | kFinalizeTrigger, pickTarget |
| 616 | spells | Tricksy Tentacles | - | offerableChoices, requestChoicex2 |
| 618 | units | Yuumi, Magical Cat | - | kFinalizeTrigger, pickTarget |
| 623 | spells | Downstage Dramatics | - | repeatCosts |
| 625 | spells | Eclipse | - | predict |
| 629 | units | Ruined Rex | - | kFinalizeTrigger, pickTarget |
| 631 | spells | Sprite Burst | - | playTokensChoosingLocations |
| 636 | gear | Frigid Jewel | - | kFinalizeTrigger, pickTarget |
| 663 | spells | Call to Battle | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickMode, pickTarget, requestChoice |
| 665 | spells | Disposal Order | pickMode | pickMode, requestChoice |
| 667 | units | Imposing Challenger | confirmOptional, kFinalizeTrigger, pickTarget | confirmOptional, kFinalizeTrigger, pickMode, pickTarget |
| 669 | spells | Stare Down | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickMode, pickTarget |
| 674 | units | Irresistible Faefolk | confirmOptional, kFinalizeTrigger | confirmOptional, kFinalizeTrigger, pickTarget |
| 680 | units | Elder Dragon | - | kFinalizeTrigger, selectObject |
| 695 | gear | Blast Cone | confirmOptionalx2 | confirmOptionalx2, kFinalizeTrigger, pickMode, pickTarget |
| 696 | spells | Existential Dread | needsPlayTimeTarget, pickTarget | needsPlayTimeTarget, pickTarget, repeatCosts |
| 698 | gear | Scryer's Bloom | requestChoice | kFinalizeActivate, requestChoicex2 |
| 700 | gear | The List | needs_activation_time_target, pickTarget | needs_activation_time_target, pickTarget, requestChoice |
| 701 | spells | Bone Skewer | pickTargetPair | confirmOptional, pickMode, pickTarget |
| 702 | spells | Conscription | confirmOptional, needsPlayTimeTarget, pickTarget | confirmOptional, kFinalizeResolve, pickTarget |
| 704 | spells | Heedless Resurrection | needsPlayTimeTargetPair, pickTargetPair | kFinalizeResolve, pickMode, pickTarget, requestChoice |
| 722 | units | Ultrasoft Poro | - | playTokensChoosingLocations |
| 726 | units | Safety Inspector | confirmOptional, pickTarget | confirmOptional, kFinalizeTrigger, selectObjectAsx2 |
| 728 | units | Stalking Wolf | kFinalizeTrigger, pickTarget | additionalPlayCost, chooseObjectNow |
| 730 | spells | Undying Loyalty | needsPlayTimeTarget, pickTarget | additionalPlayCost, chooseObjectNow, kFinalizeResolve, pickMode |
| 732 | units | Atakhan | - | additionalPlayCost, chooseObjectNow, confirmNow, selectObjectAs |
| 735 | spells | Sacrifice | needsPlayTimeTarget, pickTarget | kFinalizeResolve, pickTarget |
| 736 | gear | Shard of Undoing | - | requestChoice |
| 738 | units | Vi, Peacekeeper | - | kFinalizeTrigger, pickTarget |
| 740 | units | Poppy, Defender of the Meek | - | additionalPlayCost, confirmNow |
| 741 | units | Rift Herald | pickTarget | confirmOptional, pickTargetx2 |
| 743 | spells | Curtain Call | pickMode | kFinalizeResolve, pickMode, pickTarget, repeatCosts |
| 744 | legends | Pridestalker | - | kFinalizeTrigger, pickTarget |
| 747 | spells | Death from Below | - | confirmOptional, needsPlayTimeTarget, pickTarget |
| 751 | spells | Alpha Strike | requestChoice | splitDamageAmong |
| 754 | units | Daisy! | - | kFinalizeTrigger, pickTarget |
| 755 | spells | Moonfall | confirmOptional | confirmOptional, kFinalizeResolve, pickMode, pickTarget |
| 756 | legends | Deceiver | confirmOptional, pickTarget, requestChoice | confirmOptional, kFinalizeTrigger, pickTarget, requestChoice |
| 760 | spells | Keeper's Verdict | needsPlayTimeTarget, pickMode, pickTarget | needsPlayTimeTarget, pickTarget, requestChoice |
| 762 | battlefields | Altar of Blood | - | confirmNow |
| 763 | battlefields | Amateur Recital | - | confirmOptional, kFinalizeTrigger, pickTarget |
| 765 | battlefields | Dusk Rose Lab | confirmOptional, kFinalizeTrigger, pickTarget | confirmOptional, kFinalizeTrigger, selectObject |
| 771 | battlefields | Star Spring | confirmOptional | confirmOptional, kFinalizeTrigger, pickTarget |
| 810 | units | Zed, From the Shadows | - | pickTokenDest |
| 821 | spells | Resonating Strike | needsPlayTimeTarget, pickMode, pickTarget | kFinalizeResolve, pickMode, pickTarget |
| 822 | spells | Sanction | needsPlayTimeTarget, pickMode, pickTarget | kFinalizeResolve, pickMode, pickTarget |
| 839 | spells | Mesmerize | needsPlayTimeTarget, pickMode, pickTarget | kFinalizeResolve, pickMode, pickTarget |
| 869 | units | Profiteer | confirmOptional, kFinalizeTrigger, pickTargetPair | confirmOptional, kFinalizeTrigger, pickTarget, selectObject |
| 878 | units | Corrupted Dragon | requestChoice | offerableChoices, requestChoice |
| 887 | spells | Up from the Deep | - | playTokensChoosingLocations |
| 888 | units | Gust Monk | kFinalizeTrigger, pickTargetPair | kFinalizeTrigger, pickTarget, selectObject |
| 892 | spells | Twilight Step | needsPlayTimeTarget, requestChoicex2 | needsPlayTimeTarget, pickMode, pickTarget |
| 894 | spells | Decree of Discord | requestChoice | offerableChoices, requestChoice |
| 896 | units | Illaoi, Prophet of the Great Kraken | - | pickTokenDest |
| 901 | units | Kharox | confirmOptional, pickTarget | confirmOptional, pickEffectPlayLocation, pickTarget |
| 927 | spells | Shuriken Flip | requestChoicex3 | offerableChoices, requestChoicex3 |
| 931 | spells | Death Mark | - | playTokensChoosingLocations |
| 932 | legends | Curator of the Sands | confirmOptional | confirmOptional, pickTargetPair |
| 935 | spells | Shadow Dash | needsPlayTimeTarget, requestChoicex2 | needsPlayTimeTarget, pickMode, pickTarget |
| 936 | legends | Defender of Tomorrow | - | kFinalizeActivate, pickTarget, pickTargetPair |
| 937 | spells | Acceleration Gate | requestChoice | offerableChoicesx2, requestChoice |

210 card files changed their prompt surface.

## Rune Deck knowledge (2026-10-11, for the v6 token layout)

The judges ruled that Void Hatchling's look applies to the Rune Deck. A player can now know the top rune of
their Rune Deck (looked at and left), and both players know the rune Twisted Fate, Gambler revealed and
recycled to the bottom. The engine exposes only the Rune Deck's size to `ObservationTensor` / `InfoSetId`
and tracks no rune knowledge (plans/09 9.NN4). Missing feature to add in the v6 layout, per observer: the
known top and bottom runes of each Rune Deck.
