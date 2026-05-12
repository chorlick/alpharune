# Riftbound Simulation Engine

## Project Overview
C++20 game engine that simulates 1v1 Riftbound TCG matches. Takes two deck lists as input, runs games with agent-driven decision-making, outputs structured JSON training data for deep learning.

## Build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && RIFTBOUND_ROOT=.. ./riftbound_tests
```

**Dependencies:** cmake, g++ (C++20), libboost-all-dev, nlohmann-json3-dev, ninja-build

## Coding Standards

### C++ Style
- **C++20** standard, compiled with `-std=c++20`
- **Namespace:** all engine code in `namespace riftbound`
- **Headers:** `#pragma once`, no include guards
- **Naming:** `PascalCase` for types/enums, `camelCase` for methods/variables, `snake_case` for file names, `kConstant` for constants
- **Includes:** project headers use `"core/types.h"` relative to `src/`, system headers use `<>`
- **Strong types:** Use enums and tagged IDs instead of raw ints. `PlayerId`, `GameObjectId`, `BattlefieldId` etc.
- **No raw pointers:** Use references, `std::optional`, containers. Objects live in `GameState::objects` map.
- **Error handling:** `assert()` for invariant violations, `throw` for invalid input, return codes for expected failures (like validation)

### Architecture Patterns
- **Event Bus** (Boost.Signals2): Game actions emit events, subsystems subscribe. This is how triggered abilities, cleanup, logging, and rendering are wired. See `src/core/events.h`.
- **Intent/Command:** Every player action is an `Intent` struct. Engine enumerates legal intents, agent picks one, engine validates and executes. See `src/core/intent.h`.
- **Dependency Injection:** Subsystems (cleanup, combat, renderer, logger) are injected into the engine, not constructed by it. This makes testing easy — inject mocks.
- **Immutable CardDef, mutable GameObject:** `CardDef` (from registry) is static. `GameObject` is the runtime instance with current state.

### Testing
- Google Test framework
- Tests live in `tests/`
- Test files named `test_<module>.cpp`
- Tests for behavioral boundaries, not 100% coverage
- CardDB tests use `RIFTBOUND_ROOT` env var or relative path to find `cards/registry.json`

### Key Rule: Legends are NOT Champions
Legends (`card_type=legend`) and champion units (`card_type=unit, super_type=champion`) are fundamentally different. Never conflate them. They share champion tags but occupy different zones and follow different rules.

## Project Structure
```
automated-riftbound/
├── CLAUDE.md              ← you are here
├── CMakeLists.txt         ← build system
├── src/
│   ├── core/
│   │   ├── types.h/cpp    ✅ All enums, IDs, type aliases
│   │   ├── events.h       ✅ Event bus (Boost.Signals2) + all event types
│   │   ├── intent.h/cpp   ✅ Intent/Command pattern
│   │   ├── game_object.h  ✅ Runtime game object
│   │   ├── game_state.h/cpp ✅ Full game state container
│   │   └── card_db.h/cpp  ✅ Card database from registry.json
│   ├── engine/
│   │   ├── game_engine.h/cpp  ✅ Full turn loop, combat, showdowns, scoring
│   │   ├── chain_manager.h/cpp ✅ FEPR chain resolution
│   │   ├── effect_executor.h/cpp ✅ Atomic game operation helpers (dealDamage, drawCards, etc.)
│   │   └── trigger_manager.h/cpp ✅ Event-driven triggered abilities (uses CardRegistry)
│   ├── cards/
│   │   ├── card.h/cpp         ✅ Card base class + UnitCard/SpellCard/GearCard/etc. subclasses
│   │   ├── card_registry.h/cpp ✅ CardDefId → Card* dispatch (replaces EffectParser)
│   │   └── generated/         ✅ Auto-generated card implementations (787 cards)
│   ├── rules/
│   │   └── deck_validator.h/cpp ✅ Tournament-legal deck validation
│   ├── effects/
│   │   ├── effect_types.h     ✅ EffectScript, EffectStep, TargetSpec, TriggerType
│   │   └── effect_parser.h/cpp ✅ Ability text → structured effects
│   ├── agents/
│   │   ├── agent_interface.h  ✅ Abstract agent interface
│   │   └── random_agent.h     ✅ Random action selection
│   └── io/
│       ├── state_renderer.h/cpp ✅ ASCII board state renderer
│       └── data_serializer.h/cpp ✅ JSON-lines training data output
├── tests/
│   ├── test_types.cpp         ✅ 9 tests passing
│   ├── test_card_db.cpp       ✅ 15 tests passing
│   ├── test_deck_validator.cpp ✅ 13 tests passing
│   └── test_chain.cpp         ✅ 21 tests passing (FEPR, effects, targeting)
├── cards/
│   ├── registry.json      ✅ 787 cards with IDs + 153-dim feature vectors
│   ├── card_index.json    ✅ All card data (errata applied)
│   ├── json/              ✅ Individual card JSONs
│   └── raw/               ✅ Raw gallery data
├── decks/
│   ├── leblanc_test.txt   ✅ Sample deck (Piltover Archive format)
│   ├── vex_test_deck.txt  ✅ Sample deck
│   └── miss_fortune_test.txt ✅ Sample deck
├── rules/
│   ├── core-rules.md      ✅ Full core rules (98 pages, 4048 lines)
│   ├── tournament-rules.md ✅ Tournament rules (46 pages)
│   └── core-rules.pdf     Source PDF
├── errata/                ✅ Official errata documents
├── scripts/
│   ├── fetch_cards.py     ✅ Download cards from gallery
│   ├── apply_errata.py    ✅ Patch card text with errata
│   ├── card_registry.py   ✅ Build registry with IDs + feature vectors
│   └── deck_import.py     ✅ Import + validate deck lists
└── docs/
    ├── engine-design.md   ✅ Full architecture doc (16 sections)
    └── playmat-layout.md  ✅ ASCII board layout reference
```

