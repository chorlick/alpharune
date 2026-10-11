# Riftbound Simulation Engine — Standards & Guidelines

C++20 game engine that simulates 1v1 Riftbound TCG matches. This file is the standards and conventions reference for engine development. For project status, completed work, and active backlog, see git log + the sibling riftbound-trainer repo's `docs/`.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j6
RIFTBOUND_ROOT=. ./build/riftbound_tests
RIFTBOUND_ROOT=. ./build/src/openspiel/riftbound_openspiel_tests
```

`riftbound_tests` links only `riftbound_core`. The OpenSpiel wrapper (`RiftboundGame` / `RiftboundState`, clone and history replay, `MctsAgent` / `IsMctsAgent`) is tested by the separate `riftbound_openspiel_tests` binary (`src/openspiel/openspiel_tests.cpp`), which is not part of `riftbound_tests`: build and run both.

First configure clones OpenSpiel v1.6.14 + abseil-cpp + nlohmann/json + pybind11_json + DDS into `build/_deps/` (~10s configure + one-time ~30s build of `open_spiel_core`). OpenSpiel is mandatory — the MCTS/ISMCTS agents in the `riftbound` binary (`src/agents/mcts_agent.cpp`) depend on it.

**Build structure.** `src/pch.h` is precompiled once (`riftbound_pch`) and reused by `riftbound_core` and the card groups. Each `src/cards/<type>/` directory is an OBJECT library (`riftbound_cards_<type>`) folded into `riftbound_core`, compiled as unity batches. Options: `-DRIFTBOUND_USE_PCH=OFF`, `-DRIFTBOUND_UNITY_CARDS=OFF` (one compile per card, e.g. for clangd), `-DRIFTBOUND_CARD_UNITY_BATCH=<n>` (default 16). `-DRIFTBOUND_WARNINGS=OFF` drops `-Wall -Wextra` (on by default for engine targets, never `-Werror`).

**Parallelism cap.** `-DRIFTBOUND_BUILD_JOBS=<n>` (default 6, `0` = no cap) is a Ninja job pool shared by every compile and link, third-party targets included: `cmake --build build -j16` still runs at most `n` at once. Ninja generators only.

**ccache.** Configure with `-DCMAKE_CXX_COMPILER_LAUNCHER="env;CCACHE_SLOPPINESS=pch_defines,time_macros;$PWD/scripts/ccache_pch_safe.sh"` (run from the checkout root). `pch_defines,time_macros` is what lets compiles that use the PCH be cached at all. **The launcher must be `scripts/ccache_pch_safe.sh`, not `ccache`**: CMake rebuilds a PCH with the old `.gch` still in place, plain ccache then takes the compile for one that *uses* that `.gch` and keys the result on the old `.gch`'s bytes instead of on the headers, so two rebuilds that start from the same `.gch` share one result. That is how worktrees sharing a cache got a `.gch` built from another worktree's headers, and it also happens in a single tree (clean build at commit X, edit a header and build, clean build at X again, edit differently and build: the second edit gets the first edit's `.gch`). The script deletes the stale `.gch` before a PCH is created; CMake warns at configure time if PCH is on and the launcher is plain ccache. With the script, a cache may be shared between checkouts or git worktrees of the same sources by adding `CCACHE_BASEDIR=$PWD;CCACHE_NOHASHDIR=1` (and `include_file_mtime,include_file_ctime` to the sloppiness) to the launcher; to keep a tree's cache to itself add `CCACHE_NAMESPACE=<name>`. Evidence (2026-10, two scratch copies of this repository sharing one cache, `core/types.h` changed differently in each, only the PCH rebuilt): with plain `ccache` copy B's `.gch` contained copy A's new function and not its own; with the script each `.gch` contained only its own. If in doubt, `cmake --build build --target clean` first: a build from clean has no stale `.gch` and is always keyed correctly.

**Tests are unity-built too.** `riftbound_tests` compiles `tests/**/*.cpp` in batches (`-DRIFTBOUND_UNITY_TESTS=OFF` for one compile per test file, e.g. while iterating on a single file or for clangd; `-DRIFTBOUND_TEST_UNITY_BATCH=<n>`, default 8). File-scope names in a test file must therefore be unique across `tests/`: put a file's helpers, test cards and fixtures in a namespace of its own (`namespace riftbound::test::<file>`), not in a bare anonymous namespace. A clash is a compile error in the batch that pairs the two files; a file that cannot share a unit is listed in `RIFTBOUND_TEST_UNITY_EXCLUDE` in `CMakeLists.txt`.

**Card files share a translation unit with their neighbours.** File-scope names in a card file must be unique within its directory: a reprint's class carries its card id (`KarmaChanneler_235` / `KarmaChanneler_548`), and helpers needed by more than one card go in `src/cards/card_helpers.h` rather than being copied.

**Dependencies:** cmake, g++ (C++20), libboost-all-dev, nlohmann-json3-dev, ninja-build.

Training pipelines (model architectures, self-play loops, checkpoints) live in the sibling **riftbound-trainer** repo. Engine repo has no torch dependency.

## Coding Standards

### C++ Style
- **C++20** standard, `-std=c++20`
- **Namespace:** `namespace riftbound` for all engine code
- **Headers:** `#pragma once`, no include guards
- **Naming:** `PascalCase` types/enums, `camelCase` methods/variables, `snake_case` file names, `kConstant` constants
- **Includes:** project headers use `"core/types.h"` relative to `src/`, system headers use `<>`
- **Strong types:** Enums and tagged IDs over raw ints. `PlayerId`, `GameObjectId`, `BattlefieldId`, etc.
- **No raw owning pointers:** Use references, `std::optional`, containers. Objects live in `GameState::objects` map.
- **Error handling:** `assert()` for invariant violations, `throw` for invalid input, return codes for expected failures (e.g. deck validation).

