# Engineering History (Riftbound Engine)

Chronological record of completed phases. This is the historical archive — the live tracker for active work is in CLAUDE.md and the git log. Each section below documents what landed, why, and any non-obvious decisions.

---

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
- 72 tests passing (current count: 366 — see Phase 5e)

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
- **HTML replay**: `--render` (legacy CLI) and `--render-html` (OpenSpiel CLI) generate `replay_gameN.html` with UUID, seed, board state + decision + trace log panels, arrow-key navigation, ★ SCORE banner on scoring snapshots.
- **State features** — `src/ml/feature_extractor.cpp` produces a perfect-information state vector (currently `kStateFeatureDim = 4623`, padded to `RESERVED_STATE_DIM = 4864` for forward-compatible additions). Used by `RiftboundState::ObservationTensor` to expose game state to OpenSpiel algorithms (MCTS today; AlphaZero / NFSP / etc. when LibTorch lands). Layout details in `feature_extractor.h` + `docs/additional-gamestate-dims.md`. ObservationTensor masks hidden information per perspective (opponent's deck and hand stay hidden; opp trash/banishment remain public per CR).
- **Coin toss** — `TurnState::starting_player` records who won the coin flip (CR 116).

### Adding state to feature_extractor

When you add a new field to GameState / PlayerState that should be visible to a future ML agent, also bump `kStateFeatureDim` in `src/ml/feature_extractor.h` and append the new dim(s) to `extractStateFeatures` in the .cpp. This is what `RiftboundState::ObservationTensor` reports to OpenSpiel. There is no longer a Python parity surface to keep in sync — the layout is C++-only. `docs/additional-gamestate-dims.md` lists the legacy backlog (some already landed) for reference.

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
- 72 tests passing (current count: 366 — see Phase 5e)

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
- 72 tests passing (current count: 366 — see Phase 5e)

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
- 72 tests passing (current count: 366 — see Phase 5e)

### Completed (✅) — Phase 5c: Equip System
- ✅ **Full equip cost payment**: 33 equip gear cards manually implemented in `src/cards/manual/equip_cards.cpp`. Each card handles its own cost via `onEquip()` — domain power recycling, energy exhaustion, special costs (Last Rites: recycle 2 from trash). Uses `SimpleEquipGear` and `UniversalEquipGear` base classes.
- ✅ **Equipment effect text triggers**: `equippedTriggerType()` and `onEquippedTrigger()` on Card base class. TriggerManager checks attached gear when units attack/defend/conquer/hold/move/die. Effect text logic lives in the gear's Card object (e.g., Warmog's Armor buffs on conquer, Trinity Force scores on hold).
- ✅ **Equipment-granted keywords**: `equippedKeywords()`, `equippedAssault()`, `equippedShield()` on Card. Applied during aura recalculation (Step 3c). Serrated Dirk grants [Assault 2], Cloth Armor grants [Shield 2], Boots of Swiftness grants [Ganking], etc.
- ✅ **Weaponmaster keyword**: 9 Weaponmaster units in `src/cards/manual/weaponmaster_cards.cpp`. On-play trigger: find Equipment on board, detach from current unit if needed, attach to self for free ([A] less cost).
- ✅ **Render shows attached equipment**: Unit render includes `EQ:GearName` suffix when gear is attached.
- ✅ **Token safety guards**: All `card_db_.get(obj.card_def_id)` calls guarded with `card_def_id != kInvalidId` check. Prevents crash when tokens (no CardDef) are on board.
- ✅ **CardDefId vs GameObjectId crash fix**: `cardDefId()` (static card template ID, e.g., 601) was being passed where `ctx.source` (runtime instance ID, e.g., 70) was needed in equip cost payment. Caused assertion failures in `getObject()` for Ornn, Rengar, and VexPreCon decks. Fixed by using `ctx.source` in all `standardEquip()` and `UniversalEquipGear::onEquip()` calls.
- ✅ **Full round-robin verified**: All 15 deck matchups (5 decks x 5 decks, including mirrors) complete with zero crashes, 72 tests passing.
- 72 tests passing (current count: 366 — see Phase 5e)

### Completed (✅) — Phase 5d: Agent Decisions & Manual Cards
- ✅ **Rune selection agent choice**: Both energy exhaustion and power recycling query agent via MakeChoice. Order: exhaust first (for energy), then recycle exhausted runes for power (efficient). Each rune choice is a decision point for ML training. Tagged `[agent choice]` in trace.
- ✅ **25 manual card implementations** (`src/cards/manual/deck_cards.cpp`): Champions (LeBlanc Deathknell draw, Ahri hold=score), Legends (Gloomist hold=draw, Deceiver Reflection tokens, Fire Below the Mountain [Add][A]), Deathknell units (Ruined Rex deal 4, Black Rose channel, Glasc Mixologist play from trash), Score triggers (Sona ready 4 runes), Spells (Cull the Weak mutual kill, Defy counter+draw, Sprite Burst 2 tokens), Gear (Seal of Focus/Strength, Sprite Fountain tokens), Units (Sprite Mother token on play, Pit Rookie buff).
- ✅ **Full round-robin re-verified**: All 15 matchups (5 decks), zero crashes, card effects working (Ahri scoring, Sona rune-readying, Deathknell triggers, token creation all visible in traces).
- 72 tests passing (current count: 366 — see Phase 5e)

### Manual Card Coverage Summary
| File | Cards | Scope |
|------|-------|-------|
| `src/cards/generated/*.cpp` | 787 | All cards (209 fully generated, 578 partial) |
| `src/cards/manual/equip_cards.cpp` | 33 | Equipment gear with cost payment + triggers |
| `src/cards/manual/weaponmaster_cards.cpp` | 9 | Weaponmaster units (on-play equip) |
| `src/cards/manual/deck_cards.cpp` | 34 | Champions, legends, key units/spells in test decks (incl. Aurora, MF Captain, Elder Dragon, Baron Nashor, Bullet Time, Brynhir, Forerunner, Noxus Hopeful, Pouncing) |
| **Total manually implemented** | **76** | Overwrite generated stubs with full behavior |

### Remaining Work — Polish for v1.0

#### Do-nothing cards in test decks (~1 remaining; 41 fixed in 2026-05-15 session — see ✅ markers below)
Audited all 10 test decks. These cards have ability text but no working implementation (empty stubs or comment-only generated code). 2 keyword-only cards (Mutated Mouser, Rengar Unseen) are false positives — engine handles their keywords already. Priority: champions/legends first, then high-frequency cross-deck cards.

**Champions & Legends (11 cards — highest priority, distort training)**
- [x] **[162] Miss Fortune, Captain** — move→ready something else. Decks: miss_fortune_test  ✅ uses new `WhenAFriendlyUnitMovesToFB` trigger
- [x] **[28] Draven, Showboat** ✅ — dynamic might = controller's points (engine-side aura source in recalculateAuras)
- [x] **[348] Rengar, Pouncing** — Reaction play to attacking BF. Decks: rengar_test  ✅ uses new `Card::playableAsReactionToAttack()` hook
- [x] **[543] Sett, Brawler** ✅ — buff me on conquer/hold; spend-buff activated for +4M
- [x] **[644] Lillia, Fae Fawn** ✅ — on move, create Sprite token (Temporary). Approximate: created at controller's base, not the source location
- [x] **[705] Kha'Zix, Mutating Horror** ✅ — on attack/defend, if enemy alone +2M and +2 XP
- [x] **[552] Glorious Executioner** (legend) ✅ — on win combat, draw 1 (new WhenIWinCombat trigger)
- [x] **[744] Pridestalker** (legend) ✅ — on friendly unit play, +1M to a friendly
- [x] **[749] Bashful Bloom** (legend) ✅ — [4],[E]: play 3M Sprite (Temporary). Cost reduction per Temporary friendly NOT modeled
- [~] **[787] Voidreaver** (legend) ⚠️ — +1 XP on win combat + Spend 1 XP+[E]: buff a unit. Second "Spend 2 XP move exhausted to base" ability now SUPPORTED by Phase 6r multi-ability scaffolding but Voidreaver's Card class hasn't been migrated yet to expose both — still only models the cheaper one. Migration is mechanical: override `activatedAbilities()` to return both descriptors, override `onActivate(ctx, idx, targets)` to dispatch on idx, and add a `moveToBase` EffectExecutor helper for the second variant.
- [x] **[262] Bounty Hunter** (legend) ✅ — [E]: give a unit Ganking this turn

**Counter/Reaction spells (6 cards)**
- [x] **[368] Not So Fast** ✅ — counter enemy spell/ability targeting friendly card
- [ ] **[457] Hard Bargain** — counter unless they pay [2], Repeat. Decks: khazix_test, miss_fortune_test, sett_test
- [x] **[668] Repulse** ✅ — counter enemy spell/ability targeting a friendly unit
- [~] **[737] Tactical Retreat** ⚠️ — replacement: would die → heal+exhaust+recall. Decks: leblanc_test. Approximation present in implementation per 2026-05-19 audit; full replacement-effect-via-Card-class semantics not yet wired.
- [x] **[750] Lilting Lullaby** ✅ — counter + new cant_play_spells_this_turn flag locks out target controller's spell-play actions for the turn
- [x] **[693] Abandon** ✅ — counter spell, return to hand instead of trash, Predict 1 (already implemented in generated stub; verified 2026-05-15)

**Spells with effects (10 cards)**
- [x] **[156] Sabotage** ✅ — approximated as opponent discards 1 (reveal-and-choose UI deferred)
- [x] **[263] Bullet Time** ✅ — pay any [X][A] to deal X damage to all enemy units at target BF. Full migration in Phase 6q+5g: uses `pickXAmount` (slots 0/1/2 + data[0]) for X-amount choice and `pickTarget` (slots 6/7/8 + data[2]) for target choice. Decks: miss_fortune_test.
- [x] **[484] Deathgrip** ✅ — kill target[0], +M(=killed.might) to target[1], draw 1
- [x] **[657] Grim Resolve** ✅ — +3M to friendly (WhenIWinCombat XP rider deferred)
- [x] **[727] Shadow's Call** ✅ — give friendly Temporary, draw 2
- [x] **[735] Sacrifice** ✅ — kill friendly Mighty, draw 2, channel 1 rune exhausted (additional-cost approximated at resolve)
- [x] **[690] Star-Crossed** ✅ — bounce friendly + enemy (2 targets)
- [x] **[696] Existential Dread** ✅ — Action: stun attacker, or bounce if already stunned (Repeat is engine-handled)
- [x] **[600] Skyward Strike** ✅ — move enemy unit, Level 6: stun instead. Decks: vex_pre_con. Verified implemented per 2026-05-19 audit.
- [x] **[758] Void Assault** ✅ — move friendly then move enemy. Decks: khazix_test, sett_test. Verified implemented per 2026-05-19 audit.

**Units with triggers/effects (17 cards)**
- [x] **[27] Darius, Trifarian** ✅ — second unit played: +2M and ready (spell-play variant deferred)
- [x] **[12] Noxus Hopeful** — Legion cost reduction. Decks: draven_test, rengar_test  ✅ uses new `Card::selfCostReduction()` hook
- [x] **[26] Brynhir Thundersong** — on play, opponents can't play cards this turn. Decks: rengar_test  ✅ uses new `PlayerState::cant_play_cards_this_turn` flag, surfaced as state features at positions 4405–4406
- [x] **[192] Mindsplitter** ✅ — on play, opponent discards 1 (reveal step deferred to observation tracking)
- [ ] **[236] Karthus, Eternal** — Deathknell effects trigger additional time. Decks: leblanc_test. **MODERATE — needs `PlayerState::deathknell_double_count` tracked via aura recalc + TriggerManager bump in onUnitDied.**
- [x] **[344] Ferrous Forerunner** — Deathknell: play two 3M Mech tokens. Decks: draven_test, rengar_test  ✅
- [ ] **[352] Rek'Sai, Breacher** — friendly units from non-hand have Accelerate. Decks: draven_test. **COMPLEX — needs Intent::play_source field + payCardCost auto-Accelerate path.**
- [x] **[449] Overzealous Fan** ✅ — on defend, kill self + bounce attacker (treated as mandatory)
- [x] **[451] Treasure Hunter** ✅ — on move, create Gold gear token
- [x] **[461] Fizz, Trickster** ✅ — on play, play first spell from trash ignoring cost. Decks: draven_test. Verified implemented per 2026-05-19 audit (lacks ≤3 cost gate but functional).
- [x] **[476] Honest Broker** ✅ — Deathknell: create Gold gear token
- [x] **[583] Grim Apothecary** ✅ — Ambush + on play, bounce friendly unit (treated as mandatory)
- [x] **[674] Irresistible Faefolk** ✅ — on move to BF, move enemy unit there too via `confirmOptional` + `moveToBattlefield`. Decks: khazix_test, rengar_test, sett_test. Done in Phase 5h.
- [x] **[680] Elder Dragon** — your damage always kills, on play deal 1 to enemy at each location. Decks: miss_fortune_test  ✅ on-play AoE implemented; "any damage kills" passive was already engine-side
- [x] **[709] Baron Nashor** ✅ — As you play me: spawn the Baron Pit BF token if missing and enter there ("If you do"). `untargetable_by_enemy` (via `Card::canBeChosenByEnemy()`); +2M aura to other friendly units (text-matched in `recalculateAuras`). AoE spells (e.g. Flurry of Blades) still hit Baron — only *targeted* selection is blocked. Baron Pit sets `BattlefieldState::accepts_any_inbound`, which lets non-Ganking units move BF→Pit without returning to base. Decks: miss_fortune_test. (Earlier "deal 3 AoE on play" was a hallucination — removed.)
- [x] **[778] Plundering Poro** ✅ — on conquer, create Gold gear token
- [x] **[687] Lunar Boon** ✅ — discard 1, draw 2 (Reaction)