## Current Status
**Phases 1-3 — Complete**

### What works now
```bash
# Run a game between two decks
./build/riftbound decks/leblanc_test.json decks/vex_test_deck.json --registry cards/registry.json

# Run 100 games
./build/riftbound deck1.json deck2.json -r cards/registry.json --games 100

# Step through interactively (press Enter each decision)
./build/riftbound deck1.json deck2.json -r cards/registry.json --step --show-hand

# Render board state each turn
./build/riftbound deck1.json deck2.json -r cards/registry.json --render

# Output JSON training data
./build/riftbound deck1.json deck2.json -r cards/registry.json -o data/game.jsonl
```

### Completed (✅) — Phase 1
- Project scaffolding (CMake + Ninja, all directories)
- Event bus with 22 game event types (Boost.Signals2)
- Intent/Command pattern with factory methods
- Game object model (GameObject with full state tracking)
- Game state container (PlayerState, BattlefieldState, TurnState, ChainState)
- Card database loader (787 cards from registry.json)
- Deck validator (domain identity, copy limits, signatures, champion tags)
- Game engine with full turn loop (awaken, scoring, channel, draw, main, end)
- Phase 1 card play (units from hand/champion zone, no chain)
- Standard movement (base↔battlefield, ganking)
- Contested status → non-combat showdowns → control → conquer
- Combat (attacker/defender, damage assignment, lethal kills, resolution)
- Hold scoring during beginning phase
- Winning Point rules (hold always works, conquer requires all BFs scored)
- Cleanup processor (win check, lethal damage, battlefield control)
- Random agent (uniform random from legal actions)
- ASCII board state renderer (--render, --step, --show-hand)
- JSON-lines data serializer (full state + legal actions + chosen at each decision)
- main.cpp CLI with Boost.ProgramOptions
- Rune cost payment (Energy from exhausting, Power from recycling matching-domain runes)
- Gear plays from hand to base
- 58 unit tests all passing (37 Phase 1 + 21 chain/effect tests)

### Completed (✅) — Phase 2: Chain & Spells
- ChainManager class with full FEPR loop (Finalize → Execute → Pass → Resolve)
- Permanents routed through chain, resolve immediately at Finalize (CR 337.1.c)
- Priority passing between players during Closed State (N-length chains supported)
- Spell play: Action spells during showdowns, Reaction spells during Closed State
- Showdown loop with focus passing — spells playable during combat and non-combat showdowns
- Combat showdown step runs before damage (units can be killed/bounced pre-damage)
- Targeting system: legal target enumeration, validation on resolution, fizzle on invalid
- Reaction affordability check + cost payment during chain priority passing
- 5 chain events on EventBus (ChainCreated, ItemFinalized, ItemResolved, ChainEmptied, SpellResolved)
- Rendering: bordered layout with hands, trash, champion zone, chain display, decision counters
- Mulligan fix (both players mulligan), recycle shuffle