### Architecture Patterns
- **Event Bus** (Boost.Signals2): Actions emit events, subsystems subscribe. How triggered abilities, cleanup, logging, and rendering are wired. See `src/core/events.h`.
- **Intent/Command:** Every player action is an `Intent` struct. Engine enumerates legal intents, agent picks one, engine validates and executes. See `src/core/intent.h`.
- **Dependency Injection:** Subsystems (cleanup, combat, renderer, logger) are injected into the engine, not constructed by it. Makes testing easy — inject mocks.
- **Immutable CardDef, mutable GameObject:** `CardDef` (from registry) is static. `GameObject` is the runtime instance with current state.
- **Fiber-based step machine:** `boost::context::fiber` lets the engine yield at decision points without spawning OS threads. See `src/engine/step_driver.h`.

### Testing
- Google Test framework, tests in `tests/`, files named `test_<module>.cpp`.
- Tests cover behavioral boundaries, not 100% line coverage.
- Per-card tests under `tests/cards/` use the shared `tests/cards/card_test_fixture.h`. Card behaviour is tested through the real engine path (`playThroughEngine` / `fireTriggerThroughEngine` / `resolveThroughEngine` with an `EnginePathAgent`); the direct-call helpers never hand a trigger targets, because the engine never does.
- CardDB is built from the card classes (`buildFromClasses`); tests use `RIFTBOUND_ROOT` only to locate sibling data dirs like `decks/`.

### Key Rule: Legends are NOT Champions
Legends (`card_type=legend`) and champion units (`card_type=unit, super_type=champion`) are fundamentally different. Never conflate them. They share champion tags but occupy different zones and follow different rules.

### Core principle: card mechanics are encapsulated in Card objects, not the engine

The GameEngine handles game flow (phases, turns, chain FEPR loop, cleanup, scoring) and provides atomic operations via EffectExecutor. All card-specific behavior lives in Card subclass `onResolve()` / `onTrigger()` / `onActivate()` overrides. **Never add card-specific logic to game_engine.cpp.** If you find yourself adding `if (card_name == "X")` or matching `ability_text.find("pattern")` in the engine, it belongs in the Card object.

## Architecture Notes

