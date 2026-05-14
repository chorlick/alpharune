# Riftbound Simulation Engine

## Table of Contents

- [Project Overview](#project-overview)
- [Build](#build)
- [Coding Standards](#coding-standards)
  - [C++ Style](#c-style)
  - [Architecture Patterns](#architecture-patterns)
  - [Testing](#testing)
  - [Key Rule: Legends are NOT Champions](#key-rule-legends-are-not-champions)
- [Project Structure](#project-structure)
- [Current Status](#current-status)
  - [What works now](#what-works-now)
  - [Phase 1 — Core Engine](#completed--phase-1)
  - [Phase 2 — Chain & Spells](#completed--phase-2-chain--spells)
  - [Phase 3 — Effects, Triggers, Keywords](#completed--phase-3-effect-parser-triggers-abilities-keywords)
  - [Architecture notes for next agent](#architecture-notes-for-next-agent)
  - [ML feature expansion directive](#ml-feature-expansion-directive)
  - [Lessons learned / pitfalls](#lessons-learned--pitfalls-for-next-agent)
  - [Phase 4 — Card Object System](#completed--phase-4-card-object-system)
  - [Phase 5a — Layer-Free Mechanics](#completed--phase-5a-layer-free-game-mechanics)
  - [Phase 5b — Tagged Effects](#completed--phase-5b-tagged-effects-replaces-formal-layer-system)
  - [Phase 5c — Equip System](#completed--phase-5c-equip-system)
  - [Phase 5d — Agent Decisions & Manual Cards](#completed--phase-5d-agent-decisions--manual-cards)
  - [Manual Card Coverage](#manual-card-coverage-summary)
  - [Remaining Work — Polish for v1.0](#remaining-work--polish-for-v10)
  - [Phase 6a — Batch Game Runner](#completed--phase-6a-batch-game-runner)
  - [Phase 6b — ML Pipeline](#phase-6b--ml-pipeline)
  - [Phase 7 — Feature Expansion](#phase-7--feature-expansion-470--808-dims)
  - [Phase 8 — Self-Play RL](#phase-8--self-play-rl)
  - [Phase 9 — Cross-Archetype League](#phase-9--cross-archetype-league)
  - [Phase 10 — Memory-Augmented Agents](#phase-10--memory-augmented-agents)
  - [Future Work](#future-work)
- [Key Design Docs](#key-design-docs)
- [Data Pipeline](#data-pipeline)
- [Important Game Rules for Engine](#important-game-rules-for-engine)

## Project Overview
C++20 game engine that simulates 1v1 Riftbound TCG matches. Takes two deck lists as input, runs games with agent-driven decision-making, outputs structured JSON training data for deep learning.

## Build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && RIFTBOUND_ROOT=.. ./riftbound_tests
```

**Dependencies:** cmake, g++ (C++20), libboost-all-dev, nlohmann-json3-dev, ninja-build, ONNX Runtime (fetched by CMake)
**Python (for training):** `conda activate riftbound && pip install -r scripts/requirements.txt` (torch, numpy, onnx, onnxruntime, tensorboard)

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
│   │   ├── random_agent.h     ✅ Random action selection
│   │   └── model_agent.h/cpp  ✅ ONNX-based ML agent (per-action scoring)
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
│   ├── ban-list.csv       ✅ Banned cards (SET,ID,'DISPLAY NAME')
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
│   ├── deck_import.py     ✅ Import + validate deck lists
│   ├── generate_cards.py  ✅ Code-gen C++ card classes from registry.json
│   ├── generate_training_data.sh ✅ Batch game gen for ML training
│   ├── train_agent.py     ✅ Per-action scoring model trainer (PyTorch + ONNX)
│   ├── requirements.txt   ✅ Python deps (torch, numpy, onnx, onnxruntime, tensorboard)
│   └── environment.yml    ✅ Conda environment specification (Python 3.12 + CUDA)
├── models/                ✅ Trained model checkpoints (.pt) and ONNX exports (.onnx)
└── docs/
    ├── engine-design.md   ✅ Full architecture doc (17 sections)
    ├── ml-training-design.md ✅ ML pipeline design (phases, Elo, evolution)
    ├── deck-agent-design.md  ✅ Deck construction agent design
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

# Run with ML agent vs random
./build/riftbound deck1.json deck2.json -r cards/registry.json --agent1 model:models/miss_fortune/v001.onnx --agent2 random

# Generate training data (10K games, 8 threads)
./scripts/generate_training_data.sh decks/miss_fortune_test.json decks/miss_fortune_test.json 10000 8

# Train a model (conda activate riftbound first)
python scripts/train_agent.py train training_data/bounty_hunter_vs_bounty_hunter/ \
    --output models/miss_fortune/v001.pt --epochs 15 --batch-size 2048 --hidden-dim 512 --gpu 0
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
- **ML feature pipeline** — State features extracted by a single shared C++ module (`src/ml/feature_extractor.cpp`, namespace `riftbound::ml`) used by both online inference (`ModelAgent`) and the offline binary training-data writer (`BinaryDataSerializer`). Python (`scripts/train_agent.py`) re-implements the same layout from JSONL records, kept in lockstep via `scripts/parity_check.py` which asserts byte-for-byte equality between the two. **4407-dim vector** = 354 base + 116 Tier 1 + 3935 zone multi-hot + 2 turn-flag (perfect-information encoding of self deck/trash/banishment + opp trash/banishment per CR; opp's deck and hand stay hidden). Padded to `RESERVED_STATE_DIM=4608`. Base layout (0..353): 10 global (turn, phase, is_turn_player, went_first, score_diff, chain_length, ns_state, oc_state, is_additional_turn, delayed_ability_count) + 8 chain items (4 × source_def_id + is_self) + 70 per player × 2 (resources, champion/legend, power by domain, hand IDs + costs, rune counts + domain breakdown ready/exhausted, base unit stats + top 3 IDs, gear count + top 2 IDs, trash/banishment sizes, has_discarded, bfs_scored, num_bfs_controlled, legion_active, cost_modifier_count) + 49 per battlefield × 4 (controller, contested, combat, showdown, facedown, attacker_is_self, combat_phase, is_token, per-side 20 features: counts/keywords + top 3 individual unit def_id/might/damage). Tier 1 appendix (354..469): 8 chain target def_ids + 4 cost-mod type flags + 8 per-BF scored flags + 96 per-unit extended (top-3 per side per BF × {combat_designation, attachment_def_id, might_delta, temp_might_bonus}). Zone multi-hot (470..4404): 5 × 787 count vectors for self deck/trash/banishment + opp trash/banishment. Turn-flag block (4405..4406): self/opp `cant_play_cards_this_turn` (Brynhir Thundersong lockout). State is padded to `RESERVED_STATE_DIM=4608` at the trainer's input for forward-compatible `--resume`. Action features: 25-dim (14 type one-hot + card/targets/source/destinations/mulligan + units_to_move + chosen_object + chosen_bf). Both sides MUST produce identical feature vectors — update C++, Python, and the JSONL serializer in lockstep, then run `python scripts/parity_check.py`.
- **Per-action scoring model** — `RiftboundAgent` (PyTorch): state_encoder MLP → state embedding, concatenated with each action's 21-dim features, scored by action_scorer MLP. Value head from state alone predicts win probability. ONNX export has 3 inputs: `state_features`, `action_features`, `action_mask`. C++ `ModelAgent` runs inference via ONNX Runtime.
- **Training data pipeline** — `DataSerializer` writes JSONL with full state + all legal actions + chosen_index at every decision point. Python loads via memmap (disk-backed, no OOM on large datasets). Two-pass: count decisions, then fill pre-allocated arrays.
- **Coin toss** — `TurnState::starting_player` records who won the coin flip (CR 116). Serialized as `starting_player` in JSONL. Used as `went_first` feature in ML.

### ML feature expansion directive
**Any new game mechanic, decision point, or state change added to the engine MUST also be serialized in the JSONL training data (`DataSerializer`) and featurized in both Python (`train_agent.py`) and C++ (`model_agent.cpp`).** If a field affects gameplay decisions, it must reach the model. See `docs/additional-gamestate-dims.md` for the backlog of unfeaturized state (25 items across 3 priority tiers, ~450 additional features identified). Feature parity between Python and C++ is mandatory — update both in the same change.

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
- **ML feature parity is critical.** Python `extract_state_features()` and C++ `ModelAgent::extractStateFeatures()` must produce identical feature vectors. If you add a feature to one, add it to the other in the same position. Test by comparing Python output on a JSONL record against C++ output on the same game state. Mismatches cause silent model degradation.
- **Dataset OOM**: Python lists of float objects use 28 bytes each (vs 4 for numpy). Always use pre-allocated numpy arrays or memmap for large datasets. Never accumulate millions of Python lists during data loading.

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
| `src/cards/manual/deck_cards.cpp` | 34 | Champions, legends, key units/spells in test decks (incl. Aurora, MF Captain, Elder Dragon, Baron Nashor, Bullet Time, Brynhir, Forerunner, Noxus Hopeful, Pouncing) |
| **Total manually implemented** | **76** | Overwrite generated stubs with full behavior |

### Remaining Work — Polish for v1.0

#### Do-nothing cards in test decks (~42 remaining; 8 fixed this session — see ✅ markers below)
Audited all 10 test decks. These cards have ability text but no working implementation (empty stubs or comment-only generated code). 2 keyword-only cards (Mutated Mouser, Rengar Unseen) are false positives — engine handles their keywords already. Priority: champions/legends first, then high-frequency cross-deck cards.

**Champions & Legends (11 cards — highest priority, distort training)**
- [x] **[162] Miss Fortune, Captain** — move→ready something else. Decks: miss_fortune_test  ✅ uses new `WhenAFriendlyUnitMovesToFB` trigger
- [ ] **[28] Draven, Showboat** — might = your points. Decks: draven_test
- [x] **[348] Rengar, Pouncing** — Reaction play to attacking BF. Decks: rengar_test  ✅ uses new `Card::playableAsReactionToAttack()` hook
- [ ] **[543] Sett, Brawler** — on-play/conquer buff, spend buff for +4M. Decks: khazix_test, sett_test
- [ ] **[644] Lillia, Fae Fawn** — on-move create Sprite token. Decks: lilina_test
- [ ] **[705] Kha'Zix, Mutating Horror** — Ambush, combat +2M if enemy alone, gain 2 XP. Decks: khazix_test, sett_test
- [ ] **[552] Glorious Executioner** (legend) — win combat draw 1. Decks: draven_test
- [ ] **[744] Pridestalker** (legend) — on unit play, give a unit +1M. Decks: rengar_test
- [ ] **[749] Bashful Bloom** (legend) — activated: play Sprite token, costs less per Temporary unit. Decks: lilina_test
- [ ] **[787] Voidreaver** (legend) — win combat gain XP, activated XP-spend abilities. Decks: khazix_test, sett_test
- [ ] **[262] Bounty Hunter** (legend) — activated: give unit Ganking. Decks: miss_fortune_test

**Counter/Reaction spells (6 cards)**
- [ ] **[368] Not So Fast** — counter spell/ability targeting friendly. Decks: ornn_test, vex_test_deck
- [ ] **[457] Hard Bargain** — counter unless they pay [2], Repeat. Decks: khazix_test, miss_fortune_test, sett_test
- [ ] **[668] Repulse** — counter spell targeting a friendly unit. Decks: miss_fortune_test, rengar_test
- [ ] **[737] Tactical Retreat** — replacement: would die → heal+exhaust+recall. Decks: leblanc_test
- [ ] **[750] Lilting Lullaby** — counter + opponent can't play spells this turn. Decks: lilina_test
- [ ] **[693] Abandon** — counter, return to hand, Predict. Decks: miss_fortune_test (note: manual impl exists but check coverage)

**Spells with effects (10 cards)**
- [ ] **[156] Sabotage** — reveal opponent hand, recycle a non-unit. Decks: miss_fortune_test
- [~] **[263] Bullet Time** — pay any [A] to deal that much AoE. Decks: miss_fortune_test  ⚠️ partial: spends all energy at resolve, single-target. Full variable-X enumeration deferred.
- [ ] **[484] Deathgrip** — kill friendly, give its might to another, draw 1. Decks: leblanc_test
- [ ] **[657] Grim Resolve** — +3M, if wins combat gain 2 XP. Decks: khazix_test, sett_test
- [ ] **[727] Shadow's Call** — give Temporary, draw 2. Decks: leblanc_test
- [ ] **[735] Sacrifice** — kill friendly Mighty unit, draw 2, channel 1 rune. Decks: leblanc_test
- [ ] **[690] Star-Crossed** — return a friendly + enemy unit to hands. Decks: draven_test, vex_test_deck
- [ ] **[696] Existential Dread** — Repeat, stun attacker or bounce if already stunned. Decks: vex_pre_con, vex_test_deck
- [ ] **[600] Skyward Strike** — move enemy unit, Level 6: stun instead. Decks: vex_pre_con
- [ ] **[758] Void Assault** — move friendly then move enemy. Decks: khazix_test, sett_test

**Units with triggers/effects (17 cards)**
- [ ] **[27] Darius, Trifarian** — second card played: +2M and ready. Decks: draven_test
- [x] **[12] Noxus Hopeful** — Legion cost reduction. Decks: draven_test, rengar_test  ✅ uses new `Card::selfCostReduction()` hook
- [x] **[26] Brynhir Thundersong** — on play, opponents can't play cards this turn. Decks: rengar_test  ✅ uses new `PlayerState::cant_play_cards_this_turn` flag, surfaced as state features at positions 4405–4406
- [ ] **[192] Mindsplitter** — on play, opponent reveals hand, discard a card. Decks: draven_test, miss_fortune_test
- [ ] **[236] Karthus, Eternal** — Deathknell effects trigger additional time. Decks: leblanc_test
- [x] **[344] Ferrous Forerunner** — Deathknell: play two 3M Mech tokens. Decks: draven_test, rengar_test  ✅
- [ ] **[352] Rek'Sai, Breacher** — friendly units from non-hand have Accelerate. Decks: draven_test
- [ ] **[449] Overzealous Fan** — when defend, may kill self to bounce attacker. Decks: draven_test, khazix_test, sett_test, vex_test_deck
- [ ] **[451] Treasure Hunter** — on move, play Gold gear token. Decks: draven_test
- [ ] **[461] Fizz, Trickster** — on play, play spell from trash ignoring cost. Decks: draven_test
- [ ] **[476] Honest Broker** — Deathknell: play Gold gear token. Decks: leblanc_test
- [ ] **[583] Grim Apothecary** — Ambush, on play bounce friendly unit. Decks: rengar_test
- [ ] **[674] Irresistible Faefolk** — on move to BF, move enemy unit there too. Decks: khazix_test, rengar_test, sett_test
- [x] **[680] Elder Dragon** — your damage always kills, on play deal 1 to enemy at each location. Decks: miss_fortune_test  ✅ on-play AoE implemented; "any damage kills" passive was already engine-side
- [x] **[709] Baron Nashor** — creates Baron Pit BF token, can't be targeted, on play deal 3 AoE. Decks: miss_fortune_test  ✅ AoE damage implemented; BF token mechanic deferred
- [ ] **[778] Plundering Poro** — on conquer, play Gold gear token. Decks: lilina_test
- [ ] **[687] Lunar Boon** — discard 1, draw 2. Decks: miss_fortune_test

**Vex/XP-themed units (partially generated — Hunt XP works but Level effects broken) (6 cards)**
- [ ] **[596] Herald of Spring** — Hunt + on play gain 2 XP (generated gives 1, should be 2). Decks: vex_pre_con
- [ ] **[602] Wuju Apprentice** — Hunt + Level 6: draw 1 (generated gives XP instead of draw). Decks: vex_pre_con
- [ ] **[609] Mosstomper** — Hunt 2 + Level 3: +1M and Deflect (generated gives XP instead of keywords). Decks: vex_pre_con
- [ ] **[656] Gemhand Hunter** — Hunt + Level 6: +1M (generated gives XP instead of might). Decks: khazix_test, sett_test
- [ ] **[675] Master Yi, Tempered** — Hunt 2 + Level 6: Deflect+Ganking (generated gives XP instead). Decks: khazix_test, sett_test
- [ ] **[689] Mister Root** — Accelerate + on move gain 2 XP (generated works but verify). Decks: vex_pre_con

**Vex deck complex units (10 cards — vex_pre_con heavy)**
- [ ] **[467] Vex, Cheerless** — combat cost modification (friendly spells -1, enemy +1). Decks: vex_pre_con, vex_test_deck
- [ ] **[597] Monch** — conditional cost reduction + enter ready if opponent has stunned unit. Decks: vex_pre_con
- [ ] **[603] Allay, Eager Admirer** — aura: units here have Deflect. Decks: vex_pre_con
- [ ] **[605] Enthusiastic Promoter** — on hold, Buff all units here. Decks: vex_pre_con
- [ ] **[610] Trevor Snoozebottom** — on hold, play 3M Sprite token with Temporary. Decks: vex_pre_con
- [ ] **[612] Iascylla** — on hold, delayed: move enemy unit here next main phase. Decks: vex_pre_con
- [ ] **[614] Nami, Headstrong** — optional additional cost, stun on play, on hold next spell costs less. Decks: vex_pre_con
- [ ] **[617] Vex, Mocking** — Shield+Tank + when you stun enemy, move me there. Decks: vex_pre_con
- [ ] **[688] Megatusk** — spend 3 XP: give units here Ganking. Decks: vex_pre_con
- [ ] **[703] Evelynn, Entrancing** — Hidden+Backline, from facedown: move enemy unit. Decks: vex_pre_con
- [ ] **[752] Shadow** — enters ready to BF, activated: stun attacker. Decks: vex_pre_con, vex_test_deck

**Gear (5 cards)**
- [ ] **[465] Spirit Wheel** — when you choose friendly unit, pay [1] + exhaust to draw. Decks: khazix_test, sett_test
- [ ] **[671] Blood Rose** — on unit play pay [1] for XP, spend 3 XP + exhaust: ready a unit. Decks: khazix_test, sett_test
- [ ] **[695] Blast Cone** — on play move enemy, exhaust to stun moved enemy. Decks: vex_pre_con
- [ ] **[698] Scryer's Bloom** — enters exhausted, kill+pay+exhaust: Predict 2, draw 1, gain XP. Decks: vex_pre_con
- [ ] **[375] Heart of Dark Ice** — activated: give unit +3M this turn. Decks: lilina_test, ornn_test

**Per-deck impact summary:**
| Deck | Do-nothing | Key missing cards |
|------|-----------|-------------------|
| vex_pre_con | 26 | Vex champions, Nami, Evelynn, all XP units, Shadow |
| draven_test | 16 | Draven champion, legend, Darius, Fizz, Rek'Sai (Noxus Hopeful, Forerunner ✅) |
| khazix_test | 17 | Kha'Zix, Sett, Master Yi, Voidreaver legend |
| sett_test | 17 | Same cards as khazix (shared pool) |
| miss_fortune_test | 10 | MF legend (Bounty Hunter — Ganking activation), Sabotage, Mindsplitter, Stacked Deck, Mobilize, Catalyst, Repulse (MF Captain, Elder Dragon, Baron Nashor, Bullet Time ✅; Aurora ✅) |
| rengar_test | 6 | Pridestalker legend, Kai'Sa, Grim Apothecary, Irresistible Faefolk, Repulse (Pouncing, Noxus Hopeful, Brynhir, Forerunner ✅) |
| lilina_test | 9 | Lillia champion, Bashful Bloom legend |
| vex_test_deck | 9 | Vex, Shadow, Mutated Mouser |
| leblanc_test | 7 | Karthus, Tactical Retreat, Deathgrip |
| ornn_test | 4 | Scuttle Crab, Not So Fast |

#### Mechanical features
- [x] **Ambush keyword**: Units with [Ambush] playable as Reactions during Closed State and Showdowns to BFs where you have units. Enter BF ready. 12 cards. Intent generation in both generateClosedStateActions and generateShowdownActions.
- [x] **Equip inactive text**: `is_rules_text_inactive` flag on GameObject. Set when gear attaches, cleared on detach. TriggerManager skips triggers on inactive gear (CR 718.2).
- [x] **Discard opponent targeting**: `opponentDiscards()` method on EffectExecutor. Agent query goes to opponent player. Card objects call `ctx.executor.opponentDiscards(opponent(ctx.controller), N)`.
- [x] **Reveal/search agent choices**: `revealAndChoose()` on EffectExecutor. Agent sees each revealed card and chooses draw or recycle. Trace logs each choice.
- [x] **Combat damage distribution expansion**: Now 4 options: greedy-lethal, spread-even, focus-all-on-first, lethal-first-rest-on-second.
- [x] **Might reduction minimum**: `giveTemporaryMight()` now accepts `minimum` parameter (e.g., "-4M to a minimum of 1M"). Enforced by correcting buff_count after recompute.
- [x] **Cost reduction minimum**: `CostModifier.min_cost` field. Cost reductions floor at min_cost instead of 0 (e.g., "cost [1] less to a minimum of [1]").
- [x] **Thrill of the Hunt**: Full banish→play-from-banishment→any-BF-ready flow. Removes from banishment zone, places on BF ready.
- [x] **Quick-Draw keyword**: Gear with [Quick-Draw] playable as Reactions during Closed State targeting friendly units. Auto-attaches to target unit on resolution via `attachGearToUnit()`.
- [x] **`WhenAFriendlyUnitMovesToFB` trigger type**: Fires on all friendly cards with this trigger when any friendly unit moves to a BF (vs `WhenIMoveToFB` which only fires on the moving unit). Used by Miss Fortune Captain.
- [x] **`PlayerState::cant_play_cards_this_turn` flag**: Turn-scoped lockout used by Brynhir Thundersong. Gates legal-action generation in main / showdown / closed-state paths. Resets in `resetTurnTracking()`. Exposed to model at state features 4405–4406.
- [x] **`Card::selfCostReduction(state, player)` hook**: Per-card self-discount applied in `canAfford`/`payCardCost`. Used by Noxus Hopeful for Legion discount (cards_played_this_turn ≥ 1 → -2 energy).
- [x] **`Card::playableAsReactionToAttack()` hook**: Action generators emit play-to-attacking-BF intents in showdown/closed-state when this returns true. Used by Rengar, Pouncing.
- [x] **kInvalidId guards in engine**: Defensive guards in `executePlayCard`, `canAfford`, `payCardCost`, main-phase action generator, and the ActivateAbility handler. Prevents tokens (which have no `CardDef`) from crashing the engine when iterated over by action generators or referenced in intents.

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

### Phase 6b — ML Pipeline

#### Completed
- ✅ **Training data generation**: `scripts/generate_training_data.sh` — builds Release, runs batch games, outputs per-game JSONL files. Usage: `./scripts/generate_training_data.sh decks/miss_fortune_test.json decks/miss_fortune_test.json 10000 8`
- ✅ **Python training script**: `scripts/train_agent.py` — loads binary `.bin` files directly via `RiftboundBinaryDataset` (mmap, no Pass-2), extracts 4407 state features, trains per-action scoring MLP (policy + value heads), incremental checkpointing, thermal backoff, ONNX export. Supports train/eval/train-rl commands, dual GPU (`--gpu 0` or `--gpu 1`). Legacy JSONL path also supported.
- ✅ **Per-action scoring model**: `RiftboundAgent` scores each specific legal action (card + target + action type). Input: state features (4407 dims, padded to 4608) + per-action features (25 dims each, up to 64 actions) + action mask. Output: one score per action + win probability. Replaces v1 action-type-only classifier.
- ✅ **4407-dim state features** (354 base + 116 Tier 1 + 3935 zone multi-hot + 2 turn-flags): see "ML feature pipeline" note above for full layout, and `docs/additional-gamestate-dims.md` for fixed position assignments.
- ✅ **Shared C++ feature extractor** (`src/ml/feature_extractor.{h,cpp}`): single source of truth for state and action featurization. Used by both `ModelAgent` (online inference) and `BinaryDataSerializer` (offline training data). Eliminates the silent C++/Python feature-drift class of bugs.
- ✅ **Binary training-data format** (`src/io/binary_data_serializer.{h,cpp}`, magic `RFBA`): per-decision records with pre-extracted features. `RiftboundBinaryDataset` in `train_agent.py` mmaps these files directly — no JSONL parse, no Pass-2 feature extraction, no temp-dir memmap. Output dispatch is by extension: `-o foo.bin` → binary, anything else → JSONL (legacy).
- ✅ **Parity check** (`scripts/parity_check.py`): runs engine twice (JSONL + binary) on same seed, asserts byte-for-byte feature equality. Mandatory before training on any new feature-vector layout.
- ✅ **21-dim action features**: Action type one-hot (14), card_def_id, target_def_ids (×2), ability_source_def_id, dest_bf, play_bf, mulligan_count.
- ✅ **Coin toss**: `determineTurnOrder()` randomizes starting player (CR 116). `TurnState::starting_player` tracked in game state and serialized to JSONL as `starting_player`.
- ✅ **ModelAgent C++ class** (`src/agents/model_agent.h/cpp`): Loads ONNX model, extracts state features from live `GameState` via the shared `riftbound::ml` module (currently 4407 dims, padded to 4608), featurizes each legal action (25 dims), runs inference via ONNX Runtime, picks highest-scoring action (or samples with temperature > 0 for self-play data generation). No random fallback.
- ✅ **ONNX export**: 3 inputs (state_features, action_features, action_mask) with dynamic batch axes. ONNX Runtime v1.21.0.
- ✅ **`--agent1`/`--agent2` CLI options** — `random` or `model:path.onnx`
- ✅ **Memmap dataset**: `numpy.memmap` for disk-backed storage during training. Two-pass loading (count then fill). Handles 10K+ game datasets without OOM. OS pages data in/out as needed.
- ✅ **Ban list validation**: `cards/ban-list.csv` loaded by DeckValidator. Decks with banned cards rejected. Format: `SET,ID,'DISPLAY NAME'` (single-quoted names for comma handling).
- ✅ **Design docs**: `docs/ml-training-design.md` (training pipeline, Elo, evolutionary framework), `docs/deck-agent-design.md` (deck construction agent)
- ✅ **`.gitignore`**: build, IDE, generated, game output, models, Python

#### First Training Results (MF v001 — v1 model, 93 features)
- ✅ **10K MF mirror games generated** (194 games/sec Release, ~52 seconds)
- ✅ **5.8M decision points**, 93 input features, 132K model parameters
- ✅ **65.7% policy accuracy** (vs 7% random baseline) after 20 epochs
- ✅ **Value loss 0.496** (win prediction from board state)
- ✅ **55% win rate vs random** (vs 50% baseline) — first ML agent beats random

### Phase 7 — Feature Expansion (4407 dims; RESERVED_STATE_DIM=4608)

Feature vector layout is documented in `src/ml/feature_extractor.h` and `docs/additional-gamestate-dims.md`. Padded to `RESERVED_STATE_DIM` for forward-compatible `--resume`. Each tier requires lockstep changes to the JSONL serializer, the shared C++ feature extractor (`src/ml/feature_extractor.cpp`), and the Python trainer. `scripts/parity_check.py` must pass before training on new feature data.

#### Tier 1 — High Impact (+116 features → 470 total) ✅ COMPLETE
- [x] **Combat designation per BF unit** (+24): attacker/defender status on top-3 units per side
- [x] **Attached gear card_def_id per BF unit** (+24): equipment identity, not just has_attachment bool
- [x] **Might delta per BF unit** (+24): current_might - base_might distinguishes permanent vs temporary strength
- [x] **Chain item targets** (+8): what each spell on the chain is targeting (critical for counter-play)
- [x] **Per-BF scored-this-turn flags** (+8): which BFs were scored, not just count (Conquer win condition)
- [x] **Cost modifier type** (+4): next_spell_only vs next_unit_only vs all (changes optimal sequencing)
- [x] **Temp might bonus per BF unit** (+24): separates this-turn-only might from permanent

#### Zone Multi-hot Encoding (+3935 features → 4405 total) ✅ COMPLETE
Perfect-information game-state encoding per CR. Each zone is a 787-dim count vector indexed by `card_def_id - 1` (cards are 1..787).
- [x] **Self main_deck multi-hot** (+787): full deck composition. Lets the model reason about what it can still draw.
- [x] **Self trash multi-hot** (+787): what self has played/discarded. Drives Deathknell/recycle reasoning.
- [x] **Self banishment multi-hot** (+787): self's permanently-removed cards.
- [x] **Opponent trash multi-hot** (+787): public — drives "opponent already used their removal" awareness.
- [x] **Opponent banishment multi-hot** (+787): public.
- Opponent's deck and hand remain hidden per CR.
- Disk cost: 8.3 KB → 23.4 KB per decision record (~3× growth). A 10K-game dataset goes from ~19 GB to ~57 GB.

#### Turn-Flag Block (+2 features → 4407 total) ✅ COMPLETE
- [x] **`self.cant_play_cards_this_turn`** (pos 4405): Brynhir Thundersong lockout.
- [x] **`opp.cant_play_cards_this_turn`** (pos 4406): same, observed for opponent.

#### Tier 2 — Medium Impact (+163 features → 633 total)
- [ ] **Top-3 trash card_def_ids per player** (+6)
- [ ] **Ready vs exhausted base unit split** (+2)
- [ ] **Buff count per BF unit** (+24)
- [ ] **Assault/Shield/Deflect value magnitudes per BF unit** (+72)
- [ ] **Cost modifier magnitude sum** (+2)
- [ ] **Contested-by-self per BF** (+4)
- [ ] **Focus holder** (+1)
- [ ] **Additional turn queue depth per player** (+2)
- [ ] **Temp buff count per BF unit** (+24)

#### Tier 3 — Lower Priority (+227 features → 808 total)
- [ ] **Per-unit keyword bits for top-3 BF units** (+144)
- [ ] **Damage assignment featurization in action features** (+8 to ACTION_FEATURE_DIM)
- [ ] **Chain item classification** (spell/permanent/ability) (+12)
- [ ] **Chain item status** (pending/finalized) (+4)
- [ ] **Delayed ability identity** (+6)
- [ ] **Facedown card age** (+4)
- [ ] **Hand card power costs** (+20)
- [ ] **Total battlefield count** (+1)
- [ ] **Combat/showdown staged flags** (+8)

All tiers fit in GPU memory (~31 MB at batch_size=2048, <1% VRAM). Disk-backed memmap ~28 GB per dataset. When active features pass ~500, bump `hidden_dim` to 1024.

### Phase 8 — Self-Play RL

Replace supervised learning on random games with policy gradient on model-vs-model outcomes. This is where strategic emergence happens — the model learns tempo, baiting, board control through selection pressure, not reward shaping.

#### Completed (✅)
- ✅ **`scripts/self_play_loop.sh`** — overnight self-play loop with gated promotion. Per iter: generates self-play data with current best models (T=1.0 sampling) → trains both archetypes in parallel on separate GPUs (`train-rl` REINFORCE) → benchmarks each candidate vs its current best across 2 seat orders → promotes if decisive win rate ≥ `PROMOTE_PCT` (default 55%) → cleans up `.bin` data. MF and Rengar gens advance independently — either can plateau without blocking the other.
- ✅ **REINFORCE training (`train-rl`)** — supervised checkpoint resume, win/loss reward, entropy bonus, value-head baseline, ONNX export. Auto-pads state features to `RESERVED_STATE_DIM`.
- ✅ **`ModelAgent` ONNX session cache** — process-wide `ModelSession` keyed by model path. ONNX Runtime sessions are thread-safe for inference, so all worker threads share one session per loaded model. Avoids reloading the same model per game × per agent slot. ~4× throughput on batch self-play runs.
- ✅ **Adaptive entropy + eval-games schedule** — reject-streak counter per archetype. After 2/4/6 consecutive rejects, multiply entropy coef AND eval game count by ×2/×4/×8 (capped). Both knobs reset on promotion. Encourages exploration when stuck and tightens the statistical gate near the 55% threshold.
- ✅ **Trap-based child cleanup** — `cleanup_children` trap on EXIT/INT/TERM does `pkill -P $$` so Ctrl-C'ing the loop kills its python training jobs instead of leaving them orphaned to compete with the next run.
- ✅ **Log rotation + gzip** — 50 MB per-part .log files, gzip rotation, prune to last 100 .gz. Read with `zcat logs/self_play_*_part*.log* | less -R`.
- ✅ **Color-coded promote/reject/eval lines** — bold green/red/yellow ANSI in master log; grep-friendly. `✓ [promote MF] v002 → v003` / `✗ [reject MF]`.
- ✅ **Auto-resume from highest `v00N` on disk** — `latest_gen()` at startup detects the most recent checkpoint for each archetype. Crash-and-restart picks up where we left off instead of clobbering work.
- ✅ **Coin toss randomization** — `determineTurnOrder()` randomizes starting player (CR 116) so mirror matches are seat-symmetric. Random-vs-random mirrors verified balanced at 50/50.
- ✅ **`chosen_idx` partial-match bug fix** — `BinaryDataSerializer::recordDecision` and `DataSerializer::recordDecision` were finding the chosen intent in the legal list using a 4-field comparison (`type, card, targets, ability_source`) — Intent has 16 distinguishing fields, so "twin" intents (same primary fields, different destinations/objects/etc.) all collapsed to index 0 of their group. Biased training labels and REINFORCE gradients in ~60% of decisions. Added `Intent::operator==` (full structural equality) and switched both serializers to use it. chosen_idx norm went from 0.21 (biased) → 0.50 (correctly uniform) on random-agent data.
- ✅ **Live engine output during eval** — `bench()` uses `tee` to a tempfile: engine stream goes to stderr (live log + terminal) while the file is parsed for `P1 wins`, `P2 wins`, `Draws`. No more 70-second blackout during eval.

#### Open
- [ ] **Score-differential reward** for early training (transition to pure win/loss once model is strong)
- [ ] **Checkpoint every 5 iterations**, version naming: `miss_fortune_v005_self_play.pt`

#### Lessons learned (for next agent)
- **Supervised pre-training on random-agent data has a low ceiling.** Per-decision policy accuracy maxes out around 42–50% (most of which is "trivial" 1-legal decisions) because the supervised target is "imitate uniform random" — not a useful objective. Supervised v001 reliably loses to random at T=0 (~13–30% win rate) because argmax over a near-uniform distribution amplifies whatever weak bias the model picked up.
- **REINFORCE warm-started from a passive supervised baseline works, but plateaus fast.** Early iters (v001 → v002) promote at 92–96% win rate (banking the "play cards instead of passing" gain). Subsequent iters compress to ~45–55% win rates and start hitting reject streaks.
- **At T=0 inference, high-entropy training (×8 = 0.24) produces flat action distributions** that revert to passive behavior because there's no decisive top-scoring action. Games stretch from ~25 to ~60 turns (~400 → ~3100 decisions). Eval throughput drops from ~14 gps to ~3 gps. Watch for this when reject streaks are deep and consider capping the entropy multiplier lower than ×8.
- **`pkill -P $$` in EXIT/INT/TERM traps is non-negotiable** for any bash loop that backgrounds long-running children. Without it, Ctrl-C'ing the loop leaves the parallel pythons reparented to init, competing with the next run for GPU + disk. We discovered this when a "running slow" diagnostic turned up 4 training processes (2 orphans + 2 new) fighting for both GPUs.
- **`$()` captures stdout — any `log()`-style helper a callee uses must write to stderr**, otherwise the captured value becomes the log lines plus the trailing return value, and integer comparison on it fails. This silently rejected every candidate (including 93% / 97% winners) for the entire first run.

### Phase 9 — Cross-Archetype League

Per-legend agents that are robust across matchups, not just mirrors.

- [ ] **Per-legend agents**: Train 40 separate models, one per legend
- [ ] **Round-robin tournament**: Each legend's agent plays all others (780 matchups × 100 games)
- [ ] **Elo rating system**: Start at 1000, K=32 new / K=16 established, per-matchup tracking
- [ ] **Focused retraining**: Retrain on worst matchups (lowest Elo delta)
- [ ] **League iterations**: ~10 rounds of tournament → retrain → re-tournament

### Phase 10 — Memory-Augmented Agents

Agents that track opponent information and adapt mid-game. Hybrid approach: engine tracks observations deterministically, model interprets strategically. See `docs/ml-training-design.md` Phase 4 for full design.

**Key gap currently:** Reveal effects (Aurora top-of-deck reveals, Mindsplitter/Sabotage hand reveals, Predict, Stacked Deck, Vision) expose information to a player that the model never captures — it only sees zone end-states, not what was briefly observed. See `docs/additional-gamestate-dims.md` "Deferred — Observation Tracking" for the proposed implementation (PlayerState observation vectors + reveal-event hooks + ~1574 new feature dims). Important to circle back on before deep self-play training plateaus.

- [ ] **ObservationTracker**: Subscribes to EventBus, maintains 787-dim revealed card vector per player
- [ ] **Zone-change tracking**: Cards cleared from "seen" when they leave known zones (played, recycled into deck)
- [ ] **Observation features**: Add 787-dim revealed vector + recent play history to state features
- [ ] **Attention model**: Replace MLP state encoder with self-attention over cards (Phase 2 architecture)
- [ ] **Optional learned memory head**: 256-dim memory vector carried between decisions, BPTT training

### Future Work

- [ ] **Model architecture upgrades** — current `RiftboundAgent` is a 2-layer MLP with LayerNorm on inputs, no dropout, no embeddings, no attention. ~2.6M params at `hidden_dim=512`. The architecture is the single biggest learning bottleneck once REINFORCE plateaus. Discussed in order of leverage:
  1. **Card embedding table** (highest leverage). Currently every `card_def_id` slot (~30 slots: hand IDs, BF unit IDs, base unit/gear IDs, chain item IDs) is fed as a raw float in the state vector. To the network, card 487 sits geometrically next to card 488 on a number line — but those IDs are arbitrary database identifiers with zero semantic relation. Replace with `nn.Embedding(788, 32)` lookup so each card gets a learned 32-dim vector. Similar cards (equipment, fast units, control spells, archetype synergies) end up clustering in the embedding space — the model discovers these groupings as a byproduct of predicting outcomes. Lets the model express card-card interactions (e.g., "Last Rites equips well on Renekton-shaped units") that raw IDs can't represent. +~25K params (~1%). Requires a new model — existing checkpoints are not loadable into the new architecture. Practical cost: regen 1K-game baseline + re-run supervised baseline (~30 min total) + fresh self-play loop. Existing v00N checkpoints are passive and not worth preserving — minimal real loss.
  2. **Dropout / weight decay** (cheap insurance). No regularization beyond LayerNorm currently. Add `nn.Dropout(0.1–0.2)` in the state encoder and action scorer to reduce overfitting across REINFORCE iters. Free in params, slightly slower training.
  3. **Attention over actions** (structural change). Currently each legal action is scored independently — the model can't directly reason "score X relative to also-available Y." Replace the per-action scorer with self-attention over the action set so each action's score is computed in context of the others. Big lift; biggest expected gain when the policy needs to make conditional choices like "play A only because B is also available." +50–200K params.
  4. **More depth + residual connections**. State encoder is shallow (2 hidden layers). With more depth the model can compose strategic abstractions (sacrificing now for a bigger play later). Add residuals when stacking 3+ layers to keep gradients well-behaved. +200K–1M params per added layer.
  - **Recommended sequencing**: do (1) embeddings + (2) dropout together as the next architecture upgrade — they're easy and the embeddings are the most leveraged change for a TCG. Save (3) and (4) for after embeddings are validated and you've identified specific reasoning limits.
- [ ] **Match runner**: Best-of-3 with sideboarding and battlefield rotation (CR 481)
- [ ] **Deck builder**: `scripts/deckbuilder.py` — evolutionary optimization, ban list aware
- [ ] **OpenSpiel integration** (strategic destination for RL — supersedes a custom Phase 8 self-play loop): pybind11 wrapper exposing `riftbound::Game` / `riftbound::State` to OpenSpiel's algorithms (AlphaZero, NFSP, MCTS, CFR). The supervised baseline (e.g., rengar v001) is sufficient as the AlphaZero warm-start policy — don't build a custom REINFORCE/PPO stage in between. Four engineering pain points, in rough order of difficulty:
  1. **Action-space flattening**: every legal intent (card × target × destination × ability_source × dest_bf × ...) must encode to a single dense `int64_t` Action ID, decodable back to a unique intent. The space is large (787 cards × targets × destinations ≫ 10⁵) so a structured bit-packed encoding is required. This decision shapes everything downstream — get it right first.
  2. **Chance nodes**: every random event (deck shuffles, draws, mulligan replacements, coin toss) must be reachable via `State::ChanceOutcomes()`. Can't `std::shuffle` inline. Standard pattern (used in Poker): model each draw as a chance node with N outcomes over remaining cards, realize the deck order incrementally as draws happen — not all upfront.
  3. **Fast `State::Clone()`**: MCTS clones constantly, so `GameState` must be cheaply copyable. `EventBus` (Boost.Signals2 connections), subsystem `unique_ptr`s, and aura-recalc transient state need to be excluded from the clone path or recreated lazily. May require splitting `GameState` into a hot copyable core and a transient sidecar.
  4. **Information sets / per-player observation**: `State::InformationStateTensor(player)` must hide what's hidden (opponent's hand, hidden cards, deck order, facedown cards' contents). `extract_state_features` in `train_agent.py` already takes a `perspective` arg — the bones are there but need to be made deterministic per player and aware of partial observations (e.g., "I saw card X get hidden but don't know its identity now").

  **Reference games to mirror**: OpenSpiel's `games/universal_poker/` and `games/hearts/` are the closest analogues (large action space, imperfect info, chance nodes, structured turn flow). Read these implementations before designing `riftbound::State`.

  **Recommended sequencing**: (A) action-encoding design on paper, (B) minimal `Game`/`State` skeleton with full-info observation, (C) parity validation vs current `BatchRunner` on random-vs-random, (D) chance nodes + information sets, (E) plug in AlphaZero. Skip the custom Phase 8 self-play loop in this CLAUDE.md — those cycles go into the port.
- [ ] **Additional manual cards**: 578 complex cards have auto-generated stubs (compile but no-op on resolve). 76 manually implemented with full behavior. Remaining cards need manual Card subclass overrides — significant effort. Priority: (1) ~42 do-nothing cards in test decks (down from 50 — 8 fixed this session), (2) cards for new decks added to the league, (3) high-frequency tournament cards for Phase 9
- [ ] **Observation history feature**: 787-dim one-hot of cards opponent has played/revealed
- [ ] **Hand card features**: Card feature vectors from registry.json instead of raw IDs

## Key Design Docs
- `docs/engine-design.md` — comprehensive engine architecture (read this first)
- `docs/ml-training-design.md` — ML agent training pipeline: phases, Elo, evolutionary framework, memory system
- `docs/additional-gamestate-dims.md` — 25 unfeaturized game state fields with priority tiers and position assignments
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