### Completed (✅) — Phase 3: Effect Parser, Triggers, Abilities, Keywords
- ✅ **Effect executor**: 20+ atomic helpers: dealDamage, drawCards, killObject, bounceToHand, giveTemporaryMight, giveTemporaryKeyword, buffUnit, readyObject, moveToBase, stunUnit, discardCards (agent choice), recycleCards, banishObject, healObject, exhaustObject, channelRunes, revealUntil, playIgnoringCost, counterSpell.
- ✅ **TriggerManager** (`src/engine/trigger_manager.h/cpp`): Subscribes to EventBus, fires matching triggers on all game objects. Covers: WhenYouPlayMe (114 cards), WhenIAttack/Defend (28), WhenIDie/Deathknell (23), score triggers (54), phase triggers (32), move triggers (20), WhenYouPlayAUnit/Spell (11).
- ✅ **Activated abilities**: 71 cards with `[E]:` abilities generate ActivateAbility intents in main phase and showdowns. Exhaust + energy cost payment.
- ✅ **Accelerate keyword**: 24 cards auto-pay extra cost to enter ready instead of exhausted.
- ✅ **Stun mechanic**: `is_stunned` flag, stunned units contribute 0 might in combat, stun clears at expiration step, double-stun prevented.
- ✅ **Deathknell keyword**: `[Deathknell] — EFFECT` as WhenIDie trigger. 23 cards.
- ✅ **Legion keyword**: `[Legion] — EFFECT` as condition gate. Effects only fire if `cards_played_this_turn >= 2`.
- ✅ **Temporary keyword**: Units with Temporary are killed at start of Beginning Phase, before scoring.
- ✅ **Tank/Backline keywords**: Damage assignment reordered — Tank units take damage first, Backline units last.
- ✅ **Vision keyword**: On play, look at top card of deck, auto-recycle spells (agent choice placeholder).
- ✅ **Discard as player choice**: Agent queries via MakeChoice intent for discard selection.
- ✅ **Temporary buff/keyword expiration**: Buffs and keywords applied "this turn" clear at expiration step. Base card keywords preserved.
- ✅ **Dazzling Aurora mechanic**: RevealUntil → Banish → PlayIgnoringCost → Recycle composite effect. ~17 cards with reveal/search patterns.
- ✅ **Level N / XP / Hunt keywords**: `[Hunt N]` parsed as conquer/hold trigger granting N XP. `[Level N][>]` condition gate checking `xp >= N`. GainXP/SpendXP effect types. 42 cards with XP, 14 with Level.
- ✅ **Hidden mechanic**: HideCard intent (pay [A] to hide facedown at controlled BF). Play from facedown gains Reaction on next turn, plays for free through chain. Facedown cards removed on control loss. Render shows `[HIDDEN: P1 — facedown]`. Both normal play and hide shown as legal actions. 37 cards.
- ✅ **Replacement effects**: `killUnit()` checks for "would die...instead" on friendly objects. Heal/exhaust/recall instead of dying. Self-destructing replacements ("kill this instead") handled. 9 cards.
- ✅ **Play-to-location**: Units with "play me to an open/enemy/any battlefield" can play directly to those locations, bypassing normal "base or controlled BF" restriction. ~10 cards.
- ✅ **AoE effects**: Deal/Kill with `all` targets hits all matching objects. AoE spells don't require target selection.
- ✅ **Choose targeting**: "Choose a friendly unit at a battlefield" parsed as `ChooseTarget` step, derives targeting requirements.
- ✅ **Debug logging**: `--debug` CLI flag enables `[DBG]` output via EventBus signals. Shows trigger firings, effect execution, XP gains, replacements.
- ~~Effect parser~~ removed in Phase 4 — replaced by Card Object System.
- 72 tests passing

### Architecture notes for next agent

**Core principle: Card mechanics are encapsulated in Card objects, not the engine.**
The GameEngine handles game flow (phases, turns, chain FEPR loop, cleanup, scoring) and provides atomic operations via EffectExecutor. All card-specific behavior (what a spell does, what a trigger does, what an activated ability does, how counters work) lives in Card subclass `onResolve()`/`onTrigger()`/`onActivate()` overrides. Never add card-specific logic to game_engine.cpp — the engine dispatches to Card objects and the cards tell the engine what to do via the executor helpers. This keeps the engine generic and each card's behavior self-contained, testable, and reviewable.