**Vex/XP-themed units (partially generated — Hunt XP works but Level effects broken) (6 cards)**
- [x] **[596] Herald of Spring** ✅ — Hunt (engine-handled) + on play +2 XP
- [x] **[602] Wuju Apprentice** ✅ — Hunt + Level 6: draw 1 on play (snapshot-at-play approximation)
- [x] **[609] Mosstomper** ✅ — Hunt 2 + Level 3: +1M and Deflect (snapshot-at-play)
- [x] **[656] Gemhand Hunter** ✅ — Hunt + Level 6: +1M (snapshot-at-play)
- [x] **[675] Master Yi, Tempered** ✅ — Hunt 2 + Level 6: Deflect + Ganking (snapshot-at-play)
- [x] **[689] Mister Root** ✅ — Accelerate + on move to BF, +2 XP

**Vex deck complex units (10 cards — vex_pre_con heavy)**
- [ ] **[467] Vex, Cheerless** — combat cost modification (friendly spells -1, enemy +1). Decks: vex_pre_con, vex_test_deck. **COMPLEX — needs CostModifier lifetime "during combat only" (`combat_active_only` scope) set/cleared at combat start/end.**
- [ ] **[597] Monch** — conditional cost reduction + enter ready if opponent has stunned unit. Decks: vex_pre_con. **TRIVIAL — uses `selfCostReduction(state, controller)` + `entersReadyOnPlay(state, controller)` hooks, just gate both on `oppHasStunned`.**
- [~] **[603] Allay, Eager Admirer** ⚠️ — aura: units here have Deflect. Decks: vex_pre_con. Most likely already handled by `recalculateAuras` text-matcher (per Phase 5b, 18 aura cards supported). Verify with a unit test before adding code.
- [x] **[605] Enthusiastic Promoter** ✅ — Backline + on hold, buff friendly units at my BF
- [x] **[610] Trevor Snoozebottom** ✅ — Shield + on hold, play 3M Sprite (Temporary) at my BF
- [ ] **[612] Iascylla** — on hold, delayed: move enemy unit here next main phase. Decks: vex_pre_con. **TRIVIAL approximation — implement as immediate WhenIHold + `confirmOptional` + `moveToBattlefield`; loses delay-to-next-main timing but gives policy signal.** Proper version needs `AtStartOfMain` trigger type.
- [x] **[614] Nami, Headstrong** ✅ — Verified implemented per 2026-05-19 audit (stuns on play; lacks additional-cost gate but functional). Decks: vex_pre_con.
- [ ] **[617] Vex, Mocking** — Shield+Tank + when you stun enemy, move me there. Decks: vex_pre_con. **TRIVIAL approximation — WhenIDefend + `confirmOptional` to stun attacker using Phase 5h `card_counters["__defend_attacker_id"]`. Loses move-to-stunned-BF tempo.** Proper version needs UnitStunnedEvent + WhenYouStun trigger type.
- [x] **[688] Megatusk** ✅ — activated, spend 3 XP: friendly units here get Ganking (XP cost managed manually)
- [x] **[703] Evelynn, Entrancing** ✅ — Hidden+Backline, from facedown: move enemy unit. Decks: vex_pre_con. Verified implemented per 2026-05-19 audit (with `confirmOptional`).
- [x] **[752] Shadow** ✅ — enters ready to BF (onPlay override) + [Action] [1][A],[E]: stun attacker

**Gear (5 cards)**
- [x] **[465] Spirit Wheel** ✅ — Verified implemented per 2026-05-19 audit. Decks: khazix_test, sett_test.
- [x] **[671] Blood Rose** ✅ — Verified implemented per 2026-05-19 audit (full XP gain + ready-unit activated). Decks: khazix_test, sett_test.
- [x] **[695] Blast Cone** ⚠️ — Verified per 2026-05-19 audit. On-play move implemented; "exhaust to stun moved enemy" follow-up activated ability not yet wired. Decks: vex_pre_con.
- [x] **[698] Scryer's Bloom** ✅ — enters exhausted; activated kill+[1]+[E]: Predict 2, draw 1, +1 XP
- [x] **[375] Heart of Dark Ice** ✅ — [E]: give a unit +3M this turn

**Per-deck impact summary (re-audited 2026-05-19):**
Most originally-flagged cards turned out to be implemented (the list was stale). Updated tallies below count only TRULY EMPTY stubs that affect training quality.

| Deck | Do-nothing | Remaining truly-empty cards |
|------|-----------|-------------------|
| vex_pre_con | 4 | Vex Cheerless (467 — COMPLEX cost-aura), Vex Mocking (617 — TRIVIAL approx), Monch (597 — TRIVIAL), Iascylla (612 — TRIVIAL approx). ⚠️ Most CLAUDE.md flags from 2026-05-15 were stale — Allay/Nami/Evelynn/Skyward/Blast Cone all working. |
| draven_test | 1 | Rek'Sai (352 — COMPLEX play-source tracking). Fizz verified working. |
| khazix_test | 1 | Voidreaver (787 — partial; only first ability via back-compat shim. Migrate to Phase 6r `activatedAbilities()`). |
| sett_test | ~1 | Same as khazix (shared pool); needs Voidreaver migration. |
| miss_fortune_test | 0 | All cards implemented or have working approximations. Bounty Hunter legend works; needs `needs_activation_time_target` migration for full vocab signal. |
| rengar_test | 0 | All 19 cards verified working per 2026-05-19 audit. Thrill of the Hunt migrated to `pickTarget`. |
| lilina_test | 0 | Lillia champion + Bashful Bloom legend implemented. |
| vex_test_deck | 0-1 | Vex still has Cheerless if present; Mutated Mouser keyword-only (false positive). |
| leblanc_test | 1 | Karthus (236 — MODERATE; needs PlayerState::deathknell_double_count). Tactical Retreat has approximation. |
| ornn_test | 0 | All verified working. |

