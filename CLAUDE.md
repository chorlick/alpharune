# Riftbound Simulation Engine — Standards & Guidelines

C++20 game engine that simulates 1v1 Riftbound TCG matches. This file is the standards and conventions reference for engine development. For project status, completed work, and active backlog, see git log + the sibling riftbound-trainer repo's `docs/`.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
RIFTBOUND_ROOT=. ./build/riftbound_tests
```

First configure clones OpenSpiel v1.6.14 + abseil-cpp + nlohmann/json + pybind11_json + DDS into `build/_deps/` (~10s configure + one-time ~30s build of `open_spiel_core`). OpenSpiel is mandatory — MCTS/ISMCTS agents in `riftbound_openspiel` depend on it.

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
- Per-card tests under `tests/cards/` use the shared `tests/cards/card_test_fixture.h`.
- CardDB tests use `RIFTBOUND_ROOT` env var or relative path to find `cards/registry.json`.

### Key Rule: Legends are NOT Champions
Legends (`card_type=legend`) and champion units (`card_type=unit, super_type=champion`) are fundamentally different. Never conflate them. They share champion tags but occupy different zones and follow different rules.

### Core principle: card mechanics are encapsulated in Card objects, not the engine

The GameEngine handles game flow (phases, turns, chain FEPR loop, cleanup, scoring) and provides atomic operations via EffectExecutor. All card-specific behavior lives in Card subclass `onResolve()` / `onTrigger()` / `onActivate()` overrides. **Never add card-specific logic to game_engine.cpp.** If you find yourself adding `if (card_name == "X")` or matching `ability_text.find("pattern")` in the engine, it belongs in the Card object.

## Architecture Notes

- **Card objects** (`src/cards/card.h`): Every card has a Card subclass registered in `CardRegistry`. Overrides `onResolve()`, `onTrigger()`, `onActivate()`, etc. All card-specific behavior — effects, targeting, countering, token creation, damage computation — belongs here, not in the engine.
- **CardRegistry** (`src/cards/card_registry.{h,cpp}`): Maps `CardDefId → Card*`. Loaded ONCE at application startup, shared as `const CardRegistry&` across all game threads. Card objects are stateless — concurrent reads safe. Manual overrides (`src/cards/manual/*.cpp`) register AFTER generated cards and overwrite generated stubs.
- **Code generation** (`scripts/generate_cards.py`): Reads `registry.json`, parses `ability_text`, generates C++ Card classes. Regenerate with `python3 scripts/generate_cards.py`. Manual overrides go in `src/cards/manual/`.
- **EffectExecutor**: Utility library of atomic game operations (`dealDamage`, `drawCards`, `killObject`, `bounceToHand`, `createToken`, `copyUnit`, `predict`, `moveToBattlefield`, …). Card objects call these via `ctx.executor.*`. The executor does not decide WHAT to do — that's the card's job.
- **TriggerManager** subscribes to `EventBus` and dispatches via `card->triggerType()`. Also checks `DelayedAbility` list for one-shot delayed triggers.
- **Chain is a LIFO stack** (`std::vector`, resolve from back). When a spell/ability resolves, FEPR pops it then calls `resolveSpell()` which dispatches to `card->onResolve()`. Counter spells use **peek-and-pop**: the counter resolves (popped by FEPR), then pops the next spell. Counter-of-counter works naturally via LIFO — no flags, no scanning, no chain mutation outside the card's own `onResolve`.
- **Aura system** uses tagged effects (`AuraEffect` on GameObject), recalculated from scratch during `cleanup()`. No formal layer system. `hasKeyword()` checks both base and aura-granted keywords.
- **Combat damage**: Both attacker and defender queried through the agent interface. Tank/Backline ordering enforced. Agent picks from greedy-lethal, spread-even, focus-all, and lethal-first distributions.
- **Cost payment**: `payCardCost()` applies `CostModifier` reductions, then exhausts runes for energy (agent-chosen), then recycles exhausted runes for power (agent-chosen). Energy-first ordering allows recycling just-exhausted runes for power. Each rune selection is an agent decision point.
- **Multi-ability per Card**: `Card::activatedAbilities() → vector<ActivatedAbility>` returns N descriptors. Default impl wraps the legacy single-ability virtuals for back-compat. Multi-ability cards override `activatedAbilities()` and override the indexed `onActivate(ctx, ability_index, targets)`. `Intent::ability_index` + `ChainItem::ability_index` carry the choice through.
- **Resumable agent choices**: `Card::confirmOptional` (yes/no), `Card::pickMode` (modal), `Card::pickXAmount` (variable-X), `Card::pickTarget` (single), `Card::pickTargetPair` (dual). Each uses distinct resume_point ranges + resume_data slots so multiple choices compose in one `onResolve`:
  - confirmOptional / pickXAmount: resume_points 0/1/2 + resume_data[0]
  - pickMode: resume_points 3/4/5 + resume_data[1]
  - pickTarget / pickTargetPair: resume_points 6/7/8 + resume_data[2]
- **Target-collision fix**: `Card::needsPlayTimeTarget()` (single target) and `Card::needsPlayTimeTargetPair()` (dual) opt the action generator into emitting ONE `Play` intent per card (no per-target variants) so policy heads get distinct vocab slots per target choice. The card's `onResolve` uses `pickTarget` / `pickTargetPair` at chain time. Equivalent for activated abilities: `ActivatedAbility::needs_activation_time_target`.
- **Threading model**: `BatchRunner` wraps `boost::asio::thread_pool`. `GameRunner` is per-game, fully thread-safe (all state stack-local). Shared singletons: `CardDB` (const), `CardRegistry` (const). Per-game: `EventBus`, `GameState`, `GameEngine`, agents, I/O. `AggregateResults` uses atomics for counters, mutex for console output.
- **`on_decision` callback** fires at every decision point including mulligans, combat damage, and chain priority. Trivial single-option decisions are auto-skipped in render.
- **Logging levels**: `--debug` shows trigger firings and ability resolution. `--trace` adds every phase transition, decision, intent, effect, draw, discard, rune exhaust/recycle, damage assignment, kill, score, counter, equip, token, burn out, and mulligan with card names.
- **HTML replay**: `--render` (legacy CLI) and `--render-html` (OpenSpiel CLI) generate `replay_gameN.html` with UUID, seed, board state + decision + trace log panels, arrow-key navigation, ★ SCORE banner on scoring snapshots, clickable card-name lookups.
- **State features** (`src/ml/feature_extractor.cpp`): Produces a perfect-information state vector (`kStateFeatureDim = 4623`, padded to `RESERVED_STATE_DIM = 4864`). Used by `RiftboundState::ObservationTensor` to expose game state to OpenSpiel algorithms. ObservationTensor masks hidden information per perspective. **No torch dependency** — these are plain `vector<float>` for downstream ML consumers.
- **Coin toss**: `TurnState::starting_player` records who won the coin flip (CR 116).

## Lessons learned / pitfalls

- **Never put card-specific logic in `game_engine.cpp`.** All card behavior belongs in Card subclass overrides. Engine dispatches; cards decide.
- **Counter spells must NOT directly manipulate the chain.** Use peek-and-pop: the counter resolves (popped by FEPR), then pops the next spell. Never set flags or scan the chain — if the counter is countered first, its `onResolve` never runs, so the target is naturally preserved.
- **AoE damage must collect targets BEFORE killing.** Deal damage to all targets first, then iterate again to kill lethally damaged units. Killing during the damage loop causes iterator invalidation.
- **`addAbility()` must close state** (`oc_state = Closed`). Without this, `generateSpellActions` falls through to `isNeutralOpen()` and allows Action spells during ability chain resolution.
- **Duplicate card names across sets** exist (e.g., "Karma, Channeler" IDs 235 and 548). Code-gen handles with `_card_id` suffix. Each needs its own Card object.
- **Channeled runes enter READY during channel phase** (CR 316), but card effects that say "channel N runes exhausted" must enter EXHAUSTED. The `channelRunes()` method has an `enter_exhausted` parameter.
- **The mulligan is a single atomic decision** — agent picks which cards (0-2) to set aside in one intent. Cards are removed, replacements drawn, then set-aside recycled. The agent cannot see drawn replacements before deciding. Order: set aside → draw → recycle (CR 118).
- **Replacement effects still use raw `ability_text` matching** in `killUnit()`. Intentional — replacements intercept game actions, not chain resolution. Future: structured ReplacementEffect type on Card.
- **The render shows board state BEFORE resolution.** A chain item may be visible but its effect hasn't happened yet. The trace log in HTML replay shows what happens between renders.
- **Never confuse `cardDefId()` with `ctx.source`.** `cardDefId()` is the static card template ID from `registry.json`. `ctx.source` is the runtime `GameObjectId`. Using `cardDefId()` where a `GameObjectId` is expected causes `getObject()` assertion failures.
- **Generated card files are overwritten by `generate_cards.py`.** Manual fixes to generated cards must be re-applied or moved to `src/cards/manual/`.
- **MCTS rollouts have no internal length cap.** `RandomRolloutEvaluator` runs `while(!IsTerminal) ApplyAction(random)`. If the engine produces a state that never reaches a natural terminal under random play, the rollout spins forever inside `MCTSBot::MCTSearch`. Mitigation: `RiftboundState::IsTerminal()` has a hard cap at `MoveNumber() >= 600`.
- **Token cease-to-exist (CR 183.1).** Tokens routed to any non-board zone cease to exist immediately. Both `EffectExecutor::killObject` and `GameEngine::killUnit` route tokens to Banishment WITHOUT adding to `player.banishment` vector. Tokens copied via Mirror Image inherit `card_def_id` so death triggers still fire.
- **Trigger context capture for designations.** `combat_designation` is cleared between trigger-fire and chain resolution. Cards that need attacker/defender identity must capture it into `card_counters["__defend_attacker_id"]` at fire time, not at `onTrigger` time. See `MOverzealousFan`.
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
- Slot ranges by helper (compose cleanly within a single `onResolve`):
  - `confirmOptional` / `pickXAmount`: resume_points 0/1/2 + resume_data[0]
  - `pickMode`: resume_points 3/4/5 + resume_data[1]
  - `pickTarget` / `pickTargetPair`: resume_points 6/7/8 + resume_data[2]

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

## Reference

- `rules/core-rules.md` — full game rules (sections 000–826)
- `rules/tournament-rules.md` — deck construction + tournament policies
- `cards/ban-list.csv` — banned cards
- Sibling **riftbound-trainer** repo holds training-side docs (V*/model designs, ReBeL inference, engineering history, gamestate-dims backlog) and the trainer source.

## Data pipeline

```
fetch_cards.py → apply_errata.py → card_registry.py → deck_import.py → engine
```

Run in order to rebuild card data from scratch. The engine reads `cards/registry.json`.