- **Card objects** (`src/cards/card.h`): Every card has a Card subclass registered in CardRegistry. Card objects override `onResolve()`, `onTrigger()`, `onActivate()` etc. All card-specific behavior — effects, targeting, countering, token creation, damage computation — belongs here, not in the engine.
- **CardRegistry** (`src/cards/card_registry.h/cpp`): Maps CardDefId -> Card*. Loaded ONCE at application startup, shared as `const CardRegistry&` across all game threads. Card objects are stateless — concurrent reads safe. `card_registry_.get(def_id)->onResolve(ctx, targets)` is the dispatch path.
- **Code generation** (`scripts/generate_cards.py`): Reads registry.json, parses ability_text, generates C++ card classes. 209 cards have auto-generated effects. 578 complex cards have partial implementations. Regenerate with `python3 scripts/generate_cards.py`. Manual overrides go in `src/cards/manual/`.
- **EffectExecutor** is a utility library of atomic game operations (dealDamage, drawCards, killObject, bounceToHand, createToken, copyUnit, predict, etc.). Card objects call these via `ctx.executor.*`. The executor does NOT decide WHAT to do — that's the card's job. The executor only knows HOW to modify game state.
- **TriggerManager** uses CardRegistry (`card->triggerType()`) to match events to triggers. Subscribes to EventBus. Also checks `DelayedAbility` list for one-shot delayed triggers.
- **Chain is a LIFO stack** (`std::vector`, resolve from back). When a spell/ability resolves, FEPR pops it, then calls `resolveSpell()` which dispatches to `card->onResolve()`. Counter spells use **peek-and-pop**: the counter's `onResolve` peeks at the new chain top — if it's a spell, pops it and disposes (trash or hand). Counter-of-counter works naturally via LIFO — no flags, no scanning, no chain mutation outside the card's own onResolve.
- **Aura system** uses tagged effects (`AuraEffect` on GameObject), recalculated from scratch during `cleanup()`. No formal layer system. `hasKeyword()` checks both base and aura-granted keywords. 18 aura sources + conditional self-effects evaluated each cleanup pass.
- **Combat damage** — both attacker and defender queried through agent interface. Tank/Backline ordering enforced. Agent picks from greedy-lethal and spread-even distributions.
- **Cost payment** — `payCardCost()` applies CostModifier reductions, then exhausts runes for energy (agent-chosen), then recycles exhausted runes for power (agent-chosen). Energy-first ordering allows recycling just-exhausted runes for power (efficient). Each rune selection is an agent decision point.
- **Threading model** — `BatchRunner` wraps `boost::asio::thread_pool`. `GameRunner` is per-game, fully thread-safe (all state stack-local). Shared singletons: `CardDB` (const), `CardRegistry` (const). Per-game: `EventBus`, `GameState`, `GameEngine`, agents, I/O. `AggregateResults` uses atomics for counters, mutex for console.
- **The `on_decision` callback** fires at every decision point including mulligans, combat damage, and chain priority. Trivial single-option decisions auto-skipped in render.
- **Logging levels**: `--debug` shows trigger firings and ability resolution. `--trace` adds every phase transition, decision, intent, effect, draw, discard, rune exhaust/recycle, damage assignment, kill, score, counter, equip, token, burn out, and mulligan with card names.
- **HTML replay**: `--render` generates `replay_gameN.html` with UUID, seed, board state + decision + trace log panels, arrow-key navigation.

### Lessons learned / pitfalls for next agent
- **Never put card-specific logic in game_engine.cpp.** All card behavior belongs in Card subclass overrides (`onResolve`, `onTrigger`, `onActivate`). The engine dispatches; the card decides what happens. If you find yourself adding an `if (card_name == "X")` or checking `ability_text.find("pattern")` in the engine, you're doing it wrong — put it in the Card object.
- **Counter spells must NOT directly manipulate the chain.** Use peek-and-pop: the counter resolves (popped by FEPR), then pops the next spell. Never set flags or scan the chain to mark items — if the counter is countered first, its onResolve never runs, so the target is naturally preserved.
- **AoE damage must collect targets BEFORE killing.** Deal damage to all targets first, then iterate again to kill lethally damaged units. If you kill during the damage loop, iterator invalidation and missing targets occur (Flurry of Blades, Disintegrate bugs).
- **`addAbility()` must close state** (`oc_state = Closed`). Without this, `generateSpellActions` falls through to `isNeutralOpen()` and allows Action spells during ability chain resolution.
- **Duplicate card names across sets** exist (e.g., "Karma, Channeler" IDs 235 and 548, "Darius, Executioner" IDs 243 and 547). Code-gen handles with `_card_id` suffix. Each needs its own Card object — ML pipeline uses card_def_id for feature vectors.
- **Channeled runes enter READY during channel phase** (CR 316), but card effects that say "channel N runes exhausted" must enter EXHAUSTED. The `channelRunes()` method has an `enter_exhausted` parameter.
- **The mulligan is a single atomic decision** — agent picks which cards (0-2) to set aside in one intent. Cards are removed, replacements drawn, then set-aside recycled. The agent cannot see drawn replacements before deciding. Order: set aside → draw → recycle (CR 118).
- **Replacement effects still use raw `ability_text` matching** in `killUnit()`. This is intentional — replacements intercept game actions, not chain resolution. Future: structured ReplacementEffect type on Card.
- **The render shows board state BEFORE resolution.** A chain item may be visible but its effect hasn't happened yet. The trace log in the HTML replay shows what happens between renders.
- **Discard choice is agent-driven** but random agent picks from back of hand. Opponent-targeting discard ("They discard 1") not yet supported.
- **Never confuse `cardDefId()` with `ctx.source`.** `cardDefId()` is the static card template ID from registry.json (e.g., 601 for Soul Sword). `ctx.source` is the runtime GameObjectId (e.g., 70). Using `cardDefId()` where a GameObjectId is expected causes `getObject()` assertion failures because the ID doesn't exist in the objects map. Always use `ctx.source` to refer to the current card instance in Card object methods.
- **Generated card files are overwritten by `generate_cards.py`**. Manual fixes to generated cards (Flurry of Blades, Disintegrate, Challenge, Wind Wall, Abandon) must be re-applied after regeneration or moved to `src/cards/manual/`.