**Total truly-empty cards across all 10 decks: ~7** (vs original 41 flagged). The 2026-05-19 audit found that most CLAUDE.md flags were stale — the work had landed in later phases without updating this section.

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
- [x] **`WhenIWinCombat` trigger type**: Fires in `TriggerManager::onCombatEnded` for each surviving unit at the BF whose controller equals the combat winner. Engine emits via `on_combat_ended` after `combatResolutionStep` determines the winner but before the control change. Used by Glorious Executioner (draw 1 on win) and Voidreaver (+1 XP on win). Snapshots winner-side units before iterating to avoid chain-mutation races.
- [x] **`ActivationCost::xp_cost` field**: Engine consults it in `generateActivateAbilityActions` (affordability) and deducts the cost in the `ActivateAbility` intent execution path. Used by Voidreaver (Spend 1 XP, [E]: buff a unit). Unlike the other ActivationCost fields, XP is a player-level resource — paid from `PlayerState::xp`.
- [x] **`PlayerState::cant_play_spells_this_turn` flag**: Turn-scoped lockout used by Lilting Lullaby (counter + opponent can't play spells this turn). Gates `generateSpellActions` to emit zero spell-play actions while set; main-phase / showdown / closed-state spell generators all consult it. Resets in `resetTurnTracking()`.
- [x] **Dynamic might aura ("might = your points")**: Text-match pattern in `recalculateAuras` adds a self-aura with `might_bonus = controller.score` on each cleanup pass. Used by Draven, Showboat. Pattern: any card whose `ability_text` (lowercased) contains "my might is increased by your points" picks up the aura automatically.
- [x] **`Card::canBeChosenByEnemy()` hook + `GameObject::untargetable_by_enemy` flag**: Refreshed during aura recalculation step 1b. `Card::enumerateLegalTargets` filters enemy targets where the flag is set, blocking spell/ability selection. AoE iteration (e.g. `EffectExecutor::dealDamage` called for every object) is *unaffected* — only targeted choice is blocked, matching CR "can't be chosen". Used by Baron Nashor.
- [x] **`EffectExecutor::addBattlefieldToken(name, accepts_any_inbound) → BattlefieldId`**: Creates a tokenized BF card object (no `card_def_id`) and appends a `BattlefieldState` with `is_token=true` to `state.battlefields`. Engine paths already iterate `state.battlefields` dynamically so a third BF "just works" for movement, scoring, and combat enumeration. Used by Baron Nashor's Pit spawn.
- [x] **`BattlefieldState::accepts_any_inbound` flag**: Movement-rule relaxer. `generateMainPhaseActions` BF→BF case emits a `StandardMove` intent if `has_ganking || bf.accepts_any_inbound`, so non-Ganking units can move from any BF to a flagged destination without first returning to base. Set by `addBattlefieldToken` when Baron Pit spawns.
- [x] **`toString(Keyword)`** (`src/core/types.cpp`): Enumerates all keyword names ("Ganking", "Assault", "Tank", …). Replaces the buggy "EFFECT: give X keyword 8 (value=0)" trace line emitted by `giveTemporaryKeyword` with the readable "EFFECT: give X [Ganking] this turn".

### Known engine gaps / cleanup-tier follow-ups
These don't block any current card or test; flagged for the next agent to fix when convenient.
- ✅ ~~**Multi-ability per Card.**~~ Foundation landed in Phase 6r (2026-05-19) — see that section. Card now has `activatedAbilities() → vector<ActivatedAbility>` with a backward-compat default that wraps the legacy single-ability virtuals. `Intent::ability_index` and `ChainItem::ability_index` carry the choice through the engine + chain. `action_vocab` encodes ActivateAbility slots as `verbBase + def_id_slot * kMaxAbilitiesPerCard + ability_index` (kMaxAbilitiesPerCard=4). Remaining work to actually USE the new shape: migrate Voidreaver (787) + Grandmaster at Arms (554) + Honeyfruit (611) to override `activatedAbilities()` and return both abilities. They currently still only expose the first ability via the back-compat shim.
- [ ] **Move-to-base effect**: No `EffectExecutor` helper for "move an exhausted friendly unit from a battlefield to its base." Needed for Voidreaver's second activated ability (above) and some Vex deck cards.
- [ ] **Pre-move location in WhenIMove triggers**: The trigger fires after the move completes, so `ctx.source.location` is the destination. Cards that want the source location (Lillia, Fae Fawn) currently approximate by using the controller's base. Add a `from_location` field to the trigger context.
- [ ] **Optional-trigger agent choice — wire remaining cards through `confirmOptional`.** Mechanism is in place (Phase 5f: `Card::confirmOptional`; Phase 5g: int-coded MakeChoice slots so yes/no is visible to OpenSpiel agents). Phase 5g rolled out 3 cards as a pilot — Overzealous Fan (449), Grim Apothecary (583), Irresistible Faefolk (674) — and added `EffectExecutor::moveToBattlefield` to support Faefolk. Verified across 20 random games: Apothecary fires `MAY_PROMPT` 5× / `MAY_ACCEPTED` 1×, Faefolk fires `MAY_PROMPT` 2× / `MAY_ACCEPTED` 0× (rare trigger condition in random play). Remaining candidates: any Card whose registry `ability_text` contains "you may" but whose `onTrigger`/`onPlay` runs the effect unconditionally — grep `ability_text` for "you may" in `cards/registry.json` to find them. Each refactor follows the same template: define a `still_legal` closure, call `auto conf = confirmOptional(ctx, "Card: prompt", still_legal); if (conf < 1) return;`, then run the effect. Verify with `grep -c "MAY_PROMPT.*<CardName>"` on a rendered replay.
- ✅ ~~**Pre-existing: Overzealous Fan WhenIDefend trigger no-ops at resolve time.**~~ Fixed in Phase 5h (2026-05-17). `TriggerManager::onCombatStarted` now captures the current attacker GameObjectId into the defender's `card_counters["__defend_attacker_id"]` BEFORE adding the trigger to the chain. `MOverzealousFan::onTrigger` reads from card_counters instead of scanning for `combat_designation == Attacker` (which is cleared by resolution time). Verified across 20 random games: 3 Fan triggers fired → 2 prompted → 2 accepted → 2 successful kill+bounce executions (was 0 before).
- [ ] **Reveal-and-choose / Sabotage-style mid-reveal**: Mindsplitter / Sabotage approximate as `opponentDiscards(1)`. True implementation needs `CardRevealedEvent` (already declared in `events.h`) emitted on reveal + agent choice over revealed cards.

### Completed (✅) — Phase 6a: Batch Game Runner
- ✅ **Parallel execution via `boost::asio::thread_pool`**: `BatchRunner` posts `GameRunner` work units to pool. `--threads N` CLI option (default: 1, 0 = hardware concurrency).
- ✅ **Manual DI via `AppContext`**: `CardDB` and `CardRegistry` loaded once as shared singletons (const after init). Per-game `EventBus`, `GameEngine`, agents constructed on worker threads. Zero cross-thread mutable state.
- ✅ **`GameRunner`** (`src/engine/game_runner.h/cpp`): Encapsulates all per-game logic — EventBus, engine, renderer, serializer, replay writer, agents. Thread-safe: all state is stack-local.
- ✅ **`BatchRunner`** (`src/engine/batch_runner.h/cpp`): Wraps `boost::asio::thread_pool`. Single-threaded fallback when `--threads 1`. Progress indicator for batches >20 games.
- ✅ **`AggregateResults`**: Atomic counters (p1_wins, p2_wins, draws, total_turns, total_decisions). `std::mutex` for console output. No lock contention on game logic.
- ✅ **CardRegistry refactored to shared const**: `GameEngine` takes `const CardRegistry&` instead of owning a value. `loadAll()` called once at startup. Card objects are stateless — concurrent reads safe.
- ✅ **Per-game JSONL output**: Each game writes to its own file (`output_path.gameN`). No file contention across threads.
- ✅ **Performance**: 1 thread ~18 games/sec, 4 threads ~43 games/sec (2.4x speedup on mixed workload).
- 72 tests passing (current count: 366 — see Phase 5e), all deck matchups verified.

### Completed (✅) — Phase 5e: Ivern Deck + Replay UI + CR Token Fix (2026-05-16)

Comprehensive card-implementation pass for the new Ivern test deck, plus
a CR-correctness fix surfaced during V&V of an Ivern vs Miss Fortune
MCTS replay, plus UX upgrades to the HTML replay viewer.

**Baron Nashor [709] — "as you play me" timing fix**
- Original implementation used `TriggerType::WhenYouPlayMe` so the Pit
  spawn went on the chain after Baron resolved to base. Per CR 135.2.b.3
  + 355.1, "as you play me" instructions execute DURING the play action,
  not as a triggered ability. Now uses `Card::onPlay(ctx)` (invoked in
  `executePlayCard` between cost payment and chain insertion) — atomic
  with the play, no chain item, no priority window. Baron's location is
  rewritten before `resolvePermanent` emits `EnteredBoardEvent`, so
  opponents have no opportunity to interrupt his Pit entry.
- `Card::canBeChosenByEnemy()` virtual + `GameObject::untargetable_by_enemy`
  flag (refreshed during aura recalc). `Card::enumerateLegalTargets`
  filters enemy targets where the flag is set. AoE still hits — only
  targeted selection is blocked, matching CR "can't be chosen".
- `EffectExecutor::addBattlefieldToken(name, accepts_any_inbound)` spawns
  the third BF slot. `BattlefieldState::accepts_any_inbound` relaxes
  BF→BF movement (Baron Pit lets any unit move there without Ganking).
- 11 unit tests in `tests/cards/test_baron_nashor.cpp` — including the
  sequential-play case (P1 plays Baron, then P2 plays Baron: exactly 1
  Pit total, P1's Baron in it, P2's Baron stays at base per "If you do,
  I enter there").

**Sabotage [156] — proper reveal + memory bank**
- Previous implementation was an approximation ("opponent discards 1").
  Real card: caster sees opp's hand, chooses a NON-UNIT card, recycles
  it to the bottom of opp's deck.
- Now: emits `CardRevealedEvent` for every card in opp's hand (private
  to caster), filters choice set to non-units, caster (not opponent) is
  the chooser, chosen card recycled (not discarded).
- New `PlayerState::observed_cards : map<CardDefId,int>` memory bank
  populated by `GameEngine`'s `CardRevealedEvent` subscriber via shared
  `GameState::recordReveal` helper. Caster's bank gets +1 per revealed
  card_def_id; opp's bank untouched. Not yet featurized for the trainer.
- 6 unit tests in `tests/cards/test_sabotage.cpp`.

**CR 183.1 — tokens cease to exist on non-board transition**
- Surfaced in MCTS replay: a Bird token bounced to hand survived as a
  hand card; Sabotage then "revealed" the non-existent card to the
  opponent, polluting observation tracking.
- Per CR 183.1: "If a token is put into any Non-Board Zone besides the
  chain, it ceases to exist immediately after moving to its new zone."
- Both kill paths patched: `EffectExecutor::killObject` (spell/ability
  kills) and `GameEngine::killUnit` (combat lethal-damage kills) route
  tokens to Banishment zone WITHOUT adding to player.banishment vector
  — the GameObject stays in `state.objects` for ID safety but is
  effectively gone (no zone-vector entry). `EffectExecutor::bounceToHand`
  similarly banishes tokens instead of routing to hand.
- LeBlanc-deck interaction PRESERVED: Reflection tokens copied via
  Mirror Image inherit the source's `card_def_id` (e.g. LeBlanc
  Fragmented = 734). When the copy dies, `UnitDiedEvent` still fires
  with that `card_def_id`, so TriggerManager's Deathknell lookup
  succeeds and the chain item resolves normally — even though the
  token itself is in Banishment by the time the trigger fires.
- 7 unit tests in `tests/cards/test_tokens_cease_to_exist.cpp`,
  including `CopyUnit_PreservesCardDefIdOnToken` and
  `ReflectionCopyEmitsUnitDiedEventWithCardDefId` for the LeBlanc path.

**Ivern Deck — full implementation (15 cards)**
Manual `Card` subclasses in `src/cards/manual/deck_cards.cpp`:
- **Daisy! [754]** — `entersReadyOnPlay()` virtual (new Card hook),
  `selfCostReduction()` returns count of Bird/Cat/Dog/Poro tags among
  friendlies, `WhenIAttack` triggers stun if all 4 tags present.
- **Ivern, Friend to All [739]** — `onPlay` appends "Bird" tag
  (hardcoded default; agent-choice during play deferred),
  `WhenIConquerOrHold` scores 1 if all 4 tags present.
- **Green Father legend [753]** — `WhenIConquerOrHold` exhausts +
  replaces the held BF with a "Brush" token via new
  `EffectExecutor::replaceBattlefieldWithToken`. Brush aura
  ("B/C/D/P/Ivern +1M") wired in `recalculateAuras` keyed on BF name.
- **Ivern, Nurturer [613]** — Top-3 deck peek, drafts first unit found,
  recycles rest, themed-buff if drafted card has B/C/D/P tag. Multi-
  trigger (play + hold) noted as known gap; only play wired.
- **Trusty Ramhound [480]** — conditional +1M aura in `recalculateAuras`.
- **Frisky Hunter [595]** — `WhenYouPlayMe` creates 1M Bird token with
  Deflect at my location.
- **Friendship [608]** — +1M per distinct tag in {Bird, Cat, Dog, Poro}.
- **Loyal Poro [718]** — `WhenIDie` + new `GameObject::last_location`
  (preserved by killObject before clearing `location`) — draws 1 if
  another friendly was at the death location.
- **Vilemaw [622]** — `WhenIHold` draw 1. "Less Might → no combat
  damage" passive deferred as engine gap.
- **Vilemaw's Lair [290]** — `BattlefieldState::blocks_move_to_base`
  text-parsed in `setupBattlefields`; honored by `generateMainPhaseActions`.
- **Rockfall Path [530]** — `BattlefieldState::blocks_unit_play`
  text-parsed; gates PLAYS only, MOVES preserved per CR.
- **Vaults of Helia [775]** — `WhenYouHoldHere` appends `CostModifier`
  with new `energy_increase=1` + `affects_non_token_only=true`.
- **Flurry of Feathers [606]** — modal: counter chain-top if a spell,
  else create 4 Bird tokens with Deflect.
- **Hidden Blade [213]** — kill target + its controller draws 2.
- **Back Off [604]** — stun + draw 1.
- 24 unit tests in `tests/cards/test_ivern_deck.cpp` cover the
  corner cases (union completeness, tag scaling, BF restriction text
  parsing, draft branches, etc.).
- Plus 5 new behavioral tests for BF restrictions + reinforce
  (`Rockfall_BlockedBF_NoPlayCardIntentEmitted`,
  `VilemawsLair_BlocksMoveFromBFToBase`, `Reinforce_PlayCardToControlledBFIsLegal`,
  etc.) — first end-to-end tests of `generateLegalActions` under
  specific BF flags.

**New engine surface (Phase 5e)**
- `Card::onPlay(ctx)` invocation site in `executePlayCard` (between cost
  payment and chain insertion). Atomic with the play, no chain priority
  window. Used by Baron Nashor and Ivern, Friend to All.
- `Card::entersReadyOnPlay()` virtual consulted in `resolvePermanent`.
- `Card::canBeChosenByEnemy()` virtual + `GameObject::untargetable_by_enemy`.
- `GameObject::last_location` preserved in `killObject` / `killUnit` for
  Deathknell triggers that need to know "where did I die".
- `BattlefieldState::blocks_move_to_base`, `blocks_unit_play`,
  `accepts_any_inbound` — text-parsed in `setupBattlefields`; honored
  by action generators.
- `PlayerState::CostModifier::energy_increase` + `affects_non_token_only`
  for "your non-token units cost [1] more this turn"-style effects.
- `PlayerState::observed_cards : map<CardDefId,int>` memory bank +
  `GameState::recordReveal(event)` template helper. Engine + tests
  share one bump path.
- `EffectExecutor::addBattlefieldToken`, `replaceBattlefieldWithToken`.
- `TriggerType::WhenYouHoldHere` wired by Vaults of Helia.
- `toString(Keyword)` replaces the "keyword 8 (value=0)" trace bug.

**HTML Replay UI upgrades**
- **Clickable card-name → image side pane**. `ReplayWriter::loadCardImageMap()`
  reads `cards/raw/gallery_raw.json` once and emits a `CARD_IMAGES =
  {name: url}` JS object embedded in each replay (~767 entries, ~80KB).
  A new `#card-details-panel` column on the right of `#main` shows the
  official Riot card art on click. JS `wrapCardNames(elem)` walks text
  nodes in board / decision / trace panes after each `show()` and
  replaces card names with `<span class="card-link">`. Sort-by-length-
  desc so "Ivern, Friend to All" doesn't get pre-empted by "Ivern".
- **Resize handles** between panes (board ↔ right-panel, decision ↔
  trace, right-panel ↔ card-details). 4px navy bars with accent on hover.
  PointerEvent-based drag with `setPointerCapture`. 80px min per pane.
- **Richer counter format**: `152 / 320 · T8 P1 Main` (parses the turn
  string for context).
- **Token cease-to-exist trace lines**: `TOKEN CEASES TO EXIST (CR 183.1):
  Bird (id=109)` so the replay surfaces when a token gets banished.
- **Channel-phase rune naming**: `CHANNELED: Calm Rune (id=42, ready)`
  per rune channeled.
- **Drop decision-action truncation in state_renderer**: card names in
  Play / Move / Activate / Choose / Mulligan actions are no longer
  truncated to 16 chars (broke the card-link wrapper). Board ASCII
  playmat truncation kept (fixed-width columns).

**Bulk replay generation (`scripts/generate_replays.py`)**
- Python script that runs N games for every deck-pair combination in
  `decks/` and stashes per-pair HTML replays under
  `replays/<deck1>_v_<deck2>/`. `--pair-mode combinations` (default,
  unordered + mirrors, 66 pairs for 11 decks) or `permutations`
  (ordered N×N, 121 pairs).
- argparse + `logging` + `tqdm` + `ThreadPoolExecutor` for stacked
  progress bars (1 outer matchups + N inner per-thread). Inner bars
  tick on each `Game N:` line from the binary's stdout (line-buffered
  via `stdbuf -oL`). Intra-game ticks not surfaced — would need a
  small `--progress-emit` flag on the binary; deferred.