- **Card objects** (`src/cards/card.h`): Every card has a Card subclass registered in `CardRegistry`. Overrides `onResolve()`, `onTrigger()`, `onActivate()`, etc. All card-specific behavior — effects, targeting, countering, token creation, damage computation — belongs here, not in the engine.
- **CardRegistry** (`src/cards/card_registry.{h,cpp}`): Maps `CardDefId → Card*`. Loaded ONCE at application startup via `registerAllCards()` (aggregated in the checked-in `cards_init.cpp`, one hand-added line per card), shared as `const CardRegistry&` across all game threads. Card objects are stateless — concurrent reads safe. One authoritative class per card lives in its own TU under `src/cards/<type>/<id>_<slug>.cpp`.
- **Cards own their data** (single source of truth): each card class implements the pure-virtual `Card::def()` returning its `CardDef` (a `const CardDef def_` member). `CardDB::buildFromClasses()` materializes the card table by asking every registered card for `def()`. There is NO `registry.json` and NO code generation — cards are hand-authored C++. Tournament legality is class-owned too (`CardDef::banned`); there is no `ban-list.csv`. The CLI `--dump-registry <path>` exports the compiled card table to registry-style JSON for external/validation use.
- **EffectExecutor**: Utility library of atomic game operations (`dealDamage`, `drawCards`, `killObject`, `bounceToHand`, `createToken`, `copyUnit`, `predict`, `moveToBattlefield`, …). Card objects call these via `ctx.executor.*`. The executor does not decide WHAT to do — that's the card's job.
- **TriggerManager** subscribes to `EventBus` and dispatches via `card->triggerType()`. Also checks `DelayedAbility` list for one-shot delayed triggers.
- **Chain is a LIFO stack** (`std::vector`, resolve from back). When a spell/ability resolves, FEPR pops it then calls `resolveSpell()` which dispatches to `card->onResolve()`. Counter spells use **peek-and-pop**: the counter resolves (popped by FEPR), then pops the next spell. Counter-of-counter works naturally via LIFO — no flags, no scanning, no chain mutation outside the card's own `onResolve`.
- **Aura system** uses tagged effects (`AuraEffect` on GameObject), recalculated from scratch during `cleanup()`. No formal layer system. `hasKeyword()` checks both base and aura-granted keywords. **`Card::applyPassiveAura` is called ONCE per (card, controller)** that has an instance in play (board or Legend Zone), not once per instance and without saying which instance: the hook must loop over every instance that controller has (or use `forEachSelfOnce` in `card_helpers.h`), check each one's own condition and let copies stack. A battlefield card is called once per battlefield slot holding it. It is the only way a card has a continuous effect: the engine does not read aura wording out of `ability_text`.
- **Combat damage**: Both attacker and defender queried through the agent interface. Tank/Backline ordering enforced. Agent picks from greedy-lethal, spread-even, focus-all, and lethal-first distributions.
- **Cost payment**: `payCardCost()` applies `CostModifier` reductions, then exhausts runes for energy (agent-chosen), then recycles runes for power (agent-chosen). Energy-first ordering allows recycling just-exhausted runes for power. Each rune selection is an agent decision point. Every other payment (Equip, activated abilities, Accelerate, Repeat, Hide, Flow and trash-play grants, Deflect) goes through `engine/resource_payment.h` (`resource::canPay` / `resource::pay`), which takes a `SpendFor` purpose.
- **Restricted resources**: `RunePool::unit_energy`, `spell_energy`, `showdown_energy`, `spell_power`, `gear_power` hold resources a card restricts ("Use only to play spells"). They are spent before unrestricted ones and only for the matching `SpendFor` (showdown Energy only while a showdown or combat is under way); a card adds to the pool field directly.
- **Additional costs chosen before payment (CR 356.2)**: "As an additional cost to play me, kill a friendly unit", "you may spend a buff ... ignore this spell's cost", "you may discard 1 ... reduce my cost by [2]" are `Card::additionalPlayCost()` (what the play gate sees: exists / mandatory / payable / best discount) and `Card::payAdditionalPlayCost(ctx, affordable)` (asks with `ctx.executor.confirmNow` / `chooseObjectNow`, pays, returns the `PlayCostAdjust`). The engine calls it after the card left its zone and before its cost is paid, on every play path (hand, reaction, effect plays that pay). An optional cost may be declined only while the card is affordable without it. A card that needs to remember it was paid sets a `card_counters` flag for its play trigger's `triggerCondition`.
- **Equip (CR 818)**: a gear declares its Equip cost as data — `Card::equipCost()` (Energy / Power) plus `canPayEquipExtraCost` / `payEquipExtraCost` for the non-resource part ("Spend 1 XP", "Kill a friendly unit") and `onEquipped` for what happens on attach. `Card::canPayEquip` is the one gate (generator, execution, Weaponmaster) and `Card::payEquipAndAttach` the one payment; `EquipTerms` carries discounts (first gear ability each turn, Weaponmaster's "[A] less" and "even if it's already attached"). An attached Equipment's own Equip is not offered (its Rules Text is Inactive, CR 718.2).
- **Weaponmaster (CR 821)**: `WeaponmasterUnit` (`cards/units/weaponmaster_base.h`) asks the may and the Equipment and pays the Equip cost one Power less through `weaponmasterChoices` / `weaponmasterEquip`. A permanent that has the keyword from a continuous effect (a Sand Soldier token) gets the same ability through `engine/keyword_abilities`.
- **Repeat (CR 820)**: a card declares `Card::repeatCosts()` (one entry per printed cost; Energy, Power, or a discard). `executePlaySpell` offers each once after the base cost is paid.
- **Other cost hooks**: `Card::trashPlayCost()` (a standing "play me from your trash for ..." permission), `Card::adjustHideCost()` (Hide cost changes), `Card::optionalCostDiscount()` ("optional additional costs you pay cost [1] or [A] less"), `Card::activationPowerReductionFor()` (the "[A] less" half of an activation discount).
- **Multi-ability per Card**: `Card::activatedAbilities() → vector<ActivatedAbility>` returns N descriptors. Default impl wraps the legacy single-ability virtuals for back-compat. Multi-ability cards override `activatedAbilities()` and override the indexed `onActivate(ctx, ability_index, targets)`. `Intent::ability_index` + `ChainItem::ability_index` carry the choice through.
- **Resumable agent choices**: `Card::confirmOptional` (yes/no), `Card::pickMode` (modal), `Card::pickXAmount` (variable-X), `Card::pickTarget` (single), `Card::pickTargetPair` (dual). They compose **in any order** within one callback: each helper tracks its own waiting/answered state (flags in `resume_data[Card::kChoiceStateSlot]`; `confirmOptional` uses `ChainItem::may_answer`), not the shared `resume_point`. One call of each helper per callback (a second `confirmOptional` / `pickTarget` returns the first answer). `resume_point` is still written so a card can tell a suspend from a fizzle:
  - confirmOptional / pickXAmount: 1 waiting, 2 answered (X in resume_data[0])
  - pickMode: 4 waiting, 5 answered (resume_data[1])
  - pickTarget: 7 waiting, 8 answered (resume_data[2])
  - pickTargetPair: 10 / 12 waiting (returns `{kInvalidId, kInvalidId}` for both), 13 done (resume_data[3..4])
- **Choices at finalization (CR 355, 383.3)**: a triggered ability's "you may", its "[cost] to [effect]" payment and its targets are decided when it is finalized, not when it resolves. A card opts a callback in with `choicesAtFinalization()` (`kFinalizeTrigger` / `kFinalizeResolve` / `kFinalizeActivate`); the engine then runs that callback once at finalization (`GameEngine::runFinalizePass`) and again at resolution, where the choice helpers return the stored answers. The card marks the end of its choices with `if (finalizingChoices(ctx)) return;` — **nothing before that line may change game state** except the helpers and costs guarded by `costDueNow(ctx)`; pre-checks that depend on the cost use `costAlreadyPaid(ctx)`. Every branch of a multi-trigger card needs the marker, including branches with no choice. A target chosen at finalization that is no longer legal at resolution comes back as `kInvalidId` (mistarget). **The engine never passes targets to `onTrigger` / `onEquippedTrigger`** (the `targets` argument is always empty; the event's subject is `ChainItem::triggering_subject`): a triggered ability that needs a target opts in with `kFinalizeTrigger` and picks it with `pickTarget` / `pickTargetPair` from a list it builds itself.
- **Target-collision fix**: `Card::needsPlayTimeTarget()` (single target) and `Card::needsPlayTimeTargetPair()` (dual) opt the action generator into emitting ONE `Play` intent per card (no per-target variants) so policy heads get distinct vocab slots per target choice. The card's `onResolve` uses `pickTarget` / `pickTargetPair` at chain time. Equivalent for activated abilities: `ActivatedAbility::needs_activation_time_target`.
- **Action history and replay (OpenSpiel)**: `GameState::action_history` holds one **history token** per applied decision (`openspiel::historyToken`, `action_vocab.h`): the vocab slot plus a fingerprint of the whole Intent. A slot alone is lossy (a mulligan slot says how many cards, not which), so replay matches tokens (`findHistoryToken`), never "the first legal Intent with this slot". `RiftboundState::Clone()` is lazy: a clone taken at a top-level main-phase decision (`GameEngine::isResumableDecision`) resumes straight from the snapshot; a clone taken anywhere else (mulligan, chain priority, showdown, combat, cost payment, a play-time prompt) is rebuilt from the nearest resumable ancestor by replaying the tokens since, and the rebuilt engine is checked against the snapshot's pending decision (a mismatch throws). `MctsAgent` keeps a mirror state built from the same decks and engine seed and syncs it with `RiftboundState::syncToHistory`.
- **Target roles**: `targets` as handed to `onResolve` / `onActivate` holds only the play-time targets that are still legal, in chosen order with the gaps closed — positions are not roles. A card whose slots mean different things reads each one with `Card::chosenTarget(ctx, targets, role)` (kInvalidId once that slot's target is illegal).
- **Target legality at offer time**: the generators call `Card::hasLegalTargets(state, controller, card_db)`; override that overload when the requirement is on a printed value (Defy, Rebuttal), the two-argument one otherwise. The default requires a whole legal choice for a fixed-count multi-target spell.
- **Play locations**: a unit's printed permission is `Card::extraPlayLocations()` (`kPlayToOpenBattlefield` / `kPlayToOccupiedEnemyBattlefield` / `kPlayToLoneEnemyBattlefield`), a grant to other units is a `PlayerState` flag set from `applyPassiveAura` (`grant_friendly_units_open_bf`, `grant_units_lone_enemy_bf`); both are read in one place, `GameEngine::unitPlayableAt`. A battlefield's "can't be played here" / "can't move from here to base" are `BattlefieldCard::blocksUnitPlay()` / `blocksMoveToBase()`.
- **Standard Move (CR 144.3)**: the generator emits one intent per (unit, destination) from `GameEngine::standardMoveDestinations`; `executeStandardMove` then asks which other units join the same move (a `MakeChoice` per added unit, first option "Move no more units") and charges `BattlefieldState::multi_move_tax[player]` [A] per unit beyond the first. `GameObject::cant_move_this_turn` stops Standard Moves and its controller's effect moves until the Expiration Step.
- **Might-dependent auras**: an effect whose condition is Might ("While I'm [Mighty] ...", "units with less Might than me") overrides `hasMightDependentAura()` + `applyMightDependentAura()`; `recalculateAuras` calls it after folding every ordinary aura and folds again, so the answer never lags a pass and never counts its own grant.
- **Settling before the main-phase decision**: `advanceMainPhase` runs staged showdowns / combats and the chain, with a cleanup after each round, until nothing is pending — the turn player is never asked with an item on the chain. `combatResolutionStep` resolves the chain where CR 468.2 / 468.4 say so and, on No Result with both players present, keeps the battlefield Contested so another combat is staged (CR 468.3.d.1).
- **Hidden plays**: `generateHiddenPlayActions` serves Closed States and showdowns; a hidden spell's targets are restricted to its battlefield slot by slot, a card frees a slot with `Card::hiddenTargetFreeOfBattlefield(slot)` (CR 811.1.d.4).
- **Victory Score**: cards read `GameState::effectiveVictoryScore()`, never `mode.victory_score`.
- **Threading model**: `BatchRunner` wraps `boost::asio::thread_pool`. `GameRunner` is per-game, fully thread-safe (all state stack-local). Shared singletons: `CardDB` (const), `CardRegistry` (const). Per-game: `EventBus`, `GameState`, `GameEngine`, agents, I/O. `AggregateResults` uses atomics for counters, mutex for console output.
- **`on_decision` callback** fires at every decision point including mulligans, combat damage, and chain priority. Trivial single-option decisions are auto-skipped in render.
- **Logging levels**: `--debug` shows trigger firings and ability resolution. `--trace` adds every phase transition, decision, intent, effect, draw, discard, rune exhaust/recycle, damage assignment, kill, score, counter, equip, token, burn out, and mulligan with card names.
- **HTML replay**: `riftbound --render-html on` writes `<replay-dir>/<timestamp>/replay.html` (single game) or `<replay-dir>/<timestamp>/replay_gameN.html` (batch), with UUID, seed, board state + decision + trace log panels, arrow-key navigation, ★ SCORE banner on scoring snapshots, clickable card-name lookups.
- **State features** (`src/ml/feature_extractor.cpp`): Produces a perfect-information state vector (`kStateFeatureDim = 6188`, derived from `kCardVocabSize = 1100`: ids 1..955 are registered through Vendetta, 956..1100 are reserved for Radiance). `openspiel::kNumCardDefIds` must equal `kCardVocabSize` (static_assert in `riftbound_state.cpp`); changing either invalidates trainer checkpoints. Used by `RiftboundState::ObservationTensor` to expose game state to OpenSpiel algorithms. ObservationTensor masks hidden information per perspective. **No torch dependency** — these are plain `vector<float>` for downstream ML consumers.
- **Coin toss**: `TurnState::starting_player` records who won the coin flip (CR 116).
- **Empower / Empowered (Vendetta, CR 442-443, 827-828)**: `GameObject::empowered` is the status count (binary unless `Card::maxEmpower()` > 1). A card that prints `[Empower] [cost]` overrides `Card::empowerSpec()`; the base `activatedAbilities()` appends it as an `ActivatedAbility{is_empower}` and the engine gates it ("only if not Empowered", `Card::canActivateAbilityAt`), pays it, and resolves it via `EffectExecutor::empower`. `[Empowered][>]` passives go through `forEachEmpoweredSelf` in `applyPassiveAura`. Triggers: `WhenIBecomeEmpowered`, `WhenYouEmpower`. "Disempower this" as a cost is `ActivationCost::disempower_self`. Empowered is cleared when an object leaves the board (in `recalculateAuras`).
- **Flow (Vendetta)**: a spell overrides `Card::flowCost()`. `GameEngine::refreshFlowGrants` (end of `recalculateAuras`) rebuilds a standing `TrashReplayGrant{banish_after}` per Flow spell in a trash, so the existing trash-replay generator/payment handles it; paying sets `GameObject::banish_on_leave_chain` and `ChainManager` banishes instead of trashing.
- **Burn (Vendetta, CR 441)**: `EffectExecutor::burn(player, n)` moves the top N of the Main Deck to trash (burning out mid-way per CR 441.4 via `burnOut`) and emits `ObjectStateChangedEvent "burned"` → `WhenYouBurn`.
- **String-keyed state events**: `ObjectStateChangedEvent{id, "<what>"}` is the cheap channel for new trigger sources (`"empowered"`, `"burned"`, `"banished_owned"`, `"discarded"`, `"played_not_from_hand"`, …) — dispatch lives in `TriggerManager::onObjectStateChanged`, so adding one needs no header change.
- **Activated-ability costs**: `ActivationCost::energy` / `power` (+ `power_domain` / `power_any_domain`) after reductions are `GameEngine::netActivationCost`; `canPayActivation` is the one predicate the ability generators and `payAndQueueAbility` share.

## Lessons learned / pitfalls

- **Never put card-specific logic in `game_engine.cpp`.** All card behavior belongs in Card subclass overrides. Engine dispatches; cards decide.
- **Counter spells must NOT directly manipulate the chain.** Use peek-and-pop: the counter resolves (popped by FEPR), then pops the next spell. Never set flags or scan the chain — if the counter is countered first, its `onResolve` never runs, so the target is naturally preserved.
- **AoE damage must collect targets BEFORE killing.** Deal damage to all targets first, then iterate again to kill lethally damaged units. Killing during the damage loop causes iterator invalidation.
- **`addAbility()` must close state** (`oc_state = Closed`). Without this, `generateSpellActions` falls through to `isNeutralOpen()` and allows Action spells during ability chain resolution.
- **Duplicate card names across sets** exist (e.g., "Karma, Channeler" IDs 235 and 548). Each needs its own Card object, with the card id suffixed to the class name.
- **Channeled runes enter READY during channel phase** (CR 316), but card effects that say "channel N runes exhausted" must enter EXHAUSTED. The `channelRunes()` method has an `enter_exhausted` parameter.
- **The mulligan is a single atomic decision** — agent picks which cards (0-2) to set aside in one intent. Cards are removed, replacements drawn, then set-aside recycled. The agent cannot see drawn replacements before deciding. Order: set aside → draw → recycle (CR 118).
- **Death replacement effects are card-owned**, not text-matched: a card overrides `Card::hasReplacementEffect()` / `applyReplacement(ctx, dying_unit)` and returns true when it replaces THAT unit's death. Both kill paths (`GameEngine::killUnit` and `EffectExecutor::killObject`) ask the dying unit's controller's cards in play and the battlefield card where it is dying; the first that returns true consumes the death.
- **The engine does not read `ability_text`.** Replacement effects in `killUnit()`, play-location permissions and battlefield restrictions are all Card virtuals. Repeat costs are `Card::repeatCosts()`.
- **The render shows board state BEFORE resolution.** A chain item may be visible but its effect hasn't happened yet. The trace log in HTML replay shows what happens between renders.
- **Never confuse `cardDefId()` with `ctx.source`.** `cardDefId()` is the static card template ID (the card's own `def().id`). `ctx.source` is the runtime `GameObjectId`. Using `cardDefId()` where a `GameObjectId` is expected causes `getObject()` assertion failures.
- **MCTS rollouts have no internal length cap.** `RandomRolloutEvaluator` runs `while(!IsTerminal) ApplyAction(random)`. If the engine produces a state that never reaches a natural terminal under random play, the rollout spins forever inside `MCTSBot::MCTSearch`. Mitigation: `RiftboundState::IsTerminal()` has a hard cap at `MoveNumber() >= 600`.
- **Token cease-to-exist (CR 183.1).** Tokens routed to any non-board zone cease to exist immediately. Both `EffectExecutor::killObject` and `GameEngine::killUnit` route tokens to Banishment WITHOUT adding to `player.banishment` vector. Tokens copied via Mirror Image inherit `card_def_id` so death triggers still fire.
- **Combat designations follow the combat, not the trigger (CR 323.2).** `GameEngine::processContestedBattlefields` re-derives `combat_designation` in every cleanup: a unit at a battlefield with `combat_in_progress` has its controller's designation, a unit anywhere else has none, and combat resolution clears them all. So a designation is still set while that combat's showdown resolves "when I attack / defend" triggers, and gone once the unit leaves the battlefield or the combat ends. It says what a unit is now, not which attacker caused a trigger: `TriggerManager` therefore records the attacking unit in the defender's `card_counters["__defend_attacker_id"]` when `WhenIDefend` fires, and a card that needs "the attacker" reads that at `onTrigger` time (see `OverzealousFan`, 449) instead of scanning the board.
- **Action vocab slot collisions.** Always check whether your encoding distinguishes the cases you need the policy head to distinguish. Examples: `MakeChoice` int-coded slots for yes/no/mode/X answers; `needsPlayTimeTarget` + `pickTarget` for play-time target choice; `ActivatedAbility::needs_activation_time_target` + `pickTarget` for ability-time target choice; multi-ability `ability_index` slot encoding for per-ability distinction.

## OpenSpiel integration knowledge

### Build mechanics

- OpenSpiel's CMake reads **environment variables**, not cache vars, for its build options. Set via `set(ENV{OPEN_SPIEL_BUILD_WITH_*} "OFF")` from the parent project.
- OpenSpiel doesn't ship abseil-cpp / nlohmann/json as submodules; `install.sh` clones them and `sudo apt-get`s system packages. We clone abseil + json + pybind11_json + DDS ourselves in our parent `CMakeLists.txt`.
- `open_spiel_core` is an OBJECT library; its PUBLIC includes only expose `${open_spiel-src}/open_spiel`, not the parent. Add `target_include_directories(YOUR_TARGET PRIVATE ${open_spiel_SOURCE_DIR})` and replicate the abseil link list from OpenSpiel's directory-level `link_libraries(...)`.

### Resume pattern for in-flight chain resolution

- `ChainItem::resume_point` (int) + `resume_data` (vec<int32_t>) save progress.
- `ChainState::resuming` (`optional<ChainItem>`) holds the resolving item across iterations.
- `EffectExecutor::{requestChoice, recordChoice, takeChoice, applyDiscard}` publishes/consumes a choice slot.
- `ChainManager::stepResolve` pops to `resuming`, calls `resolve_spell` in a `while(true)`, breaks on no pending choice. Counter spells unaffected (they peek `items.back()`, not `resuming`).
- The choice helpers compose in any order within a single callback (see "Resumable agent choices" above). A card that drives `resume_data` by hand and also calls a pick* helper leaves slots 0..6 to the helpers.

## Adding state to `feature_extractor`

When you add a field to `GameState` / `PlayerState` that should be visible to a future ML agent, also bump `kStateFeatureDim` in `src/ml/feature_extractor.h` and append the new dims to `extractStateFeatures` in the `.cpp`. This is what `RiftboundState::ObservationTensor` reports to OpenSpiel. The trainer repo's `docs/additional-gamestate-dims.md` documents the dim layout + a backlog of additional fields to expose.

## Important game rules

- **Victory Score:** 8 points (1v1).
- **Battlefield Count:** 2 at start, can grow via token battlefields.
- **Winning Point (CR 466.1.b):** Via Hold = always works. Via Conquer = must have scored EVERY battlefield (including tokens) this turn. Via card effect = no restrictions.
- **Cleanup (CR 319):** Fires after nearly every state change. Reentrant. 9-step process.
- **Units enter exhausted** (CR 143.4) unless Accelerate or similar.
- **Gear enters ready** (CR 149.1).
- **Rune pools empty** at end of draw phase and end of turn.
- **Replace (CR 438):** Original goes to Banishment, token takes its place, can swap back.
- **Token cease-to-exist (CR 183.1):** Tokens routed to any non-board zone cease to exist immediately.
- **Ambush (CR 822.1.b):** grants a play location (a battlefield where you control units) and [Reaction] timing while played there. It does NOT make the unit enter ready.

## Reference

- `rules/core-rules.md` — full game rules (2026-10-09 edition, sections 000–832; regenerate from the PDF with `scripts/rules_to_md.py`)
- `rules/patch-notes/` — per-set core-rules patch notes, FAQs and ban announcements; `errata/` — card errata
- `rules/tournament-rules.md` — deck construction + tournament policies
- Sibling **riftbound-trainer** repo holds training-side docs (V*/model designs, ReBeL inference, engineering history, gamestate-dims backlog) and the trainer source.

## Card data

Card data is **hand-authored C++** — there is no `registry.json`, no `ban-list.csv`,
and no code-generation pipeline. Each card's class implements `Card::def()` returning
its `CardDef` (cost, domains, tags, keywords, art URL, `banned` flag, …); `CardDB` is
materialized from those at startup (`buildFromClasses`). To add or change a card, edit
its `src/cards/<type>/<id>_<slug>.cpp` directly.

- `scripts/fetch_cards.py` — pulls raw card metadata (art, artist, rarity) from the
  official gallery (playriftbound.com) into `cards/raw/gallery_raw.json`, for reference when authoring new
  cards. It does NOT feed the engine.
- `./build/riftbound --dump-registry <path>` — exports the compiled card table to
  registry-style JSON (validation / external consumers).