### Completed (✅) — Phase 4: Card Object System
- ✅ **Card base class** (`src/cards/card.h`): Abstract Card with UnitCard/SpellCard/GearCard/LegendCard/BattlefieldCard/RuneCard subclasses. Virtual methods: onResolve, onTrigger, onActivate, triggerType, getTargetRequirements, enumerateLegalTargets, hasActivatedAbility, getActivationCost, requiresLegion, requiresLevel, hasReplacementEffect.
- ✅ **CardRegistry** (`src/cards/card_registry.h/cpp`): Maps CardDefId -> Card*. Replaces EffectExecutor/EffectScript dispatch entirely.
- ✅ **Code-gen** (`scripts/generate_cards.py`): Python script reads registry.json, parses ability_text, generates C++ card classes. 209 cards fully generated, 578 complex cards have partial implementations.
- ✅ **Engine wiring**: GameEngine, TriggerManager, and legal action generation all dispatch through CardRegistry. EffectExecutor retained as utility library of atomic game operations.
- ✅ **EffectParser removed**: `effect_parser.h/cpp` deleted. `EffectScript` removed from `CardDef`. No more runtime parsing.
- ✅ **Quality audit**: Zero non-complex cards with missed effect verbs. All 72 tests passing. Games run normally.
- 72 tests passing

### Completed (✅) — Phase 5a: Layer-Free Game Mechanics
- ✅ **Burn Out** (CR 431): When deck empty during draw, shuffle trash into deck, lose 1 point. Both GameEngine and EffectExecutor updated.
- ✅ **Token creation**: `EffectExecutor::createToken()` creates token GameObjects on board. Emits `TokenCreatedEvent`. 7 predefined types ready (Recruit, Sprite, Sand Soldier, Mech, Gold, Reflection, Bird). Card objects call `ctx.executor.createToken(...)`.
- ✅ **Cost reduction** (31 cards): `CostModifier` system on PlayerState. `canAfford`/`payCardCost` apply active modifiers. Supports per-turn, next-spell-only, next-unit-only expiration. Minimum cost 0.
- ✅ **Complex activation costs**: Engine pays `[C]` (recycle self), discard costs, and energy costs during ability activation. Legality checks verify affordability.
- ✅ **Predict keyword** (6 cards): `EffectExecutor::predict()` peeks top N cards, agent chooses which to recycle via MakeChoice. Cards put back on top or recycled to bottom.
- ✅ **Delayed abilities** (CR 389-392): `DelayedAbility` struct on GameState. `TriggerManager::checkDelayedAbilities()` fires matching one-shot triggers. Expire at end of turn.
- ✅ **Additional turns** (CR 734-738): Turn queue on PlayerState. `runTurnLoop` checks queue before normal alternation. `is_additional_turn` flag on TurnState.
- ✅ **Battlefield Replace/Swap-back** (CR 438): `replaceBattlefield()` sends original to Banishment, token takes slot. `swapBackBattlefield()` restores original. Uses existing `is_token`/`replaced_card`/`was_replaced` fields.
- ✅ **Equip/Attach** (CR 716-725, basic): `attachGearToUnit()` links gear→unit, applies `might_bonus` via `attachment_might_bonus` field. `detachAllGear()` called on unit death/bounce. `recomputeMight()` includes attachment bonus. Full inactive text system deferred to layer system.
- ✅ **Trace logging**: `--trace` CLI flag shows every game action (phases, decisions, plays, spells, combat, scoring, effects, costs).
- ✅ **Banishment zone render**: Banished cards shown in board render when non-empty.
- ✅ **Hidden card debug reveal**: `--show-hand` reveals hidden card names in render for V&V.
- 72 tests passing

