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
- [Architecture notes for next agent](#architecture-notes-for-next-agent)
- [Adding state to feature_extractor](#adding-state-to-feature_extractor)
- [Lessons learned / pitfalls](#lessons-learned--pitfalls-for-next-agent)
- [Remaining Work](#remaining-work)
  - [Do-nothing cards in test decks](#do-nothing-cards-in-test-decks)
  - [Mechanical features (TODO)](#mechanical-features-todo)
- [Known engine gaps / cleanup-tier follow-ups](#known-engine-gaps--cleanup-tier-follow-ups)
- [OpenSpiel integration knowledge](#openspiel-integration-knowledge)
  - [Build / run invocations](#build--run-invocations)
  - [Known OpenSpiel build mechanics](#known-openspiel-build-mechanics)
  - [Resume pattern for in-flight chain resolution](#resume-pattern-for-in-flight-chain-resolution)
- [Future Work](#future-work)
- [Key Design Docs](#key-design-docs)
- [Data Pipeline](#data-pipeline)
- [Important Game Rules for Engine](#important-game-rules-for-engine)

> **Note on completed work:** Historical "Phase X" content lives in `docs/engineering-history.md` (full archive) and the git log (`git log --oneline --grep='Phase' master`). This document focuses on current state, live knowledge, and remaining TODOs — no phase labels.

## Project Overview
C++20 game engine that simulates 1v1 Riftbound TCG matches. Takes two deck lists as input, runs games with agent-driven decision-making, and exposes the game tree to OpenSpiel's algorithm suite (MCTS today; AlphaZero / NFSP / etc. when C++ neural-net training lands).

A previous Python-based ML pipeline (REINFORCE, ONNX inference) was retired in commit `752e54f` to consolidate everything in C++. Training is now done via OpenSpiel's MCTS bootstrap + V2 transformer model with LibTorch (see `src/ml/`, `src/training/`).

## Build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && RIFTBOUND_ROOT=.. ./riftbound_tests
```

**Optional: OpenSpiel wrapper**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DRIFTBOUND_BUILD_OPENSPIEL=ON
cmake --build build --target riftbound_openspiel
./build/src/openspiel/riftbound_openspiel --agent1 random --agent2 random --games 10
```
First configure with the flag clones OpenSpiel v1.6.14 + abseil-cpp 20250814.1 + nlohmann/json v3.11.3 + pybind11_json + DDS into `build/_deps/`. Adds ~10s configure + a one-time ~30s build of `open_spiel_core` and abseil. Off by default to keep the normal dev loop fast.

**Optional: LibTorch (training)**
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DRIFTBOUND_BUILD_OPENSPIEL=ON \
    -DRIFTBOUND_BUILD_LIBTORCH=ON \
    -DRIFTBOUND_LIBTORCH_VARIANT=cu121   # or cpu
cmake --build build --target riftbound_runner
```

**Dependencies:** cmake, g++ (C++20), libboost-all-dev, nlohmann-json3-dev, ninja-build

The `scripts/` directory has Python (fetch_cards.py, apply_errata.py, card_registry.py, deck_import.py, generate_cards.py) — card-data tooling, not ML. They build cards/registry.json from the gallery + apply errata + codegen the C++ card scaffolding. None are needed at engine runtime.

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
│   ├── core/              types, events, intent, game_object, game_state, card_db
│   ├── engine/            game_engine, chain_manager, effect_executor, trigger_manager,
│   │                      batch_runner, game_runner, step_driver
│   ├── cards/             card.{h,cpp}, card_registry.{h,cpp}, manual/, generated/
│   ├── rules/             deck_validator
│   ├── effects/           effect_types.h
│   ├── agents/            agent_interface.h, random_agent.h
│   ├── ml/                feature_extractor, entity_tokens, v2_model, libtorch_evaluator
│   ├── training/          replay_buffer, self_play, trainer, config_driver,
│   │                      ring_logger, watchdog
│   ├── openspiel/         riftbound_game, riftbound_state, action_vocab, openspiel_match
│   ├── io/                state_renderer, replay_writer
│   └── runner/            riftbound_runner.cpp (main entry)
├── tests/                 Google Test suite (533 tests as of 2026-05-19)
├── cards/
│   ├── registry.json      787 cards with IDs + feature vectors
│   ├── card_index.json    All card data (errata applied)
│   ├── ban-list.csv       Banned cards
│   ├── json/              Individual card JSONs
│   └── raw/               Raw gallery data
├── decks/                 Sample decks (per-archetype .json + .txt formats)
├── rules/                 core-rules.md, tournament-rules.md, core-rules.pdf
├── errata/                Official errata documents
├── configs/               Training config JSONs (gpu_bootstrap_*.json etc.)
├── scripts/               Card-data tooling + vast.ai deployment scripts
└── docs/                  engine-design.md, deck-agent-design.md, playmat-layout.md,
                           potential-model-architecture.txt, additional-gamestate-dims.md
```

## Current Status

Engine, training pipeline, OpenSpiel port, and V2 model are all wired and verified end-to-end. 533/533 unit tests passing. Bootstrap-style AlphaZero training runs successfully on vast.ai GPU instances.

### What works now
```bash
# Legacy CLI — fast build (no OpenSpiel deps), random agent only.
./build/riftbound decks/leblanc_test.json decks/vex_test_deck.json -r cards/registry.json

# Multiple games + threading
./build/riftbound deck1.json deck2.json -r cards/registry.json --games 100 --threads 8

# Render per-game HTML replay (board + decisions + trace) to ./replay_gameN.html
./build/riftbound deck1.json deck2.json -r cards/registry.json --render

# Step through interactively (press Enter each decision)
./build/riftbound deck1.json deck2.json -r cards/registry.json --step --show-hand

# OpenSpiel binary — random vs MCTS, with HTML replay into ./replays/
./build-release/src/openspiel/riftbound_openspiel \
    --agent1 random --agent2 mcts:sims=5 \
    --deck1 decks/miss_fortune_test.json --deck2 decks/miss_fortune_test.json \
    --games 10 --seed 42 --render-html

# Model agent (after training)
./build-torch/src/openspiel/riftbound_openspiel \
    --agent1 model:sims=10,path=checkpoints/gpu_rengar/iter_20.pt \
    --agent2 random --games 20 --threads 4

# Training (single legend)
./build/riftbound_runner --config configs/gpu_bootstrap_rengar.json
```

## Architecture notes for next agent

**Core principle: Card mechanics are encapsulated in Card objects, not the engine.**
The GameEngine handles game flow (phases, turns, chain FEPR loop, cleanup, scoring) and provides atomic operations via EffectExecutor. All card-specific behavior (what a spell does, what a trigger does, what an activated ability does, how counters work) lives in Card subclass `onResolve()`/`onTrigger()`/`onActivate()` overrides. Never add card-specific logic to game_engine.cpp — the engine dispatches to Card objects and the cards tell the engine what to do via the executor helpers. This keeps the engine generic and each card's behavior self-contained, testable, and reviewable.

- **Card objects** (`src/cards/card.h`): Every card has a Card subclass registered in CardRegistry. Card objects override `onResolve()`, `onTrigger()`, `onActivate()` etc. All card-specific behavior — effects, targeting, countering, token creation, damage computation — belongs here, not in the engine.
- **CardRegistry** (`src/cards/card_registry.h/cpp`): Maps CardDefId -> Card*. Loaded ONCE at application startup, shared as `const CardRegistry&` across all game threads. Card objects are stateless — concurrent reads safe. `card_registry_.get(def_id)->onResolve(ctx, targets)` is the dispatch path. Manual overrides (`src/cards/manual/*.cpp`) register AFTER generated cards and overwrite generated stubs.
- **Code generation** (`scripts/generate_cards.py`): Reads registry.json, parses ability_text, generates C++ card classes. 209 cards have auto-generated effects. 578 complex cards have partial implementations. Regenerate with `python3 scripts/generate_cards.py`. Manual overrides go in `src/cards/manual/`.
- **EffectExecutor** is a utility library of atomic game operations (dealDamage, drawCards, killObject, bounceToHand, createToken, copyUnit, predict, moveToBattlefield, etc.). Card objects call these via `ctx.executor.*`. The executor does NOT decide WHAT to do — that's the card's job. The executor only knows HOW to modify game state.
- **TriggerManager** uses CardRegistry (`card->triggerType()`) to match events to triggers. Subscribes to EventBus. Also checks `DelayedAbility` list for one-shot delayed triggers.
- **Chain is a LIFO stack** (`std::vector`, resolve from back). When a spell/ability resolves, FEPR pops it, then calls `resolveSpell()` which dispatches to `card->onResolve()`. Counter spells use **peek-and-pop**: the counter's `onResolve` peeks at the new chain top — if it's a spell, pops it and disposes (trash or hand). Counter-of-counter works naturally via LIFO — no flags, no scanning, no chain mutation outside the card's own onResolve.
- **Aura system** uses tagged effects (`AuraEffect` on GameObject), recalculated from scratch during `cleanup()`. No formal layer system. `hasKeyword()` checks both base and aura-granted keywords. ~18 aura sources + conditional self-effects evaluated each cleanup pass.
- **Combat damage** — both attacker and defender queried through agent interface. Tank/Backline ordering enforced. Agent picks from greedy-lethal, spread-even, focus-all, and lethal-first distributions.
- **Cost payment** — `payCardCost()` applies CostModifier reductions, then exhausts runes for energy (agent-chosen), then recycles exhausted runes for power (agent-chosen). Energy-first ordering allows recycling just-exhausted runes for power (efficient). Each rune selection is an agent decision point.
- **Multi-ability per Card** — `Card::activatedAbilities() → vector<ActivatedAbility>` returns N descriptors. Default impl wraps the legacy single-ability virtuals for back-compat. Multi-ability cards override `activatedAbilities()` and override the indexed `onActivate(ctx, ability_index, targets)`. `Intent::ability_index` and `ChainItem::ability_index` carry the choice through the engine + chain. `action_vocab` encodes ActivateAbility slots as `verbBase + def_id_slot * kMaxAbilitiesPerCard + ability_index` (kMaxAbilitiesPerCard=4).
- **Resumable agent choices** — `Card::confirmOptional` (yes/no triggers), `Card::pickMode` (modal spells), `Card::pickXAmount` (variable-X cost), `Card::pickTarget` (target selection), `Card::pickTargetPair` (dual targets). Each uses distinct resume_point ranges + resume_data slot:
  - confirmOptional / pickXAmount: 0/1/2 + resume_data[0]
  - pickMode: 3/4/5 + resume_data[1]
  - pickTarget: 6/7/8 + resume_data[2]
  These compose cleanly within a single onResolve.
- **Target-collision fix** — `Card::needsPlayTimeTarget()` (single target) and `Card::needsPlayTimeTargetPair()` (dual targets) opt the action generator into emitting ONE Play intent per card (no per-target variants) so the policy head gets distinct vocab slots per target choice. The card's onResolve uses `pickTarget` / `pickTargetPair` to resolve targets at chain time. Equivalent for activated abilities: per-ability `ActivatedAbility::needs_activation_time_target` flag.
- **Threading model** — `BatchRunner` wraps `boost::asio::thread_pool`. `GameRunner` is per-game, fully thread-safe (all state stack-local). Shared singletons: `CardDB` (const), `CardRegistry` (const). Per-game: `EventBus`, `GameState`, `GameEngine`, agents, I/O. `AggregateResults` uses atomics for counters, mutex for console.
- **The `on_decision` callback** fires at every decision point including mulligans, combat damage, and chain priority. Trivial single-option decisions auto-skipped in render.
- **Logging levels**: `--debug` shows trigger firings and ability resolution. `--trace` adds every phase transition, decision, intent, effect, draw, discard, rune exhaust/recycle, damage assignment, kill, score, counter, equip, token, burn out, and mulligan with card names.
- **HTML replay**: `--render` (legacy CLI) and `--render-html` (OpenSpiel CLI) generate `replay_gameN.html` with UUID, seed, board state + decision + trace log panels, arrow-key navigation, ★ SCORE banner on scoring snapshots, clickable card-name lookups.
- **State features** — `src/ml/feature_extractor.cpp` produces a perfect-information state vector (currently `kStateFeatureDim = 4623`, padded to `RESERVED_STATE_DIM = 4864` for forward-compatible additions). Used by `RiftboundState::ObservationTensor` to expose game state to OpenSpiel algorithms. Layout details in `feature_extractor.h` + `docs/additional-gamestate-dims.md`. ObservationTensor masks hidden information per perspective (opponent's deck and hand stay hidden; opp trash/banishment remain public per CR).
- **V2 entity-token model** (`src/ml/v2_model.{h,cpp}`) — transformer (6L × 8H × d=512, FFN=2048) over per-token embeddings (card 256d, zone 64d, domain 64d, stance 64d, spatial 32d, stats 64d, perspective 8d → d_model=512), followed by spatial fusion (2 layers, edge-bias attention over 8 spatial nodes), pointer heads (action_type, source, target, dest_node — computed but not yet supervised), and flat policy head (kVocabSize) + value head. Trained on `(entity_tokens, MCTS visit distribution, game outcome)` tuples via `src/training/`. See `docs/potential-model-architecture.txt` for design rationale.
- **Training dynamics guardrails** — five coordinated fixes prevent the policy/value heads from collapsing into a degenerate "always EndTurn" attractor that earlier bootstrap runs fell into:
  - **`HeuristicValueEvaluator`** (`src/training/heuristic_evaluator.{h,cpp}`) replaces `RandomRolloutEvaluator` during bootstrap. Uses `score_diff / 8.0` clamped to [-1, +1] so MCTS gets a real value signal on mirror matches where random rollouts return noise. Prior is uniform-over-legal to prevent prior-bias leakage.
  - **Freeze value head during bootstrap** (`TrainerConfig::freeze_value_head`). Value loss is computed for reporting but the gradient is detached. Keeps the head at random init through bootstrap so it doesn't converge to the "predict zero everywhere" degenerate optimum on balanced mirror data.
  - **Entropy regularization** (`TrainerConfig::entropy_coef`). Adds `-entropy_coef * H(model_policy)` to the policy loss. config_driver sets this to 0.02 during bootstrap, 0.0 afterward. Prevents the policy from peaking onto a single dominant action.
  - **`ActionVerb::Reserved` at slot 0** — never emitted by `encodeAction`. Removes the structural privilege of any specific action (EndTurn moved to slot 1).
  - **MCTS exploration bias** — bootstrap configs use `uct_c=4.0` + `dirichlet_fraction=0.5` (vs. AlphaZero defaults 1.4 / 0.25) so Q-values matter more relative to the prior at low visit counts, breaking bias amplification.
- **AssignCombatDamage 128 buckets** — bumped from 16 to cut hash-collision rate on combat damage distributions. 128 covers most realistic patterns with <10% collision.
- **Equip-target hash** — equip intents go through the ActivateAbility encoder path; when the source is a gear and intent has a target, the encoder hashes target's `card_def_id % kMaxAbilitiesPerCard` into `ability_index` so (gear, target-unit) pairs get distinct vocab slots. Partial fix (4 buckets) sufficient for most decks; full fix needs per-gear migration to `needsEquipTimeTarget`.
- **Coin toss** — `TurnState::starting_player` records who won the coin flip (CR 116).

## Adding state to feature_extractor

When you add a new field to GameState / PlayerState that should be visible to a future ML agent, also bump `kStateFeatureDim` in `src/ml/feature_extractor.h` and append the new dim(s) to `extractStateFeatures` in the .cpp. This is what `RiftboundState::ObservationTensor` reports to OpenSpiel. There is no longer a Python parity surface to keep in sync — the layout is C++-only. `docs/additional-gamestate-dims.md` lists the legacy backlog (some already landed) for reference.

## Lessons learned / pitfalls for next agent
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
- **MCTS rollouts have no internal length cap.** `RandomRolloutEvaluator` runs `while(!IsTerminal) ApplyAction(random)`. If the engine produces a state that never reaches a natural terminal under random play (stalemate, forced k=1 sequences), the rollout spins forever inside MCTSBot::MCTSearch. Mitigation: `RiftboundState::IsTerminal()` has a hard cap at `MoveNumber() >= 600` (matches `max_decisions_per_game`). Without this cap, training hangs.
- **Self-play workers should match physical cores, not hyperthreads.** Configuring `num_self_play_workers = physical_cores` (e.g. 16 on a 64-physical / 128-hyperthread box) gives each worker a dedicated physical core and avoids the contention that makes individual MCTS calls slow enough to falsely trigger hang detection.
- **The 600-decision cap on the outer game** doesn't catch single-decision hangs (e.g. an MCTSBot::MCTSearch infinite loop). The cap is checked between decisions; if a single decision hangs forever inside MCTS, the outer counter never advances. The IsTerminal-level cap (above) is what actually bounds work.
- **Token cease-to-exist (CR 183.1).** Tokens routed to any non-board zone (trash, banishment, hand) cease to exist immediately. Both `EffectExecutor::killObject` and `GameEngine::killUnit` route tokens to Banishment WITHOUT adding to player.banishment vector. Tokens copied via Mirror Image inherit `card_def_id` so death triggers still fire.
- **Trigger context capture for designations.** `combat_designation` is cleared between trigger-fire and chain resolution. Cards that need attacker/defender identity must capture it into `card_counters["__defend_attacker_id"]` at fire time, not at onTrigger time. See `MOverzealousFan`.
- **Action vocab slot collisions.** Always check whether your encoding distinguishes the cases you need the policy head to distinguish. Examples: `MakeChoice` int-coded slots for yes/no/mode/X answers; `needsPlayTimeTarget` + `pickTarget` for play-time target choice; `ActivatedAbility::needs_activation_time_target` + `pickTarget` for ability-time target choice; multi-ability `ability_index` slot encoding for per-ability distinction.

## Remaining Work

### Do-nothing cards in test decks

Active TODOs remaining (most originally-flagged cards have shipped — see git log for landed migrations):

| ID | Card | Decks | Complexity | What's needed |
|----|------|-------|-----------|--------------|
| 352 | Rek'Sai, Breacher | draven_test | PARTIAL | Engine surface (`Intent::play_source` + payCardCost auto-Accelerate) shipped. Card's Card subclass not yet wired — the cost path checks for card_def_id 352 in objects, so the effect is live, but a dedicated MRekSaiBreacher Card class would be cleaner (currently UnitCard default). |
| 457 | Hard Bargain | khazix, miss_fortune, sett | MODERATE | Counter unless they pay [2], Repeat. Counter mechanics exist; the "unless they pay" branch needs pickXAmount-style choice for the opponent. |

**Partial / verify:**

| ID | Card | Status |
|----|------|--------|
| 603 | Allay, Eager Admirer | Likely already handled by `recalculateAuras` text-matcher. Verify with a unit test before adding code. |
| 614 | Nami, Headstrong | Implemented (stuns on play); lacks additional-cost gate but functional. |
| 695 | Blast Cone | On-play move implemented; "exhaust to stun moved enemy" follow-up activated ability not yet wired (needs multi-trigger Card support since the gear has both an on-play trigger AND an activated ability). |
| 737 | Tactical Retreat | Approximation present; full replacement-effect-via-Card-class semantics not yet wired. |
| 612 | Iascylla | Immediate-on-hold approximation shipped. Proper version needs multi-trigger per Card (Iascylla has both WhenIHold + AtStartOfMain triggers). AtStartOfMain trigger type itself IS wired in TriggerManager. |

**Per-deck training-quality summary:**

| Deck | Empty cards | Targeted-spell collisions | Targeted-activate collisions |
|------|---|---|---|
| vex_pre_con | 0 | 0 (Combat Experience + Skyward Strike migrated) | 0 (Shadow migrated) |
| vex_test_deck | 0 | 0 (Charm + Discipline + Rebuke migrated) | 0 |
| khazix_test | 0 (Voidreaver multi-ability shipped) | 0 (Rebuke + Skyward Strike + Combat Experience all migrated) | 0 (Voidreaver + Blood Rose migrated) |
| sett_test | 0 (shared pool with khazix) | 0 | 0 |
| miss_fortune_test | 0 | 0 (Gust + Bullet Time migrated) | 0 (Bounty Hunter + Heart of Dark Ice migrated) |
| rengar_test | 0 | 0 (Thrill of the Hunt migrated) | 0 |
| lilina_test | 0 | (not audited) | (not audited) |
| leblanc_test | 0 (Karthus shipped via `applyPassiveAura` + 7 unit tests) | (not audited) | (not audited) |
| draven_test | 1 (Rek'Sai partial — engine surface wired, dedicated Card class is a polish item) | (not audited) | (not audited) |
| ornn_test | 0 | (not audited) | (not audited) |

**Per-card noise is cleared on the 6 deck pools we've trained or plan to train next** (rengar, miss_fortune, vex_pre_con, vex_test_deck, khazix_test, sett_test). Remaining items are polish (Rek'Sai dedicated Card subclass) or "approximations that work but aren't pixel-perfect" (Iascylla immediate-on-hold instead of delayed-to-next-main). The non-audited decks (lilina, leblanc, draven, ornn) need a target-collision audit before being added to training. See `docs/engineering-history.md` for the chronological record of each migration.

### Mechanical features (TODO)

Active TODO items remaining (most engine surface from the prior backlog shipped — see git log + `docs/engineering-history.md` for the chronological record):
- [ ] **Equip-action target collision — gear migrations.** Framework shipped: `Card::needsEquipTimeTarget()` virtual + `game_engine.cpp` action gen branch + execution-time `kInvalidId` target handling. Remaining work: migrate each of the ~33 equip gear cards in `src/cards/manual/equip_cards.cpp` to override `needsEquipTimeTarget=true` and rewrite `onEquip` to call `pickTarget` when the unit param is `kInvalidId`. Mechanical loop similar to the Phase 6q Play target migration.
- [ ] **Multi-trigger per Card** — Card supports only one `triggerType()`. Cards with multiple distinct triggers (Blast Cone's on-play move + activated stun; Iascylla's WhenIHold + AtStartOfMain delay) currently only model one. Same shape of refactor as the multi-ability work but for triggers instead.
- [ ] **Rek'Sai dedicated Card subclass.** The auto-Accelerate engine path is live (`payCardCost` checks for `card_def_id == 352` among controller's units). For consistency with the Karthus / applyPassiveAura pattern, a dedicated `MRekSaiBreacher` Card subclass with a passive aura hook is cleaner than the hardcoded def_id check.
- [ ] **[457] Hard Bargain** — counter unless opponent pays [2], Repeat. Counter exists; "unless they pay" needs a pickXAmount-style opponent choice between paying or letting the counter through.
- [ ] **Reveal-and-choose / Sabotage-style mid-reveal.** Mindsplitter / Sabotage approximate as `opponentDiscards(1)`. True implementation needs `CardRevealedEvent` emit sites + agent choice over revealed cards. Memory bank exists (`PlayerState::observed_cards`) but only Sabotage populates it.

## Known engine gaps / cleanup-tier follow-ups

- [ ] **Pre-move location in WhenIMove triggers**: The trigger fires after the move completes, so `ctx.source.location` is the destination. Cards that want the source location (Lillia, Fae Fawn) currently approximate by using the controller's base. Add a `from_location` field to the trigger context.
- [ ] **Optional-trigger agent choice — wire remaining cards through `confirmOptional`.** Mechanism is in place. Remaining candidates: any Card whose registry `ability_text` contains "you may" but whose `onTrigger`/`onPlay` runs the effect unconditionally — grep `ability_text` for "you may" in `cards/registry.json` to find them.
- [ ] **Equipped triggers through chain** — `TriggerManager::fireEquippedTriggers` calls `onEquippedTrigger` directly, bypassing `state.chain.resuming`. Means `requestChoice` from an equipped-trigger card silently fails (e.g. Doran's Ring [445] still calls `discardCards` directly because the resume pattern can't reach it). Reroute through the chain.

## OpenSpiel integration knowledge

### Build / run invocations
```bash
# Release strongly recommended — Debug is ~5× slower
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DRIFTBOUND_BUILD_OPENSPIEL=ON
cmake --build build-release --target riftbound_openspiel riftbound_tests

# Random vs Random
./build-release/src/openspiel/riftbound_openspiel --agent1 random --agent2 random --games 1000 --threads 8

# MCTS vs Random
./build-release/src/openspiel/riftbound_openspiel --agent1 random --agent2 mcts:sims=5 --games 100 --seed 42

# Unit tests
cd build && RIFTBOUND_ROOT=.. ./riftbound_tests
```

### Known OpenSpiel build mechanics
Gotchas preserved for next-agent:
- OpenSpiel's CMake reads **environment variables** not cache vars for its build options. Use `set(ENV{OPEN_SPIEL_BUILD_WITH_*} "OFF")` from the parent project.
- OpenSpiel doesn't ship abseil / nlohmann/json as submodules; `install.sh` clones them and `sudo apt-get`s system packages. We clone abseil + json + pybind11_json + DDS ourselves in our parent `CMakeLists.txt`.
- `open_spiel_core` is an OBJECT library; its PUBLIC includes only expose `${open_spiel-src}/open_spiel`, not the parent. Add `target_include_directories(YOUR_TARGET PRIVATE ${open_spiel_SOURCE_DIR})` and replicate the abseil link list from OpenSpiel's directory-level `link_libraries(...)`.

### Resume pattern for in-flight chain resolution
Live mechanism documented for next-agent:
- `ChainItem::resume_point` (int) + `resume_data` (vec<int32_t>) for saved progress.
- `ChainState::resuming` (optional<ChainItem>) holds the resolving item across iterations.
- `EffectExecutor::{requestChoice, recordChoice, takeChoice, applyDiscard}` — publish/consume choice slot.
- `ChainManager::stepResolve` pops to `resuming`, calls `resolve_spell` in a `while(true)`, breaks on no pending choice. Counter spells unaffected (peek `items.back()`, not `resuming`).
- Slot ranges by helper (so multiple helpers in one onResolve compose cleanly):
  - `confirmOptional` / `pickXAmount`: resume_points 0/1/2 + resume_data[0]
  - `pickMode`: resume_points 3/4/5 + resume_data[1]
  - `pickTarget` / `pickTargetPair`: resume_points 6/7/8 + resume_data[2]

## Future Work

**Training pipeline:**
- [ ] **Multi-iter training run with loss-curve validation** — confirm losses trend down across 20+ iterations on a mirror, then on heterogeneous matchups.
- [ ] **`Play` slot target-variant collapsing — full sweep.** Highest-impact spells already migrated; most generated cards still have the collision. Either change the default of `needsPlayTimeTarget` to true and regen via `generate_cards.py`, or migrate-as-touched. Activated-ability variant has its own pending migrations (see "Mechanical features TODO").
- [ ] **TorchScript inference path** — current model is loaded via `torch::load` (native libtorch state_dict). For external deployment (CLI agent loadable without the full build) wrap a `LibTorchAgent` around a `.pt` TorchScript module produced by `torch::jit::trace(model, dummy_input)` in the trainer's checkpoint code.
- [ ] **Per-archetype league + Elo** — once a model trains: train per-legend, round-robin tournament, Elo per matchup, focused retraining on weak matchups.
- [ ] **Pointer-head supervision (V2 model)** — `action_type_logits` / `source_logits` / `target_logits` / `dest_node_logits` are computed but not supervised. Build action_decomposer that maps OpenSpiel Action id → (action_type, source_idx, target_idx, dest_node); add separate CE losses; combine into flat-policy at inference. See `src/ml/v2_model.h` + `docs/potential-model-architecture.txt`.
- [ ] **Auxiliary loss heads** — turn-phase prediction, who-scores-next, score-differential. Cheap signal that helps the trunk converge.
- [ ] **Eval pipeline on GPU** — eval subprocess currently runs on CPU at threads=1 (~hours per iter). Disabled in `configs/gpu_bootstrap_*.json` for this reason. Wire GPU device support into the eval-binary invocation in `config_driver.cpp`.

**OpenSpiel correctness:**
- [ ] **Chance nodes** — extract `std::mt19937_64 rng_` from `GameEngine` into a pluggable `ChanceSource`. Implement `RiftboundState::ChanceOutcomes()` (incremental deck realization, pattern from `universal_poker` / `hearts`). Flip `GameType::chance_mode` to `kExplicitStochastic`. Required for CFR / NFSP / theoretically-correct imperfect-info algorithms; AlphaZero/MCTS work fine without it.
- [ ] **Observation tracking** — `CardRevealedEvent` is dispatched but only Sabotage emits + populates `PlayerState::observed_cards`. Wire remaining emit sites (Aurora / Mindsplitter / Vision / Predict) and expose `observed_cards` in `extractStateFeatures` (bumps state-dim — coordinate with trainer). See `docs/additional-gamestate-dims.md`.
- [ ] **Proper ISMCTS hidden-info resampling** — current `Clone()`-only resampler doesn't determinize. Real determinization should: read `PlayerState::observed_cards` memory bank, shuffle opp's hand and deck from the unseen pool, randomize facedown card identities at BFs.
- [ ] **Full memcpy Clone** — replace lazy-thread Clone with `std::make_unique<RiftboundState>(*this)` once `GameEngine::step_thread_` is fully removed. Would drop Clone from ~8 μs to ~1 μs. Not needed for current DoD; skip unless training throughput bites us.

**Engine features:**
- [ ] **Match runner**: Best-of-3 with sideboarding and battlefield rotation (CR 481).
- [ ] **Deck builder**: `scripts/deckbuilder.py` — evolutionary optimization, ban-list aware.

**Repo hygiene:**
- [ ] **Delete stale branches**: `phase-c1/commit-5-runmainphase-step-machine`, `phase-c2a/pr-1-v2-tensors-foundation`, `phase-c2a/pr-2-extract-entity-tokens` — all fully ancestral to `master`.
- [ ] **Drop `stash@{0}`** — 52 commits behind master, references files deleted in `752e54f` (Python ML cleanup). Fully obsolete.

## Key Design Docs
- `docs/engine-design.md` — comprehensive engine architecture (read this first)
- `docs/engineering-history.md` — chronological archive of completed phases. Pulled out of CLAUDE.md to keep this file focused on live state.
- `docs/potential-model-architecture.txt` — V2 model design (transformer + spatial fusion + pointer heads)
- `docs/additional-gamestate-dims.md` — game state field backlog with priority tiers + position assignments
- `docs/deck-agent-design.md` — deck construction agent design (not yet built)
- `docs/playmat-layout.md` — physical board layout → ASCII renderer reference
- `docs/ml-training-design.md` — historical ML pipeline design (Python-era, superseded; see commit `752e54f`)
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
- **Token cease-to-exist (CR 183.1):** Tokens routed to any non-board zone cease to exist immediately. Engine handles this by routing tokens to Banishment without adding to player.banishment vector.