- 366 tests passing (was 313 pre-Phase 5e).

### Completed (✅) — Phase 5f: Resumable Agent-Choice API (2026-05-17)

Three sequential-decision helpers on `Card` for the most common
"agent picks at resolve time" patterns. **Important: these are the
canonical, in-place APIs — do NOT build a parallel/duplicate framework.**
All three follow the same resumable shape (publish via
`EffectExecutor::requestChoice`, return `-1`, consume via
`takeChoice`, advance `state.chain.resuming->resume_point`).

| Helper | Card.h doc | What it does | resume_points / resume_data slots used |
|---|---|---|---|
| `Card::confirmOptional(ctx, label, still_legal)` | yes/no for "you may [X]" triggers | Two-pass: validates BEFORE prompting AND after agent says yes. Returns -1 / 0 / 1 | 0, 1, 2 |
| `Card::pickXAmount(ctx, label, min, max)` | "Pay any amount of [X]" variable cost | Publishes (max-min+1) MakeChoice options, agent picks X. Returns -1 / chosen X | 0, 1, 2; `resume_data[0]` |
| `Card::pickMode(ctx, label, num_modes, mode_labels, legal_modes)` | "Choose one — A / B / C" modal spells | Publishes one MakeChoice per legal mode (filtered by bitmask). Returns -1 / -2 / mode-idx | 3, 4, 5; `resume_data[1]` |

Helpers coexist on the same Card (Bullet Time uses `pickXAmount`,
Curtain Call uses `pickMode`, Rocket Barrage uses `pickMode`). The slot
reservations DON'T COLLIDE — `pickXAmount` uses points 0/1/2 with
`resume_data[0]`, `pickMode` uses 3/4/5 with `resume_data[1]`.

**Trace tags (greppable for V&V):**
- `MAY_PROMPT`, `MAY_DECLINED`, `MAY_ACCEPTED`, `MAY_INVALIDATED`,
  `MAY_NOT_OFFERED` — optional-trigger flow
- `X_PROMPT`, `X_PICKED`, `X_NOT_OFFERED` — variable-X flow
- `MODE_PROMPT`, `MODE_PICKED`, `MODE_FORCED`, `MODE_NOT_OFFERED` —
  modal flow

**Cards on the API today:**
- `confirmOptional` — `MVirtuoso` (782) "may banish 4-cost spell"
- `pickXAmount` — `MBulletTime` (263), `MHextechAnomaly` (405)
- `pickMode` — `MRocketBarrage` (400, 2 modes), `MCurtainCall` (743,
  4 modes with used-mask filter across Repeat re-runs)

**Scoring regression fix (2026-05-17):**
Contested-BF staging was single-shot — once `bf.is_contested = true` was
set, subsequent moves into the same BF skipped the
`showdown_staged`/`combat_staged` assignment. If a showdown ended with
0 units on BOTH sides (sole_player=None), control was never assigned and
the BF stayed permanently stuck — game ran to the 200-turn cap, 0-0
draw. Now `executeStandardMove` and `resolvePermanent` re-evaluate
staging on every arrival at a contested BF whose staging slots are all
free. Regression tests in `tests/cards/test_burn_out_and_scoring.cpp`:
`ContestedBF_StuckAfter0vs0Showdown_NextMoveReStages` /
`...MoveByOpponentStagesCombat`.

**Action-side Repeat-choice generation (CR 820)**
`GameEngine::parseRepeatCost(text) -> RepeatCost{energy, power, power_domain, valid}`
parses the first `[Repeat] [N]` / `[Repeat] [N][D]` / `[Repeat] [A]` /
`[Repeat] [D]` token. `generateSpellActions` consults it for every
spell-play candidate: if Repeat is valid, the base play intent is
duplicated once per affordable additional tranche, with `chosen_value = R`
(R extra repeats). Capped at R=6 to keep the action space bounded.
`executePlaySpell` reads `intent.chosen_value`, calls
`payRepeatCost(player, repeat_cost)` once per extra tranche, sets
`chain_item.repeats_paid = R` and `total_energy_spent = base + R *
repeat_energy`. `ChainManager::stepResolve` already loops over
`repeats_paid` extra resolutions in its post-resume tail.

Limitation: Curtain Call has three modal Repeat costs
(`[Repeat] [1]/[A]/[1][A]`) — `parseRepeatCost` picks the FIRST `[1]`,
which means action-gen offers a fixed-cost Repeat. The card's onResolve
modal logic handles the actual mode selection. For full modal-Repeat
support, the per-mode cost would need to be the post-pickMode payment.
Future work.

### Completed (✅) — Phase 5g: Int-coded MakeChoice slots (2026-05-17)

Training-pipeline correctness fix. Every int-coded mid-resolve choice
(yes/no triggers, modal-spell mode picks, variable-X picks) now occupies
a DISTINCT slot in the OpenSpiel action vocabulary, so the policy head
can learn meaningful preferences between options.

**The bug.** `Card::confirmOptional` previously encoded "yes" by stuffing
`kYesSentinel = 0` into `chosen_objects`. The action_vocab MakeChoice
encoder computed the slot from `defIdOf(state, chosen_objects[0])`,
which returns 0 for any non-existent GameObjectId. So both `no_choice`
(empty chosen_objects) and `yes_choice` (chosen_objects = {0}) encoded
to MakeChoice slot 0. `RiftboundState::LegalActions` then dedupes by
encoded slot via `unordered_set` (riftbound_state.cpp:144-150), so
OpenSpiel saw exactly ONE legal action per confirmOptional prompt — the
first inserted, always `no_choice`. `decodeAction` returned that intent;
the agent never had a chance to pick "yes". Verified in a 6-seed Jhin
mirror: 0 of 43 Virtuoso `MAY_PROMPT`s were ever accepted.

`Card::pickMode` had the same class of bug — encoded mode index `m` as
`chosen_objects = {m+1}`, which the encoder turned into the slot for the
card whose def_id was `m+1` (e.g. mode 0 → def_id 1 = Bounty Hunter,
mode 1 → def_id 2 = MF Captain). The policy head was learning a smeared
distribution over two unrelated decisions.

`Card::pickXAmount` had the same encoding shape and the same problem
for variable-X spells (Bullet Time, Hextech Anomaly).

**The fix.** Use `Intent::chosen_value` (already an `optional<int>` on
`Intent`) as the authoritative answer field for all int-coded MakeChoice
helpers. Reserve a new range of `kNumIntChoices = 16` slots at the top
of MakeChoice's arity for these answers. Encoder priority: if
`chosen_value.has_value()` → int-coded slot; else `chosen_objects` →
card-keyed slot; else slot 0. Card-keyed MakeChoice (discard pick,
target pick, predict pick) is unchanged.

**Files touched.**
- `src/openspiel/action_vocab.h` — added `kNumIntChoices = 16`. MakeChoice
  arity is now `kNumCardDefIds + 1 + kNumIntChoices`. Slot layout
  documented inline.
- `src/openspiel/action_vocab.cpp` — encoder prefers `chosen_value` when
  set, otherwise falls back to legacy card-keyed encoding.
- `src/cards/card.cpp` — `confirmOptional`, `pickMode`, `pickXAmount`
  all set `chosen_value` on the published options and read it back via
  `picked->chosen_value`. Removed dead `kYesSentinel`.