### Completed (✅) — Phase 5b: Tagged Effects (replaces formal layer system)
- ✅ **Aura system**: `recalculateAuras()` in cleanup scans all aura sources, applies/removes tagged effects on affected units. 18 aura cards supported (keyword-granting + might-modifying). `AuraEffect` struct on GameObject tracks source, might bonus, keyword, minimum.
- ✅ **`hasKeyword()` method**: Checks both base keywords AND aura-granted keywords. Used in combat (Tank/Backline), movement (Ganking), and targeting.
- ✅ **Battlefield auras**: Trifarian War Camp, Windswept Hillock, Brush — "Units here have +1M / [Ganking]". Source can be a BF card or a unit.
- ✅ **Conditional self-effects**: "If you've discarded, I have [Ganking]", "While I'm buffed, I have [Ganking]", "While I'm [Mighty], I have [Deflect]". Evaluated during aura recalculation based on game state.
- ✅ **Copy effects**: `EffectExecutor::copyUnit()` copies base traits (might, keywords, tags, name) not computed values. Auras reapply on top during recalculation.
- ✅ **Elder Dragon damage rule**: "Any amount of your damage is enough to kill enemy units" — checked in `processLethalDamage()`. Any damaged enemy unit dies while Elder Dragon is on board.
- ✅ **Equip intent generation**: Gear with `[Equip]` keyword generates equip actions targeting friendly units. Executes `attachGearToUnit()` on activation.
- ✅ **No formal layer system needed**: Tagged effects handle all 787 cards without ordering issues. Keyword auras evaluated before might auras (conditional self-effects in Step 3b, external auras in Step 3).
- ✅ **Combat damage assignment trace**: Both attacker AND defender assign damage. Per-unit breakdown logged with current damage/might. Default greedy-lethal assignment (agent choice integration ready).
- ✅ **Draw card name logging**: Each card drawn logged with name and ID in trace.
- ✅ **Discard card name logging**: Each card discarded logged with name and ID in trace. Sets `has_discarded_this_turn` flag.
- ✅ **Channel runes exhausted**: Card effects that say "channel N runes exhausted" now correctly enter runes exhausted (19 cards). Normal channel phase runes enter ready.
- ✅ **HTML replay viewer**: `--render` generates `replay_gameN.html` with UUID + seed in header. Self-contained browser-based game replay with arrow-key navigation, board state + decision + trace log panels.
- ✅ **Combat damage assignment agent choice**: Both attacker AND defender queried via agent interface. Agent sees greedy-lethal and spread-even options. Assignment fully logged per-unit. ML agents can generate custom distributions.
- ✅ **Rune selection trace logging**: Every rune exhausted for energy and recycled for power logged with name, ID, and state (ready/exhausted). Cost payment fully auditable.
- ✅ **Counter spell fix (Wind Wall, Abandon)**: Counter spells now use peek-and-pop on the chain instead of flags. When a counter resolves, it pops the next spell off the chain. Counter-of-counter works naturally via LIFO — if the counter is countered first, it never gets to pop its target.
- ✅ **Challenge card fix**: Dual targeting (friendly + enemy). Engine generates all valid pairs as intents. `onResolve` deals mutual might-based damage with lethal kill check.
- ✅ **Abandon card fix**: Properly counters the next spell on chain and returns it to owner's hand (not trash). Also runs Predict 1.
- ✅ **Mulligan trace logging**: Opening hand draw and mulligan decisions fully traced with card names. Shows which cards were mulliganed and what replacements were drawn.
- ✅ **Game UUID**: Each game gets a UUID v4. Shown in console output and HTML replay header alongside seed.
- 72 tests passing

### Completed (✅) — Phase 5c: Equip System
- ✅ **Full equip cost payment**: 33 equip gear cards manually implemented in `src/cards/manual/equip_cards.cpp`. Each card handles its own cost via `onEquip()` — domain power recycling, energy exhaustion, special costs (Last Rites: recycle 2 from trash). Uses `SimpleEquipGear` and `UniversalEquipGear` base classes.
- ✅ **Equipment effect text triggers**: `equippedTriggerType()` and `onEquippedTrigger()` on Card base class. TriggerManager checks attached gear when units attack/defend/conquer/hold/move/die. Effect text logic lives in the gear's Card object (e.g., Warmog's Armor buffs on conquer, Trinity Force scores on hold).
- ✅ **Equipment-granted keywords**: `equippedKeywords()`, `equippedAssault()`, `equippedShield()` on Card. Applied during aura recalculation (Step 3c). Serrated Dirk grants [Assault 2], Cloth Armor grants [Shield 2], Boots of Swiftness grants [Ganking], etc.
- ✅ **Weaponmaster keyword**: 9 Weaponmaster units in `src/cards/manual/weaponmaster_cards.cpp`. On-play trigger: find Equipment on board, detach from current unit if needed, attach to self for free ([A] less cost).
- ✅ **Render shows attached equipment**: Unit render includes `EQ:GearName` suffix when gear is attached.
- ✅ **Token safety guards**: All `card_db_.get(obj.card_def_id)` calls guarded with `card_def_id != kInvalidId` check. Prevents crash when tokens (no CardDef) are on board.
- ✅ **CardDefId vs GameObjectId crash fix**: `cardDefId()` (static card template ID, e.g., 601) was being passed where `ctx.source` (runtime instance ID, e.g., 70) was needed in equip cost payment. Caused assertion failures in `getObject()` for Ornn, Rengar, and VexPreCon decks. Fixed by using `ctx.source` in all `standardEquip()` and `UniversalEquipGear::onEquip()` calls.
- ✅ **Full round-robin verified**: All 15 deck matchups (5 decks x 5 decks, including mirrors) complete with zero crashes, 72 tests passing.
- 72 tests passing

### Completed (✅) — Phase 5d: Agent Decisions & Manual Cards
- ✅ **Rune selection agent choice**: Both energy exhaustion and power recycling query agent via MakeChoice. Order: exhaust first (for energy), then recycle exhausted runes for power (efficient). Each rune choice is a decision point for ML training. Tagged `[agent choice]` in trace.
- ✅ **25 manual card implementations** (`src/cards/manual/deck_cards.cpp`): Champions (LeBlanc Deathknell draw, Ahri hold=score), Legends (Gloomist hold=draw, Deceiver Reflection tokens, Fire Below the Mountain [Add][A]), Deathknell units (Ruined Rex deal 4, Black Rose channel, Glasc Mixologist play from trash), Score triggers (Sona ready 4 runes), Spells (Cull the Weak mutual kill, Defy counter+draw, Sprite Burst 2 tokens), Gear (Seal of Focus/Strength, Sprite Fountain tokens), Units (Sprite Mother token on play, Pit Rookie buff).
- ✅ **Full round-robin re-verified**: All 15 matchups (5 decks), zero crashes, card effects working (Ahri scoring, Sona rune-readying, Deathknell triggers, token creation all visible in traces).
- 72 tests passing

### Manual Card Coverage Summary
| File | Cards | Scope |
|------|-------|-------|
| `src/cards/generated/*.cpp` | 787 | All cards (209 fully generated, 578 partial) |
| `src/cards/manual/equip_cards.cpp` | 33 | Equipment gear with cost payment + triggers |
| `src/cards/manual/weaponmaster_cards.cpp` | 9 | Weaponmaster units (on-play equip) |
| `src/cards/manual/deck_cards.cpp` | 25 | Champions, legends, key units/spells in test decks |
| **Total manually implemented** | **67** | Overwrite generated stubs with full behavior |

### Remaining Work — Polish for v1.0

#### Do-nothing cards in test decks (12 remaining)
These cards are in the 5 test decks but currently have no implemented effect. Priority: implement before ML training.
- [ ] **[467] Vex, Cheerless** — "While I'm in combat, friendly spells cost [1][A] less, enemy spells cost [1][A] more." Needs dynamic cost modification during combat showdowns.
- [ ] **[476] Honest Broker** — "When you play me, if you have 3+ cards in hand, draw 1." Conditional on-play trigger with hand-size check.
- [ ] **[449] Overzealous Fan** — "When I defend, you may kill me to move an attacking unit to its base." Self-sacrifice combat trigger.
- [ ] **[26] Brynhir Thundersong** — "When I attack, deal 2 to all enemy units here." Combat AoE trigger.
- [ ] **[344] Ferrous Forerunner** — "[Deathknell] — Play a 3 [M] Mech unit token here." Token on death.
- [ ] **[610] Trevor Snoozebottom** — "When you play me, give enemy units here -1 [M] this turn." On-play debuff.
- [ ] **[612] Iascylla** — "When I hold, at start of your next Main Phase, move an enemy unit here." Delayed movement.
- [ ] **[609] Mosstomper** — Keywords + conditional state.
- [ ] **[615] Scuttle Crab** — "When I move, channel 1 rune exhausted." Move trigger.
- [ ] **[597] Monch** — "When you play me, draw 1 if you control a Fae." Conditional draw.
- [ ] **[603] Allay, Eager Admirer** — Aura: "Units here have [Deflect]." (already handled by aura system, verify)
- [ ] **[605] Enthusiastic Promoter** — "[Backline] When I hold, [Buff] all units here." Hold trigger + AoE buff.