- `src/io/state_renderer.cpp` — MakeChoice render branch shows
  `Choose: =<n>` for int-coded picks (previously rendered as "Choose:
  ?" or the bogus card-by-fake-defId name).
- `tests/test_action_vocab.cpp` — three new regression tests:
  `ConfirmOptionalYesAndNoSlotsAreDistinct`,
  `PickModeIndicesEncodeToDistinctSlots`,
  `IntCodedMakeChoiceDisjointFromCardKeyedMakeChoice`. These pin the
  encoding so a re-introduction of the sentinel pattern fails CI.
- `tests/cards/test_spells_misc.cpp`, `tests/cards/test_jhin_deck.cpp`
  — `pickX` test helpers updated to match by `chosen_value`.

**Verified.**
- All 495 tests pass.
- Jhin mirror across 6 seeds: Virtuoso accepts ~50% of `MAY_PROMPT`s
  (was 0%); `VIRTUOSO: 4 spells banished-with-me` payoff fires in 3 of
  6 seeds (channels 4 runes + draws 1).
- `Banish: <card>` row renders correctly in the replay snapshot
  immediately after a banish (e.g. seed=42 idx=90 shows
  `| Banish: Frigid Touch |` in P2's section).
- 20-game smoke run: 50/50 win split, 14.7 games/sec (no perf
  regression).

**Pilot rollout of `confirmOptional`** — three "you may..." cards
refactored to demonstrate the resumable-yes/no pattern end-to-end on
the now-correct vocab:
- `MOverzealousFan` (449) — `WhenIDefend`, may kill self to bounce
  attacker. Surfaces a pre-existing engine timing bug (see Known
  engine gaps): `combat_designation` is cleared before the chain
  resolves the trigger, so `still_legal` always fails. Now traces
  `MAY_NOT_OFFERED` instead of silently no-op'ing.
- `MGrimApothecary` (583) — `WhenYouPlayMe`, may bounce a friendly
  unit at a battlefield. Works end-to-end: 5 `MAY_PROMPT`s, 1
  `MAY_ACCEPTED` across 20 random games (rengar vs khazix).
- `MIrresistibleFaefolk` (674) — `WhenIMoveToFB`, may pull an enemy
  unit to my battlefield. The generated stub had the wrong effect
  (`moveToBase` on the target); the manual override fixes it. Added
  `EffectExecutor::moveToBattlefield(target, bf)` as a reusable helper
  — first effect in the executor that targets a specific BF rather
  than the controller's base.

**Action-vocab quality follow-ups** (training-pipeline polish, not
blockers — flagged here so the next agent picks them up before Phase
C-2 training lands):

- [ ] **`Play` slots collapse target variants.** "Play Frigid Touch @
  Miss Fortune" and "Play Frigid Touch @ Bounty Hunter" both encode to
  the same Play slot (keyed only by card_def_id). The policy head
  can't express a preference between target choices through the action
  head — target choice has to be a follow-up decision the model picks
  via subsequent action(s), or `Play` needs to expand its slot
  dimension. May be intentional (chain-priority follow-up decisions
  already cover targeting), but worth a deliberate decision before
  scaling training.
- [ ] **`AssignCombatDamage` hashes distributions into 4 buckets.**
  `action_vocab.cpp:73-82` does `hash = (hash * 31 + da.damage) & 3`.
  Two genuinely different lethal-allocation patterns can land in the
  same bucket — lossy for the policy. Either widen the bucket count
  or move damage-assignment selection into a separate verb keyed by
  target-unit (similar to how `MakeChoice` now keys some answers).
- [ ] **Multi-game Jhin mirror at MCTS** — quick sanity-check that
  `mcts:sims=N` actually exercises the new int-coded slots (Virtuoso
  `MAY_ACCEPTED` should now appear in MCTS-driven games too, not just
  random rollouts). Same `--render-html` + `grep MAY_ACCEPTED`
  procedure that verified random.

### Completed (✅) — Phase 5h: One-pass training-pipeline polish (2026-05-17)

Four-item follow-up pass after Phase 5g. All four addressed in one
sitting; verified end-to-end including a live `riftbound_train`
iteration.

**1. Overzealous Fan `WhenIDefend` combat-timing fix.**
`TriggerManager::onCombatStarted` (line 224 area) now captures the
current attacker GameObjectId into the defender's
`card_counters["__defend_attacker_id"]` BEFORE adding the trigger to
the chain. Necessary because `combat_designation` is cleared between
combat staging (when the trigger fires) and chain resolution (when the
ability runs). `MOverzealousFan::onTrigger` reads from card_counters
instead of scanning the board. Verified in 20 random Draven vs Khazix
games: 3 triggers fired → 2 prompted → 2 accepted → 2 successful
kill+bounce. Previously: 0 successful executions across many games.

**2. `AssignCombatDamage` vocab arity 4 → 16.**
The old 4-bucket hash collided distinct damage allocation patterns onto
the same OpenSpiel action slot, blurring the training signal for combat
decisions. Bumped to 16 buckets with a proper FNV-1a hash that mixes
`target_unit` id + damage amount per assignment, so distributions that
shuffle damage between Tank / Backline / regular units encode to
distinct slots. The 4 named distribution kinds (greedy-lethal /
spread-even / focus-all / lethal-first) naturally land in slots 0..3;
ad-hoc agent patterns spread across the rest. Files:
`src/openspiel/action_vocab.{h,cpp}`.

**3. Three more `confirmOptional` rollouts.**
- `MGloomist` (785) — `WhenIHold`, "you may exhaust to draw 1".
  Previously auto-fired. Verified ~50% accept rate (14/27) across 10
  Vex vs LeBlanc games.
- `MGreenFather` (753) — `WhenIConquerOrHold`, "you may exhaust to
  replace BF with Brush token". Previously auto-fired.
- `MBlitzcrankImpassive` (67) — `WhenYouPlayMe` (to BF), "you may
  move an enemy unit to here". Was empty stub; added manual override
  using `EffectExecutor::moveToBattlefield` (added in Phase 5g for
  Faefolk). Note: Blitzcrank's second triggered ability ("When I hold,
  return me to my owner's hand") is unmodeled — Card supports only one
  triggerType today (Multi-ability per Card is in Known engine gaps).
  Not yet in any test deck so not exercised by random smoke runs.

**4. Phase C-2 scaffold sync.**
Discovered Phase C-2 is more advanced than the docs suggested — model,
trainer, replay buffer, self-play, and `riftbound_train` are all
already in tree (`src/ml/`, `src/training/`). My Phase 5g vocab bump
(+28 slots: +16 MakeChoice for int-coded answers, +12
AssignCombatDamage) had stale references in two places:
- `src/ml/model.h` — `ModelConfig::num_actions` default was hardcoded
  to the old 7113. Bumped to 7141 (the new `kVocabSize`).
- `tests/test_model_forward.cpp` — `EXPECT_EQ(cfg.num_actions, 7113)`
  bumped to 7141 with a comment marking it as a tripwire when the
  vocab changes.
- `src/training/riftbound_train.cpp` — now sets
  `model_cfg.num_actions = ::riftbound::openspiel::kVocabSize`
  explicitly, so future vocab bumps are picked up by freshly-trained
  checkpoints without a `model.h` edit. Existing checkpoints loaded
  via `--resume` must match shape; `torch::load` fails loudly on
  mismatch.

Verified end-to-end:
```bash
cmake --build build-torch --target riftbound_train
RIFTBOUND_ROOT=. ./build-torch/riftbound_train \
    --iterations 1 --games-per-iter 2 --mcts-sims 5 --train-steps 5 \
    --deck1 decks/jhin_test.json --deck2 decks/jhin_test.json \
    --checkpoint-dir /tmp/rb-train-smoke
# → 2 self-play games, 724 tuples, policy loss 0.66, value loss 1.14,
#   checkpoint saved. 33s wallclock.
```

**Test count.** All 495 tests pass (debug build), 498 with LibTorch
enabled (adds the 3 ModelForward tests).

**What still blocks production training:**
- [ ] Replay-driven hyperparameter sweep across deck pairs to settle
  loss-curve health. Single-iteration smoke is `does it run`; multi-
  iter (10+) tells you whether the policy/value losses actually
  trend down.
- [ ] **`Play` slot target-variant collapsing** (Phase 5g follow-up
  still open). Currently every `Play X @ Target` for the same card
  aliases. The policy can't learn target preferences through the
  action head — target selection has to be a follow-up
  `MakeChoice`-style decision the model picks separately, OR `Play`
  arity needs to widen significantly. The cleanest fix is decoupling
  target selection into a follow-up choice; substantial action-
  generator surgery and worth a dedicated change.
- [ ] **Per-archetype league + Elo** (Phase 11 future work, unchanged).

### Completed (✅) — Phase 6b + 6c: ISMCTS wiring + resumable clone (2026-05-17)

**Phase 6b — ISMCTS agent kind.**
Added `ismcts:sims=N` to `riftbound_openspiel`'s agent spec parser
alongside `random`, `mcts`, and `model`. Wires
`open_spiel::algorithms::ISMCTSBot` through the Decider, sharing the
same `RandomRolloutEvaluator` + `CountingEvaluator` pipeline as MCTS.

Required engine surface additions to make ISMCTSBot work:
- `RiftboundState::ObservationString(player)` — FNV-1a hash over the
  hidden-info-masked observation tensor. ISMCTSBot uses it as a
  hash-map key to identify info-set nodes in its search tree.
- `provides_observation_string=true` in `GameType` declaration.
- `use_observation_string=true` passed to ISMCTSBot constructor (the
  default would use `InformationStateString`, which we don't implement).
- `SetResampler(...)` installed with a Clone()-only resampler (no
  actual hidden-info determinization yet — queued as a follow-up).
  Without a resampler, the bot's `SampleRootState` would throw at
  search time.

Verified end-to-end: `riftbound_openspiel --agent1 ismcts:sims=10
--agent2 random --games 3 --seed 42 --deck1 decks/jhin_test.json
--deck2 decks/jhin_test.json` runs to completion. P1 (ismcts) beat
P2 (random) 2-1, 60-second total wallclock.

**Phase 6c — Resumable Clone via `GameEngine::resumeFromSnapshot`.**

The bottleneck for tree-search throughput was `RiftboundState::Clone()
+ first ApplyAction`. Microbench at decision 100:
- Before: Clone=18 µs, ApplyAction=**4638 µs** (5ms replay path).
- After:  Clone=18 µs, ApplyAction=**140 µs** at clean boundaries
  (33x speedup), 5ms at in-flight boundaries (replay fallback).

**Mechanism.** Added `GameEngine::resumeFromSnapshot(GameState, seed)`:
- Substitutes the snapshotted GameState into a fresh engine.
- Initialises ChainManager / EffectExecutor / TriggerManager pointing
  at the substituted state on a fresh EventBus.
- Spawns the worker thread which dispatches to `runTurnFromPhase`
  (new) — a fall-through switch that picks up at `state.turn.phase`
  and runs the remainder of the turn, then re-enters `runTurnLoop`
  for subsequent turns.

`RiftboundState::ensureLive` now prefers `resumeFromSnapshot` over
the legacy replay path when the snapshot is at a **clean decision
boundary**:
- `turn.phase` not in {Setup, Mulligan}
- `turn.oc_state == Open` (not mid-chain priority pass)
- `turn.ns_state == Neutral` (not mid-showdown)
- `!chain.exists()` (no chain in flight)
- `!cost_cursor.has_value()` (no mid-cost-payment)
- No BF has `combat_in_progress` / `showdown_in_progress` /
  `combat_staged` / `showdown_staged` set

In-flight snapshots fall back to the legacy `beginGame` + replay
action_history path (slow but verified correct).

**Why the gate is conservative.** Mid-chain/showdown/combat snapshots
have side effects already captured in `state_` that running the
phase function again would re-trigger. The conservative gate is the
correctness floor; full speedup at every clone requires extracting
entry side-effects from `runShowdownLoop`, `runCombat`, `processFEPR`,
and `payCardCost` — each one its own surgical change. See follow-ups
below.

**Engine refactor for clean-boundary MainPhase resume.**
- `mainPhase()` split into entry (`mainPhase()`) + loop body
  (`mainPhaseLoop()`). The entry resets `ns_state`/`oc_state`/
  `priority_holder` and emits `PhaseChangedEvent` — re-running it on
  resume would clobber the snapshot's priority holder and re-fire
  phase-change triggers. `runTurnFromPhase` calls `mainPhaseLoop()`
  directly when resuming AT MainPhase.

**Verified.**
- Clone-equivalence test: 10/10 games pass (was 0/10 with naive
  resume-everywhere). Per-game: 50 walked, ~200 post-clone steps,
  reaches terminal, returns match.
- 516/516 unit tests pass.
- ISMCTS 3-game smoke: ~20s/game (was 49s with replay-only), 2.4x
  end-to-end throughput improvement on Jhin mirror.

**Full memcpy speedup follow-ups** (queued, not landed):
- [ ] **Extract `showdownLoop`/`combatLoop`/`processFEPRLoop` from
  their entry-functions** so in-flight snapshots can resume via the
  loop body without re-running entry side effects. Each one needs the
  same `mainPhase`/`mainPhaseLoop` split treatment.
- [ ] **Resume mid-cost-payment** via the existing `cost_cursor`
  (already engine-stored). The payCardCost path needs an entry point
  that picks up from `cost_cursor` instead of starting fresh.
- [ ] **Get GameState memcpy from 8µs to 1µs** — requires POD-ifying
  `GameObject` (replace `std::string`, `std::vector`, `std::unordered_map`
  fields with fixed-size arrays/POD) or implementing a pool allocator
  shared across GameObjects. Touches every card implementation;
  multi-day refactor.

**ISMCTS quality follow-up** (queued, not landed):
- [ ] **Proper hidden-info resampling.** Today's Clone()-only
  resampler doesn't actually determinize — it returns the same state
  every sample, so ISMCTSBot behaves like standard MCTS over the
  perspective player's view. The real determinization should: read
  `PlayerState::observed_cards` (already maintained as a memory bank),
  shuffle opp's hand and deck from the unseen pool, randomize
  facedown card identities at BFs. Estimate: 1-2 days.

### Completed (✅) — Phase 6d–6i: V2 model + per-legend training pipeline (2026-05-17)

End-to-end **rip-and-replace** of the V1 residual-MLP training stack
with the V2 entity-token / transformer / spatial-fusion / pointer-head
architecture spec'd in `docs/potential-model-architecture.txt`. The
V1 `RiftboundModel` (`src/ml/model.{h,cpp}`) is **deleted**; V2
(`src/ml/v2_model.{h,cpp}`) is the only model class in tree.

**Phase 6d — Entity-token extractor (C++)**
- `src/ml/entity_tokens.{h,cpp}` (`riftbound::ml::extractEntityTokens(state,
  perspective, card_db) → EntityTokens`).
- SoA layout: per-token `card_def_id / zone_id / domain_id / stance_id /
  is_perspective / chain_index / spatial_node / valid_mask` (int32) +
  `stats` (float32, kEntityStatsDim=5). Padded to `kMaxEntityTokens=256`.
- 20-value `EntityZone` enum (Self/Opp × Hand/MainDeck/RuneDeck/Trash/
  Banish/Base/Champion/Legend + Battlefield + Facedown + ChainItem).
- 8-node spatial grid: SelfBase=0, OppBase=1, BF[0..5] → nodes 2..7;
  off-board = -1.
- Information-set masking: opp hand + main_deck identities emitted as
  `card_def_id=0`; counts + zones still visible. Public zones
  (trash/banish/on-board) keep identities. Facedown at BFs masked
  unless perspective owns it.
- Bounds-safe CardDB lookup via `safe_cost` / `safe_first_domain`
  lambdas — synthetic test ids that aren't in the registry return
  zeros instead of throwing.
- 13 tests in `tests/test_entity_tokens.cpp`.

**Phase 6e — V2 model (transformer + spatial fusion + pointer heads)**
- `src/ml/v2_model.{h,cpp}`. Inputs: 9 batched tensors (card_ids,
  zone_ids, domain_ids, stance_ids, stats, is_perspective, chain_index,
  spatial_node, token_mask). Outputs: `Output` struct with
  `action_type_logits` / `source_logits` / `target_logits` /
  `dest_node_logits` / `flat_policy_logits` / `value`.
- Architecture:
  1. Per-token embeddings: card (256d), zone (64d), domain (64d),
     stance (64d), perspective (8d) + stats Linear projection (5→64d) +
     spatial-node embedding (32d, with index 0 reserved for off-board).
  2. Token assembly: concat → LayerNorm → Linear → `d_model` (512).
  3. Sinusoidal positional encoding for chain items
     (`buildChainPositionalEncoding`) added only when
     `chain_index >= 0`.
  4. TransformerEncoder (6L × 8H × d=512, FFN=2048, dropout=0.1) over
     valid tokens with `key_padding_mask`.
  5. Spatial fusion: mean-pool tokens by `spatial_node` →
     (B, num_spatial, d_model); run `spatial_attn_layers` (default 2)
     of pre-LN MultiheadAttention + post-LN FFN. **Edge biases**
     replace a formal GNN per the engineering-review note —
     `edge_bias_` parameter of shape (num_edge_types=3, num_spatial=8,
     num_spatial=8) sums across edge types into a (8,8) attn_mask
     added to attention logits. Mathematically equivalent over the
     fixed grid; saves a library dep.
  6. Pointer heads: pooled encoder state (masked mean) → per-head
     query projections; dot-product against encoder tokens (for
     source/target, masked to -1e9 in padded slots) and against
     `dest_node_key_proj(spatial_features)` (for dest_node).
  7. Value head: pooled → Linear(d_model→128) → ReLU → Linear(128→1)
     → tanh.
- `flat_policy_logits` head (Linear(d_model → num_action_slots)) added
  alongside the pointer heads as the **AlphaZero training target until
  the pointer-head decomposition + aux losses land** (deferred). The
  trunk still trains end-to-end via this head's gradient.
- 4 forward-shape tests in `tests/test_v2_model_forward.cpp`. Smoke
  config: d_model=64, encoder_layers=2, encoder_heads=4 (~250K params).
  Default config: d_model=512, encoder_layers=6 (~30M params).

**Phase 6f — Replay buffer migration to entity tokens**
- `ReplaySample` rewritten: holds `ml::EntityTokens tokens` instead of
  a flat 4623-dim observation vector. Per-tuple memory ~14 KB.
- `self_play.cpp` calls `extractEntityTokens(rb_state.engineState(),
  pid, rb_game.cardDb())` for the acting player at each decision and
  stuffs the result into the pending tuple.
- `trainer.cpp` stacks N tokens into batched V2 input tensors via
  `stackBatch` (per-field accessor-based copy), forwards through
  `V2Model`, masks illegal logits, and supervises `flat_policy_logits`
  + `value` against MCTS visits + game outcome.
- `LibTorchEvaluator` rewritten to forward V2: extracts entity tokens
  on each `Evaluate`/`Prior` call via dynamic_cast to RiftboundState +
  RiftboundGame, packs into single-batch tensors, returns
  `flat_policy_logits` for `Prior` and `value` for `Evaluate`.
- V1 files deleted: `src/ml/model.{h,cpp}` and
  `tests/test_model_forward.cpp`. Build references removed.

**Phase 6g — Per-legend driver via riftbound_runner**
- `src/training/config_driver.{h,cpp}` adapts the parsed
  `config::TrainingConfig` to the V2 self-play + trainer loop.
- Per-game deck-pair sampling: with probability `mirror_ratio`, both
  decks drawn from `deck_pools.self`; otherwise self vs opp. Falls
  back to mirror if opponent pool is empty.
- Per-legend invariant enforced upstream by `parseConfig` (all decks
  in `deck_pools.self` must declare the same legend as
  `training.legend`); driver trusts and doesn't re-validate.
- `parseDevice` accepts `cpu` / `cuda` / `cuda:N`. Throws cleanly
  when CUDA requested in a CPU-only build.
- Checkpoint pruning: keeps the most-recent `keep_last_n` `.pt` files
  in `checkpoint_dir`. `keep_last_n=0` → keep all.
- `riftbound_runner` now dispatches `mode=train` into
  `runConfigTraining(cfg)` (gated on `RIFTBOUND_HAVE_LIBTORCH`); the
  legacy `riftbound_train` binary still exists for single-deck CLI
  smokes.

**Phase 6i — End-to-end smoke**
Sample config `configs/train_gloomist_smoke.json` (2 iters, 2 games,
2 train steps, batch=4, d_model=128). 23s wallclock, both iters
self-play + train + checkpoint cleanly. Loss values printed per iter,
checkpoints land under `checkpoints/gloomist_smoke/iter_{0,1}.pt`.

A 5-iter loss-trend config (`train_gloomist_loss_trend.json`,
games=4, train_steps=20, batch=16, d_model=128) was added for the
next agent to extend into a real 50+ iter run.

**What's deferred (logged here so future agents don't re-discover):**
- **Pointer-head supervision.** `action_type_logits` /
  `source_logits` / `target_logits` / `dest_node_logits` are
  COMPUTED on every forward pass — their parameters get gradient
  flow via the shared trunk — but they aren't supervised against a
  decomposed action target. To land:
  1. Build a `ml::action_decomposer` that takes an OpenSpiel
     `Action` id (`riftbound::openspiel::kVocabSize` space) +
     EntityTokens metadata and returns `(action_type, source_idx,
     target_idx, dest_node)` or "irrelevant for this head".
  2. Add a token_idx → game_object_id field to `EntityTokens` so
     `source_logits` / `target_logits` can be matched against the
     intent's source/target object ids.
  3. Add separate cross-entropy losses for each pointer head,
     weighted into the total loss alongside the flat-policy CE +
     value MSE.
  4. At inference time, combine pointer-head probabilities into the
     flat-policy distribution (Bayes-like product over decomposed
     dimensions) and use that instead of `flat_policy_logits`.
- **Aux heads** (auxiliary objectives — turn-phase prediction, who-
  scores-next, etc.). Out of scope until the pointer heads supervise
  correctly.
- **Multi-GPU self-play.** `config.training.devices` is parsed
  (`["cuda:0", "cuda:1"]`) but `runConfigTraining` only honors
  `train_device` for the optimizer; self-play is sequential
  single-device today. Wire a per-device worker pool against the
  parsed list when scaling matters.
- **Temperature-aware visit sampling.** Trainer samples MCTS
  visit-counts as a soft target; the per-decision argmax action is
  played (no temperature drop at decision N). For real AlphaZero
  training, honor `cfg.training.mcts.temperature` +
  `temperature_drop_at_decision`.
- **Eval / promote.** `eval` block is parsed but unused. Wire a
  baseline-vs-current playoff under `eval.every_iter` and gate
  checkpoint promotion on the configured `promote_threshold`.

### Completed (✅) — Phase 6r: Multi-ability per Card foundation + activated-ability target collision fix groundwork (2026-05-19)

Two paired engine refactors. The first lifts Card from "one activated
ability per card" to "N abilities per card" with full back-compat for
the 51 existing single-ability cards. The second introduces the
`needs_activation_time_target` per-ability flag — same shape as the
Phase 6q `needsPlayTimeTarget` fix for Spell plays — so activated
abilities with targets can route target selection through `pickTarget`
instead of pre-enumerating per-target Intents that collide on the same
vocab slot.

**1. `ActivatedAbility` struct** (`src/effects/effect_types.h`). Holds
`cost` (ActivationCost), `targets` (TargetRequirements), `is_action`,
`is_reaction`, `needs_activation_time_target`. One descriptor per
distinct activated ability on a card.

**2. `Card::activatedAbilities() → vector<ActivatedAbility>`**
(`src/cards/card.h:101-115`, `src/cards/card.cpp:556-572`). Default impl
wraps the legacy `hasActivatedAbility() / getActivationCost() /
getTargetRequirements() / isActionAbility() / isReactionAbility()`
methods into a one-element vector when `hasActivatedAbility()` is true.
**Single-ability cards keep working without any changes.** Multi-ability
cards override `activatedAbilities()` directly and return N descriptors.

**3. `Card::onActivate(ctx, ability_index, targets)`**
(`src/cards/card.h:64-74`). New indexed overload. Default impl forwards
to the legacy `onActivate(ctx, targets)`. Multi-ability cards override
the indexed version and dispatch on the index.

**4. `Card::enumerateLegalTargets(state, controller, ability_index)`**
(`src/cards/card.h:262-269`). Indexed overload for multi-ability target
enumeration. Default falls through to the legacy single-arg version.

**5. `Intent::ability_index`** (`src/core/intent.h:36-39`) — carries
which ability of `ability_source`'s `activatedAbilities()` is being
invoked. Also added to `operator==` for replay correctness.

**6. `ChainItem::ability_index`** (`src/core/game_state.h:259-262`) —
threaded through `ChainManager::addAbility(... int ability_index = 0)`
from the engine's ActivateAbility intent handler. `stepResolve` reads
it and calls `card->onActivate(ctx, item.ability_index, targets)`.

**7. `action_vocab`** (`src/openspiel/action_vocab.h:59-66`, `.cpp:116-132`).
`ActivateAbility` arity grew from `kNumCardDefIds` (787) to
`kNumCardDefIds * kMaxAbilitiesPerCard` (787 × 4 = 3148). Slot encoding:
`verbBase + (def_id - 1) * 4 + ability_index`. Single-ability cards
default to `ability_index=0`, so their slot is identical to the pre-6r
layout — back-compat for any pre-trained policy weights at the bit
level. **Vocab grew by 2361 slots.** `kVocabSize` is read at runtime by
`config_driver.cpp` so the trainer picks up the change automatically.

**8. Engine surgery sites** (`src/engine/game_engine.cpp`):
- `generateActivateAbilityActions` (line 2402, main-phase action gen):
  loops over `activatedAbilities()` and emits one Intent group per
  ability with `ability_index` set.
- Showdown action gen (line 2010, ActivateActionAbility): same shape.
- Closed-state action gen (line 2202, ActivateReactionAbility): same.
- ActivateAbility intent execution (line 1043): reads
  `activatedAbilities()[ability_index].cost` instead of the legacy
  `getActivationCost()`.
- `chain_manager_->addAbility` call site (line 1089): passes
  `intent.ability_index`.
- ChainItem resolution dispatch (line 1349): passes
  `item.ability_index` to `onActivate`.

**9. `needs_activation_time_target` flag**. When a per-ability
descriptor has this set to true, the action generators emit ONE
intent with empty targets regardless of `targets.count`. The card's
`onActivate` uses `pickTarget` at resolve time. Mirrors the Phase 6q
`needsPlayTimeTarget` mechanism but for activated abilities.

**10. Bullet Time backward-compat shim** (`MBulletTime::onResolve`).
Tests pre-supply targets directly; if `targets` is non-empty, use
`targets[0]`. Otherwise call `pickTarget`. Production (action gen with
`needsPlayTimeTarget=true`) always emits empty targets, so always
goes through `pickTarget`. Test-only legacy path; no production
impact.

**Verified.** 533/533 tests pass (full suite). Build clean. No
behavioral regression for the 51 single-ability cards because the
default `activatedAbilities()` impl produces semantically-identical
behavior to the legacy single-ability methods.

**What's NOT done yet** (queued — see "Remaining Work" / "Other engine
gaps" for full list):
- [ ] Migrate the 3 actually-multi-ability cards (Voidreaver 787,
  Grandmaster at Arms 554, Honeyfruit 611) to override
  `activatedAbilities()` and return both abilities.
- [ ] Migrate the 5 single-target activated abilities to use the
  `needs_activation_time_target` + `pickTarget` flow: Bounty Hunter
  (262), Heart of Dark Ice (375), Shadow (752), Voidreaver (787),
  Blood Rose (465).
- [ ] `EffectExecutor::moveToBase(unit)` helper — prerequisite for
  Voidreaver's second ability.
- [ ] Engine surface for the remaining "do-nothing" cards (UnitStunnedEvent
  + WhenYouStun for Vex Mocking; cost-aura combat-scope for Vex
  Cheerless; play-source tracking for Rek'Sai; Deathknell-double-fire
  for Karthus; AtStartOfMain delayed trigger for Iascylla).

### Completed (✅) — Phase 6q: Validation-gated promotion + MCTS bootstrap + target decoupling (2026-05-18)

Three orthogonal training-pipeline fixes landed together after the
Phase 6m/n/o run produced models that lost 35% vs random (Rengar) /
55% vs random (MF, needed 60). Diagnosis: nothing during training
gave a real signal of model strength (mirror self-play is structurally
50/50 regardless of strength), so confidently-wrong checkpoints
shipped uncontested. Fixes target the three failure modes
independently.

**1. Validation-gated checkpoint promotion** (`config_driver.cpp`).
After each eval block runs, the candidate's P1 win-rate is parsed
from the eval log (`P1 wins: K (XX%)`), compared against
`eval.promote_threshold` (default 0.55). On PROMOTE: update
best_winrate / best_iter / best_path and write a `best.pt` symlink
(or hard-copy fallback) in `checkpoint_dir`. On REJECT: delete the
just-saved checkpoint so downstream tooling can't pick up a
regression. Verdict printed each iter (`promotion: PROMOTE/REJECT
(winrate=X >= threshold=Y)`). When eval fails to parse (e.g. eval
subprocess returns non-zero), verdict is `UNDETERMINED` and the
checkpoint is kept (conservative — don't delete on infrastructure
failure). Best-so-far summary printed on every reject.

**2. MCTS-bootstrap self-play mode** (new `bootstrap_iters` /
`bootstrap_mcts_sims` config knobs; new `SelfPlayConfig::use_model`
flag). For the first `bootstrap_iters` iters, self-play uses
`open_spiel::algorithms::RandomRolloutEvaluator` instead of the V2
model — pure MCTS targets, no model inference. The trainer still
consumes the (entity_tokens, π, z) tuples, so the network learns
to imitate MCTS's visit distribution and rollout value. Skips the
cold-start trap where a random-init model's noisy prior poisons
MCTS, which in turn generates bad training targets, which in turn
confidently-wrong-trains the prior. RandomRolloutEvaluator's
targets are signal even on iter 0. After `bootstrap_iters`, the
loop swaps to model-driven self-play using `self_play_agent.sims`
+ `LibTorchEvaluator`. `bootstrap_mcts_sims` (default 40) controls
the MCTS depth during bootstrap; can be set independently of
post-bootstrap sims since there's no model inference cost to pay.

**3. `Card::pickTarget` + play-time-target decoupling** (`card.h/.cpp`,
`game_engine.cpp:generateSpellActions`, `card.h:needsPlayTimeTarget`
virtual). Phase 5g action-vocab quality follow-up — the `Play`
vocab slot was keyed by `card_def_id` only, so every (card, target)
variant of the same play collapsed to one slot. Policy head
couldn't express "Hidden Blade @ MF Captain" vs "Hidden Blade @
Bounty Hunter" — both encoded to slot Play(Hidden Blade), aliased.
Fix: opt-in `needsPlayTimeTarget()` returns true on a card → action
gen emits ONE Play intent per card (no target loop) → at resolve
time, `onResolve` calls `pickTarget(ctx, label, legal_targets)` →
helper publishes one MakeChoice per legal target keyed by target
card_def_id (Phase 5g MakeChoice slots already give DISTINCT slots
per chosen object). Mirrors `pickMode` shape exactly:
- Reserves resume_points 6/7/8 and `resume_data[2]` (pickXAmount
  uses 0/1/2 + data[0]; pickMode uses 3/4/5 + data[1]; all three
  can coexist in one Card's onResolve).
- Returns `kInvalidId` on suspend (resume_point==7) OR no-legal-
  targets (resume_point==8). Callers distinguish via
  `ctx.state.chain.resuming->resume_point` — needed when the card
  has partial-fizzle riders (Shadow's Call draws 2 even when the
  targeted part fizzles; pin'd by `ShadowsCallTest.
  DrawsTwoEvenWithoutTarget`).
- Trace tags: `TGT_PROMPT`, `TGT_PICKED`, `TGT_FORCED`,
  `TGT_NOT_OFFERED`.

Card migrations (proof of concept — 3 representative cards):
- `MGrimResolve` (657) — single friendly-unit target, no rider.
- `MShadowsCall` (727) — single friendly-unit target, draw-2 rider
  (demonstrates the resume_point==7 suspend check).
- `MHiddenBlade` (213) — any-side single unit target (most
  interesting policy-head signal — model can learn enemy vs
  friendly target preference).

Pattern documented at `Card::needsPlayTimeTarget()` in `card.h`.
Remaining cards (~50–100 with on-play targeting) can be migrated
incrementally — each conversion is mechanical: override
`needsPlayTimeTarget` → true, rewrite onResolve to use pickTarget +
re-enumerated legal targets. Cards that don't opt in keep working
via the original per-target-Play-intent path (`needsPlayTimeTarget`
default is false).

**Verified.** 530/530 tests pass (debug build), including 3
migration regression tests (`ShadowsCallTest.{Gives*, DrawsTwo*}`
and the Hidden Blade kill+draw test). End-to-end 4-iter smoke
training run with bootstrap_iters=2, eval.every_iter=2,
promote_threshold=0.55 (TODO — confirm runs).

**Remaining migration work:**
- [ ] Decide a migration policy: opt-in-all-targeted-spells (sweep
  the codebase, override `needsPlayTimeTarget` on every Card with
  non-empty `getTargetRequirements()`) vs let-it-grow (migrate as
  cards get touched). Opt-in-all gives the full vocab benefit; let-
  it-grow trades some training signal for less code churn.
- [ ] Dual-target cards (MDeathgrip, MStarCrossed, MChallenge) need
  a `pickTarget` variant that returns TWO objects with constraints
  on each (one friendly, one enemy). Could mirror pickMode's
  pattern with `chosen_objects = {friendly, enemy}` paired
  intents. Or call pickTarget twice with different filters.
  Punted for now — these cards keep the per-pair-intent path.
- [ ] `generateActivateAbilityActions` has the same target-collapse
  problem for activated abilities with targets. Same fix shape —
  `Card::onActivate` calls `pickTarget`. Punted; lower priority
  because there are fewer activated abilities with targets.

### Historical: Python ML Pipeline (Phases 6b – 10) — REMOVED

The repo previously contained a custom Python training pipeline with the following surface:
- `scripts/train_agent.py` — REINFORCE / supervised trainer (PyTorch + ONNX export)
- `scripts/parity_check.py` — byte-for-byte C++/Python feature-extractor parity verification
- `scripts/self_play_loop.sh` / `self_play_loop_rengar.sh` — overnight self-play loops with gated promotion
- `src/agents/model_agent.{h,cpp}` — ONNX inference agent
- `src/io/binary_data_serializer.{h,cpp}` — pre-extracted feature dump for Python ingestion
- `src/io/data_serializer.{h,cpp}` — JSONL training-data dump
- `src/ml/v2_entity_tokens.{h,cpp}` — experimental entity-token features
- ONNX Runtime CMake dependency

**Removed in commit `752e54f`** (~5,200 LOC deleted) to consolidate everything in C++ ahead of switching to OpenSpiel-driven training. The previous pipeline produced one ML model (Rengar v002) that beat its supervised baseline (~86% in mirror) but never beat random reliably; per-iter REINFORCE plateaued and the Python/C++ parity surface was a constant maintenance tax.

**What survives from the old ML stack:**
- `src/ml/feature_extractor.{h,cpp}` — kept because `RiftboundState::ObservationTensor` (the OpenSpiel info-set view) uses it. Future C++ AlphaZero training will read the same layout.
- `tests/test_observation_tensor.cpp` — verifies hidden-info masking via ObservationTensor.

**Future training direction:** OpenSpiel C++ AlphaZero + LibTorch. Phase C-2 scaffold complete as of Phase 5h (2026-05-17) — `src/ml/`, `src/training/`, `riftbound_train` all in tree and verified end-to-end at the 1-iter smoke scale. See "Future Work" → "Phase C-2 — Training" for what's still needed before a production run.

The git history before commit `752e54f` has the full Python-pipeline lessons-learned if needed for reference (architecture rebuild details, REINFORCE gradient signal validation, card-embedding implementation notes, self-play loop bash patterns).
### Phase 11 — OpenSpiel Port

Strategic destination for ML training. OpenSpiel gives us imperfect-info-correct training (NFSP), MCTS at inference, AlphaZero-style policy distillation, and proper chance-node-correct game tree expansion. Future C++ AlphaZero training lands inside OpenSpiel via `open_spiel/algorithms/alpha_zero_torch/` + LibTorch.

**Phase A — Skeleton port** ✅ COMPLETE 2026-05-14. RandomAgent only, no ML integration.
Files: `src/openspiel/riftbound_{game,state}.{h,cpp}` (Game + State subclasses, `REGISTER_SPIEL_GAME` registration). 1000-game statistical parity vs BatchRunner verified (decision counts match exactly; win distributions within noise at N=1000; 88.9 games/sec).

**Phase B — usable for non-random algorithms** ✅ COMPLETE.

Key components:
- **Dense action vocabulary** (`src/openspiel/action_vocab.{h,cpp}`). Intent → dense slot in `[0, kVocabSize)` keyed by `CardDefId` (stable across clones). Verb-bucket layout; semantic variants (PlayCard / PlayReaction / PlayActionCard etc.) intentionally collapse to one slot — AlphaZero policy heads handle aliased slots gracefully. Earlier draft used 52-bit structural bit-packing; replaced because policy heads need a small fixed-size softmax target. Tests: `tests/test_action_vocab.cpp` (20 tests).
- **Information-set `ObservationTensor(player)`**. Delegates to `ml::extractStateFeatures` with hidden-info masking. `GameType::information = kImperfectInformation`, shape `{kStateFeatureDim}` (4623 dims). Tests: `tests/test_observation_tensor.cpp` (4 tests).
- **`CardRevealedEvent`** declared in `src/core/events.h` with `EventBus` signal. Dispatcher in place but no card emits yet, and `extractStateFeatures` doesn't read an `observed_cards` field — see Future Work below.
- **Unified match runner** (`riftbound_openspiel` target). `--agent1`/`--agent2` take `random` | `mcts:sims=N`. Drives `open_spiel::MCTSBot` (RandomRolloutEvaluator). Pulls in OpenSpiel's `algorithms` + `game_transforms` + `utils` OBJECT libraries via `$<TARGET_OBJECTS:..>`.
- **`ChanceMode::kSampledStochastic`** — RNG lives inside the engine; no chance nodes exposed. CFR / theoretically-correct imperfect-info algorithms need explicit chance nodes; see Future Work for the Phase B-2 chance-node extraction.

**Phase C-1 — Engine step-machine refactor** ✅ COMPLETE 2026-05-16. All 6 DoD criteria met.

Goal: drop `RiftboundState::Clone()` from ~5 ms (replay-based) to ≤10 μs so MCTS / AlphaZero work at scale. Approach: pull-driven state machine via `GameEngine::{beginGame, currentStep, applyChoice, stepResult}` + `StepDriver` worker (in `src/engine/step_driver.{h,cpp}`). All engine subroutines split into `advance*` / `resolve*Decision` pairs (main phase, showdown, combat damage, mulligan, cleanup, payCardCost cursor).

**Clone path landed** (commit `625e328`, 2026-05-15): **lazy-thread Clone**. Deep-copies `GameState` + step snapshot into a fresh `RiftboundState`, defers spawning engine + worker thread until a clone genuinely advances. Measured **8.2 μs median / 8.6 μs p95** at decision 100 — meets the <10 μs DoD target. The originally-planned full memcpy Clone (commit 9) is deferred indefinitely as engineering polish; lazy-thread is sufficient.

**Phase C-1 DoD scorecard:**

| # | Criterion                                                       | Status |
|---|-----------------------------------------------------------------|--------|
| 1 | `Clone()` < 10 μs on mid-game state                             | ✅ 8.2 μs median (`riftbound_clone_microbench`) |
| 2 | No threading in OpenSpiel wrapper                               | ✅ relocated into engine via `StepDriver` |
| 3 | 100-game MCTS match (sims=5), MCTS measurably stronger          | ✅ 2026-05-16: **MCTS 71% / Random 13% / Draws 16%** (decisive win rate 84.5%, 100 games × sims=5 × 8 threads, seed=42, miss_fortune mirror, 541s = ~9 min wallclock) |
| 4 | All existing unit tests still pass                              | ✅ 313/313 |
| 5 | Clone-equivalence test passes                                   | ✅ 10/10 at K=50 (`riftbound_clone_equiv_test`) |
| 6 | CLAUDE.md Phase 11 updated                                      | ✅ this section |

**Resume pattern for in-flight chain resolution**:
- `ChainItem::resume_point` (int) + `resume_data` (vec<int32_t>) for saved progress.
- `ChainState::resuming` (optional<ChainItem>) holds the resolving item across iterations.
- `EffectExecutor::{requestChoice, recordChoice, takeChoice, applyDiscard}` — publish/consume choice slot.
- `ChainManager::stepResolve` pops to `resuming`, calls `resolve_spell` in a `while(true)`, breaks on no pending choice. Counter spells unaffected (peek `items.back()`, not `resuming`).
- 17 cards refactored (4 pilots: `MLunarBoon`, `MSabotage`, `MMindsplitter`, `MAbandon`; 13 follow-up cards in `deck_cards.cpp` covering all live `discardCards` sites from generated stubs via a shared `discardThenAct` helper).
- Tests: `tests/test_resume_resolution.cpp`.
- Remaining: equipped triggers (e.g. Doran's Ring) — `TriggerManager::fireEquippedTriggers` bypasses `chain.resuming` so the resume pattern doesn't reach those sites. Reroute through the chain to close.

**Build / run invocations:**
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

**Known OpenSpiel build mechanics** (preserved for the next agent — gotchas):
- OpenSpiel's CMake reads **environment variables** not cache vars for its build options. Use `set(ENV{OPEN_SPIEL_BUILD_WITH_*} "OFF")` from the parent project.
- OpenSpiel doesn't ship abseil / nlohmann/json as submodules; `install.sh` clones them and `sudo apt-get`s system packages. We clone abseil + json + pybind11_json + DDS ourselves in our parent `CMakeLists.txt`.
- `open_spiel_core` is an OBJECT library; its PUBLIC includes only expose `${open_spiel-src}/open_spiel`, not the parent. Add `target_include_directories(YOUR_TARGET PRIVATE ${open_spiel_SOURCE_DIR})` and replicate the abseil link list from OpenSpiel's directory-level `link_libraries(...)`.


### Completed (✅) — Phase 6s: Engine surface + card backlog clearance (2026-05-19)

Cleared the bulk of the engine-surface and card-implementation backlog
in one session, plus pushed back on a hardcoded-card-id implementation
to land a properly-encapsulated Karthus via the new `Card::applyPassiveAura`
hook.

**Engine surface additions:**

- `UnitStunnedEvent` + `WhenYouStun` trigger type (`src/core/events.h`,
  `src/effects/effect_types.h`). Emitted by new
  `EffectExecutor::stunUnitBy(target, stunner_source)` overload (legacy
  single-arg `stunUnit` forwards). `TriggerManager::onUnitStunned`
  subscribed and dispatches per-controller WhenYouStun matchers,
  capturing the stunned unit id into `card_counters["__stunned_unit_id"]`
  for the trigger's onTrigger to read.

- `AtStartOfMain` trigger type. Fires on `PhaseChangedEvent` to MainPhase
  via `TriggerManager::onPhaseChanged`. Also calls
  `checkDelayedAbilities()` so Iascylla-style delayed effects can use
  this trigger. The trigger surface is live; the multi-trigger refactor
  (one Card → multiple `triggerType()`s) is the remaining gap for
  cards that need both WhenIHold AND AtStartOfMain.

- `Intent::play_source` enum (`src/core/intent.h`) — values `{Hand,
  Trash, Banishment, ChampionZone, Hidden, ChainZone}`. Default Hand.
  Set by callers that play from non-hand zones (Fizz, Thrill of the
  Hunt, hidden reveals). Surfaced via
  `PlayerState::current_play_source` transient field set by
  `executePlayCard` before calling `payCardCost`. Consumed by Rek'Sai
  Breacher's auto-Accelerate path.

- `CostModifier` scopes:
  `combat_active_only` (only consulted when any BF has
  `combat_in_progress`), `affects_friendly_only`, `affects_enemy_only`
  (the latter consulted cross-player so Vex Cheerless's +1 enemy spell
  modifier in HER list applies to opp's `canAfford` / `payCardCost`
  sweep).

- `Card::applyPassiveAura(GameState&, PlayerId)` virtual — generic hook
  called once per on-board Card instance during `recalculateAuras`.
  Cards override to broadcast passive effects. Counters mutated here
  must be reset to default at start of recalcAuras (engine does
  `deathknell_double_count = 0` per player in Step 1a).

- `Card::needsEquipTimeTarget()` virtual — gear opts into deferred
  target selection. Action gen emits one intent per gear; execution
  path passes `kInvalidId` as the unit and the gear's `onEquip` calls
  `pickTarget`. Fixes the equip-action target collision class (~33
  affected gear cards remain to be migrated card-by-card).

- `PlayerState::deathknell_double_count` (`src/core/game_state.h`).
  Reset in `recalculateAuras` Step 1a. Read by
  `TriggerManager::onUnitDied` which enqueues `(1 + count)` chain
  items per Deathknell death.

- `Card::entersReadyOnPlay(state, controller)` state-aware overload.
  Default falls through to the no-arg version. Used by Monch
  ("conditional enter-ready when opponent has stunned unit").

**Card migrations using the new surface:**

- **[262] Bounty Hunter** — activated-target collision migration via
  `ActivatedAbility::needs_activation_time_target=true` + `pickTarget`
  inside `onActivate`.
- **[375] Heart of Dark Ice** — same migration.
- **[752] Shadow** — same migration; also switched to `stunUnitBy` so
  the WhenYouStun trigger sees the correct stunner attribution.
- **[465] Blood Rose** — same migration on the Spend 3 XP, [E]: ready
  activated ability.
- **[787] Voidreaver** — full multi-ability migration. `activatedAbilities()`
  returns BOTH abilities (Spend 1 XP buff; Spend 2 XP recall to base).
  Custom `enumerateLegalTargets(state, controller, ability_index)`
  filters per-ability target requirements. `onActivate(ctx, idx, targets)`
  dispatches on index. Uses existing `EffectExecutor::moveToBase` helper
  for the recall ability.
- **[236] Karthus, Eternal** — passive aura via
  `Card::applyPassiveAura` bumping `deathknell_double_count`. **7 unit
  tests** (`tests/cards/test_karthus_deathknell.cpp`) cover:
  baseline (no Karthus), 1 Karthus, 2 Karthus stacking, opponent's
  Karthus doesn't double mine, Karthus in trash doesn't count,
  sequential (Karthus dies between deaths → second fires only 1x), and
  CR-323.4.3a-grounded simultaneous-death semantics (state snapshotted
  at trigger-queue time, so simultaneous deaths still double-fire even
  if Karthus is processed first in the kill batch).
- **[617] Vex, Mocking** — WhenIDefend approximation that stuns the
  attacker via `card_counters["__defend_attacker_id"]` + `stunUnitBy`.
- **[612] Iascylla** — WhenIHold approximation (immediate optional
  move instead of delayed-to-next-main). Proper version blocked on
  multi-trigger per Card.
- **[467] Vex, Cheerless** — `applyPassiveAura` adds two
  combat_active_only cost modifiers (friendly -1 and enemy +1) every
  recalc.
- **[597] Monch** — overrides both `selfCostReduction` (state-aware)
  and the new state-aware `entersReadyOnPlay`. Both gate on
  `oppHasStunned`.

**Architectural moment** (worth recording for next-agent): the user
pushed back on an initial Karthus implementation that hardcoded
`if (obj.card_def_id == 236) state.player(obj.controller).deathknell_double_count++;`
in `recalculateAuras`. Their critique — "I would have thought
essentially the Karthus card text would function like an aura?" —
prompted the refactor to `Card::applyPassiveAura`. The engine itself
no longer knows that card_def_id 236 is Karthus; it just iterates
on-board cards and lets each one broadcast its passive effect. This
is the correct pattern for ALL future "Karthus-shaped" passive cards
(global counters bumped by on-board presence).

CR cross-reference (per user follow-up — "see if the CRs can make any
advisements"): CR 323.4.3a (Step 2d of cleanup) says Deathknell
triggers queue "MAKING NOTE OF THEIR CURRENT LOCATION, ATTRIBUTES,
AND OTHER INFORMATION RELEVANT" — state snapshotted at trigger-queue
time. Combined with CR 303.2 (game actions never truly simultaneous)
and CR 383.3.d (simultaneous triggers ordered by controller), this is
the rule that makes my implementation CR-correct without any explicit
ordering logic in the engine.

**Test count:** 540/540 passing (533 pre-session + 7 Karthus tests).

**What's NOT done in this session** (deferred):
- Per-card equip migrations (the ~33 gear cards in equip_cards.cpp).
  Framework is live; the migration is mechanical card-by-card.
- Multi-trigger per Card (blocks proper Iascylla, Blast Cone).
- Hard Bargain "unless they pay" branch.
- Reveal-and-choose for Sabotage / Mindsplitter (CardRevealedEvent
  emit sites + agent UI).

### Completed (✅) — Phase 6t: Training-dynamics fixes — escape the EndTurn-collapse attractor (2026-05-20)

The Phase 6r-foundation rengar bootstrap run produced a catastrophic
degenerate model by iter 6: 80% EndTurn picks, 0% positive value
predictions, 0/5 wins vs random. Root-cause diagnosis (sims-sweep +
HTML-replay analysis) confirmed the failure was a tight negative
feedback loop between MCTS prior-bias amplification and value-head
collapse to "predict ~0 everywhere" on mirror-match data. Five
coordinated fixes break the loop:

**1. `ActionVerb::Reserved` at slot 0** (`src/openspiel/action_vocab.h`).
Slot 0 is now an unused reserved verb that `encodeAction` never
emits. EndTurn moved to slot 1. Removes the structural privilege of
any specific action; any "slot-0 positional bias" from init or
regularization falls on the unused Reserved slot which the
legal-action mask never selects. Vocab grew by 1.

**2. `HeuristicValueEvaluator`** (`src/training/heuristic_evaluator.{h,cpp}`).
New OpenSpiel Evaluator subclass. Returns `(score_diff/8.0, -score_diff/8.0)`
clamped to [-1, +1] from the engine's score state. Replaces
`RandomRolloutEvaluator` in `self_play.cpp` when bootstrap mode is
on. Gives MCTS a non-zero value signal during bootstrap — random
rollouts on a balanced mirror match returned ~0 values, leaving MCTS
to fall back entirely on the prior. Prior is uniform-over-legal
(prevents the prior from leaking action-id bias into visit counts).

**3. Freeze value head during bootstrap** (`TrainerConfig::freeze_value_head`).
When true, `value_loss` is computed and reported but detached so the
gradient doesn't propagate. Keeps the value head at random init
through bootstrap so it can't converge to the degenerate "predict
zero everywhere" optimum that mirror-match z values would teach it.
`config_driver` toggles via `trainer.setFreezeValueHead(is_bootstrap)`
per iter.

**4. Entropy regularization on policy loss** (`TrainerConfig::entropy_coef`).
Adds `-entropy_coef * H(model_policy)` to the policy loss. Penalizes
peaked policies, preserves exploration. `config_driver` sets
`entropy_coef = 0.02` during bootstrap, 0.0 afterward. Numerical
care: illegal-slot log_probs are -inf and probs are 0; the naive
`probs * log_probs` produces NaN via 0 * -inf even when guarded by
`torch::where` (where evaluates both branches). Fix uses
`masked_fill` to zero illegal slots BEFORE the multiplication.

**5. MCTS exploration bias** (config: `mcts.uct_c = 4.0`,
`mcts.dirichlet_fraction = 0.5`). Bumped from AlphaZero defaults
(1.4 / 0.25). Higher `uct_c` makes Q-values matter more relative to
the prior in the PUCT formula; higher `dirichlet_fraction` puts more
noise on the root prior during self-play. Both attack the same
prior-bias amplification mechanism from different angles.

Applied to ALL 5 legend configs (gpu_bootstrap_{rengar, miss_fortune,
gloomist, khazix, virtuoso}.json). Universal — not rengar-specific.

**Adjacent vocab cleanups landed in the same window:**

- **`AssignCombatDamage` 16 → 128 buckets.** 16 buckets had ~50%
  collision rate on realistic 2-5 attacker × 2-5 defender × 5-20
  damage distributions. 128 cuts that to <10%. Named distribution
  kinds (greedy-lethal / spread-even / focus-all / lethal-first)
  still land at natural hash points 0..3.
- **Equip-target hash in action_vocab encoder.** When source is a
  Gear AND intent has a target, the encoder hashes
  `target_def_id % kMaxAbilitiesPerCard` into `ability_index` so
  (gear, target-unit-type) pairs get distinct slots. Partial fix
  (4-way differentiation per gear); full fix would require a
  dedicated `EquipUnit` verb or per-gear migration to
  `needsEquipTimeTarget` + `pickTarget` in `onEquip`.

**Smoke verification** (local 1-iter, 2-game CPU smoke):
- Before: avg_dec/game = 135 (degenerate), policy_loss ≈ 0.50
  (collapsed peaky)
- After: avg_dec/game = 371, policy_loss = 0.6361 (HIGHER means
  policy isn't collapsing — that's intentional from entropy reg)

**Initial real-world re-deploy** (fresh vast.ai 4090, 32 workers):
- Iter 0: 32 games in 451s (7.5 min), avg_dec = 349 (vs collapsed
  135), policy_loss = 0.5895 (entropy reg working), value_loss
  reported but frozen.
- Iter 1: 430s, avg_dec = 336, W/L/D 13/18/1 (one cap-hit draw —
  evidence of long, complex games, opposite of the collapse).

**What to expect at iter 20** (bootstrap→model-self-play transition):
- Per-iter wallclock similar (~9-12 min with model inference).
- Value loss starts to descend as value head training unfreezes.
- Policy loss may briefly spike as entropy reg turns off + MCTS
  starts using the model prior.

---

### Completed (✅) — Phase 6u: Vex/Khazix targeted-play migrations (2026-05-20)

Six spells in vex_pre_con / vex_test_deck / khazix_test had Play
target-collision (each (card, target) pair encoded to the same vocab
slot, hiding target preferences from the policy head). Migrated to
the Phase 6q `needsPlayTimeTarget` + `pickTarget` pattern.

| ID | Card | Source | Notes |
|----|------|--------|-------|
| 43  | Charm           | new MCharm in deck_cards.cpp | move enemy unit; was generated stub |
| 58  | Discipline      | new MDiscipline | +2M + draw rider runs even if target fizzles; uses resume_point==7 suspend distinction |
| 172 | Rebuke          | existing MRebuke | bounce a unit at a BF |
| 173 | Ride the Wind   | new MRideTheWind | move + ready friendly; was generated stub |
| 593 | Combat Experience | existing MCombatExperience | +1M / +3M at Level 6 |
| 600 | Skyward Strike  | existing MSkywardStrike | also switched to `stunUnitBy` so the WhenYouStun trigger (Phase 6s) sees correct stunner attribution |

Active deck coverage post-Phase 6u:
- vex_pre_con: all targeted-play spells migrated
- vex_test_deck: Charm/Discipline migrated; Defy is a counter (no target collision)
- khazix_test: Combat Experience / Rebuke / Skyward Strike migrated;
  Challenge/Punch First already done (Phase 6q); Hard Bargain remains
  (counter "unless they pay" branch not modeled, separate concern)

Tests: 540/540 pass.