#### Mechanical features
- [x] **Ambush keyword**: Units with [Ambush] playable as Reactions during Closed State and Showdowns to BFs where you have units. Enter BF ready. 12 cards. Intent generation in both generateClosedStateActions and generateShowdownActions.
- [x] **Equip inactive text**: `is_rules_text_inactive` flag on GameObject. Set when gear attaches, cleared on detach. TriggerManager skips triggers on inactive gear (CR 718.2).
- [x] **Discard opponent targeting**: `opponentDiscards()` method on EffectExecutor. Agent query goes to opponent player. Card objects call `ctx.executor.opponentDiscards(opponent(ctx.controller), N)`.
- [x] **Reveal/search agent choices**: `revealAndChoose()` on EffectExecutor. Agent sees each revealed card and chooses draw or recycle. Trace logs each choice.
- [x] **Combat damage distribution expansion**: Now 4 options: greedy-lethal, spread-even, focus-all-on-first, lethal-first-rest-on-second.
- [x] **Thrill of the Hunt**: Full banish→play-from-banishment→any-BF-ready flow. Removes from banishment zone, places on BF ready.
- [x] **Quick-Draw keyword**: Gear with [Quick-Draw] playable as Reactions during Closed State targeting friendly units. Auto-attaches to target unit on resolution via `attachGearToUnit()`.
- [ ] **Additional manual cards**: 578 complex cards total, 68 manually implemented. Priority by tournament frequency and test deck usage.

### Completed (✅) — Phase 6a: Batch Game Runner
- ✅ **Parallel execution via `boost::asio::thread_pool`**: `BatchRunner` posts `GameRunner` work units to pool. `--threads N` CLI option (default: 1, 0 = hardware concurrency).
- ✅ **Manual DI via `AppContext`**: `CardDB` and `CardRegistry` loaded once as shared singletons (const after init). Per-game `EventBus`, `GameEngine`, agents constructed on worker threads. Zero cross-thread mutable state.
- ✅ **`GameRunner`** (`src/engine/game_runner.h/cpp`): Encapsulates all per-game logic — EventBus, engine, renderer, serializer, replay writer, agents. Thread-safe: all state is stack-local.
- ✅ **`BatchRunner`** (`src/engine/batch_runner.h/cpp`): Wraps `boost::asio::thread_pool`. Single-threaded fallback when `--threads 1`. Progress indicator for batches >20 games.
- ✅ **`AggregateResults`**: Atomic counters (p1_wins, p2_wins, draws, total_turns, total_decisions). `std::mutex` for console output. No lock contention on game logic.
- ✅ **CardRegistry refactored to shared const**: `GameEngine` takes `const CardRegistry&` instead of owning a value. `loadAll()` called once at startup. Card objects are stateless — concurrent reads safe.
- ✅ **Per-game JSONL output**: Each game writes to its own file (`output_path.gameN`). No file contention across threads.
- ✅ **Performance**: 1 thread ~18 games/sec, 4 threads ~43 games/sec (2.4x speedup on mixed workload).
- 72 tests passing, all deck matchups verified.

### Phase 6b — ML Pipeline (NEXT)
- **ML training data pipeline**: State→153-dim feature vector (from registry.json) + board state tensor → model → action selection. JSONL output already captures full state + legal actions + chosen at each decision point.
- **Supervised learning baseline**: Train value/policy network on random-vs-random games (~50K games to beat random).
- **Self-play RL agent**: PPO/A2C on game outcomes.
- **MCTS/AlphaZero agent**: Neural network evaluation + Monte Carlo tree search. Requires game state hashing.
- **Match runner**: Best-of-3 with sideboarding and battlefield rotation (CR 481).
- **Performance optimization**: Release build, object pooling, incremental aura recomputation, state hashing for MCTS.

## Key Design Docs
- `docs/engine-design.md` — comprehensive engine architecture (read this first)
- `docs/ml-training-design.md` — ML agent training pipeline: phases, Elo, evolutionary framework, compute estimates
- `docs/deck-agent-design.md` — deck construction agent: evolutionary optimization, ban list, card frequency
- `docs/playmat-layout.md` — physical board layout → ASCII renderer reference
- `rules/core-rules.md` — full game rules (sections 000-826)
- `rules/tournament-rules.md` — deck construction + tournament policies
- `cards/ban-list.csv` — banned cards (SET,ID,'DISPLAY NAME' — single-quoted names due to commas)

## Data Pipeline
```
fetch_cards.py → apply_errata.py → card_registry.py → deck_import.py → engine
```
Run in order to rebuild card data from scratch. The engine reads `cards/registry.json`.

## Important Game Rules for Engine
- **Victory Score:** 8 points (1v1)
- **Battlefield Count:** 2 at start, can grow via token battlefields
- **Winning Point (CR 466.1.b):** Via Hold = always works. Via Conquer = must have scored EVERY battlefield (including tokens) this turn. Via card effect = no restrictions.
- **Cleanup (CR 319):** Fires after nearly every state change. Reentrant. 9-step process.
- **Units enter exhausted** (CR 143.4) unless Accelerate or similar.
- **Gear enters ready** (CR 149.1).
- **Rune pools empty** at end of draw phase and end of turn.
- **Replace (CR 438):** Original goes to Banishment, token takes its place, can swap back.
