# Riftbound Simulation Engine - Design Document

**Purpose:** Build a high-performance C++ rules engine that simulates 1v1 Riftbound games, tracks complete game state at every decision point, and outputs structured data for deep learning training.

**Scope:** Engine only. No deck generation, no deck evaluation, no AI/ML code. Just the simulator, state tracker, and data serializer.

**Target:** C++20 with Boost (serialization, filesystem, uuid, program_options).

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────┐
│                   GameRunner                         │
│  Accepts 2 deck submissions, validates legality,    │
│  runs N games, outputs replay corpus                │
├─────────────────────────────────────────────────────┤
│                   GameEngine                         │
│  Turn loop, phase sequencing, priority/focus,       │
│  chain resolution, cleanup orchestration            │
├──────────┬──────────┬───────────────────────────────┤
│ CardDB   │ RulesEnf │  ActionResolver               │
│ (static) │ (static) │  (per-game)                   │
├──────────┴──────────┴───────────────────────────────┤
│              GameState (mutable snapshot)            │
│  Players, Zones, Board, Chain, Scores, Rune Pools   │
├─────────────────────────────────────────────────────┤
│           StateLogger / DataSerializer               │
│  JSON-lines output per decision point               │
└─────────────────────────────────────────────────────┘
```

### Key Design Decisions

- **Intent/Command Pattern** for all game actions. Every action a player can take is an `Intent` object that is validated, then executed. This gives us: (a) a clean enumeration of legal actions at any point, (b) replay capability, (c) undo capability for illegal state detection.
- **Card effects modeled as Effect objects** — each card has a vector of `Effect` descriptors parsed from its ability text at load time. Effects are not hardcoded per-card; instead they compose from a vocabulary of ~40 atomic operations.
- **Immutable card definitions, mutable game objects** — `CardDef` is static data from the CSV; `GameObject` is its runtime representation with temporary modifications tracked via a layer system.
- **State snapshots at every priority pass** — whenever a player gains priority or focus, the full state is captured for the training corpus.

---

## 2. Game State Model

### 2.1 Zones

Each zone is a container of `GameObject` pointers with zone-specific ordering semantics:

| Zone | Per-Player | Ordered | Privacy | Contents |
|------|-----------|---------|---------|----------|
| MainDeck | Yes | Yes (stack) | Secret | Cards |
| RuneDeck | Yes | Yes (stack) | Secret | Runes |
| Hand | Yes | No (set) | Private (owner) | Cards |
| Base | Yes | No (set) | Public | Permanents, Runes |
| Trash | Yes | No (set) | Public | Cards |
| Banishment | Yes | No (set) | Public | Cards |
| ChampionZone | Yes | Single | Public | Chosen Champion (or empty) |
| LegendZone | Yes | Single | Public | Champion Legend |
| BattlefieldZone | Shared | N/A | Public | Battlefields (each is a Location) |
| Chain | Shared | Yes (stack, LIFO) | Public | ChainItems |

```cpp
enum class ZoneType {
    MainDeck, RuneDeck, Hand, Base, Trash, Banishment,
    ChampionZone, LegendZone, BattlefieldZone, Chain,
    FacedownZone // sub-zone per battlefield
};

struct Zone {
    ZoneType type;
    PlayerId owner; // kNone for shared zones
    PrivacyLevel privacy;
    std::vector<GameObjectId> objects; // ordered for decks/chain
};
```

### 2.2 Locations

Locations are places where units can be. A `LocationId` is either a `PlayerId` (for that player's Base) or a `BattlefieldId`.

```cpp
using LocationId = std::variant<PlayerId, BattlefieldId>;
```

### 2.3 Game Objects

Every card, token, and ability on the board or chain is a `GameObject`. This is the mutable runtime representation.

```cpp
struct GameObject {
    GameObjectId id;
    CardDefId card_def_id;        // link to static card data
    PlayerId owner;
    PlayerId controller;

    // Current state (after layer resolution)
    std::string name;
    CardType card_type;            // Unit, Gear, Spell, Rune, Battlefield, Legend
    SuperType super_type;          // None, Champion, Signature, Token
    std::vector<Tag> tags;
    std::vector<Domain> domains;

    // Unit-specific
    int base_might = 0;
    int current_might = 0;         // after all layers applied
    int damage_marked = 0;

    // State flags
    bool is_exhausted = false;
    bool is_ready = true;

    // Location (for permanents/runes on board)
    std::optional<LocationId> location;

    // Attachment
    std::optional<GameObjectId> attached_to;
    std::vector<GameObjectId> attachments;

    // Combat designation
    CombatDesignation combat_designation = CombatDesignation::None;

    // Buffs
    int buff_count = 0;

    // Temporary modifications (cleared on zone change or turn end)
    std::vector<TemporaryModification> temp_mods;

    // Active keyword set (computed via layers)
    KeywordSet keywords;

    // Active abilities (from rules text + effect text of attachments)
    std::vector<AbilityId> active_abilities;
};
```

### 2.4 Player State

```cpp
struct PlayerState {
    PlayerId id;
    int score = 0;                    // Victory points
    int xp = 0;

    // Rune Pool (floating resources)
    int energy = 0;
    std::map<Domain, int> power;      // per-domain power
    int universal_power = 0;          // [A] - any domain

    // Zone references
    ZoneId main_deck;
    ZoneId rune_deck;
    ZoneId hand;
    ZoneId base;
    ZoneId trash;
    ZoneId banishment;
    ZoneId champion_zone;
    ZoneId legend_zone;

    // Tracking
    int cards_played_this_turn = 0;
    bool has_discarded_this_turn = false;
    std::set<BattlefieldId> battlefields_scored_this_turn;
    bool burned_out = false;
};
```

### 2.5 Battlefield State

```cpp
struct BattlefieldState {
    BattlefieldId id;
    GameObjectId card_object_id;       // the battlefield card itself

    std::optional<PlayerId> controller;
    bool is_contested = false;
    PlayerId contested_by = kNoPlayer; // who applied contested

    // Sub-zone
    ZoneId facedown_zone;
    int facedown_max_occupancy = 1;

    // Staged events
    bool showdown_staged = false;
    bool combat_staged = false;

    // Active combat/showdown tracking
    bool showdown_in_progress = false;
    bool combat_in_progress = false;
    std::optional<PlayerId> attacker;
    std::optional<PlayerId> defender;

    // Token battlefield tracking (CR 438, 439)
    bool is_token = false;             // true if created during play (Baron Pit, Brush, blank)
    PlayerId contributed_by = kNoPlayer; // player who brought this BF to the game (or created it)

    // Replacement tracking (CR 438.5-438.7)
    // When a battlefield is Replaced, the original goes to Banishment and a token
    // takes its place. The token can potentially "swap back" to the original.
    std::optional<GameObjectId> replaced_card; // the card in Banishment this token replaced
    bool was_replaced = false;                 // true if this BF is currently a replacement token
};
```

### 2.6 Turn State

```cpp
enum class TurnPhase {
    // Start of Turn
    AwakenPhase,
    BeginningStep,
    ScoringStep,
    ChannelPhase,
    DrawPhase,
    // Main
    MainPhase,
    // End
    EndingStep,
    ExpirationStep,
    // Special
    GameOver
};

enum class NeutralShowdownState { Neutral, Showdown };
enum class OpenClosedState { Open, Closed };

// Combined: NeutralOpen, NeutralClosed, ShowdownOpen, ShowdownClosed
struct TurnState {
    PlayerId turn_player;
    TurnPhase phase;
    NeutralShowdownState ns_state;
    OpenClosedState oc_state;

    std::optional<PlayerId> priority_holder;
    std::optional<PlayerId> focus_holder;

    int turn_number = 0;
    bool is_additional_turn = false;
};
```

### 2.7 Chain State

```cpp
enum class ChainItemStatus { Pending, Finalized };

struct ChainItem {
    ChainItemId id;
    ChainItemStatus status;

    // Source: either a card being played or an ability being activated/triggered
    std::variant<GameObjectId, AbilityInstance> source;
    PlayerId controller;

    // Choices made during finalization
    std::vector<Choice> choices;       // targets, modes, locations

    // Resolved cost
    Cost total_cost;
    bool cost_paid = false;
};

struct ChainState {
    std::vector<ChainItem> items;      // top = back (newest)
    bool exists() const { return !items.empty(); }
};
```

---

## 3. Intent/Command Pattern

Every possible player action is modeled as an `Intent`. The engine:
1. Enumerates all legal `Intent`s for the current state
2. The agent (or random policy) selects one
3. The engine validates it (redundant safety check)
4. The engine executes it, mutating `GameState`

### 3.1 Intent Types

```cpp
// All discretionary and prompted actions a player can take
enum class IntentType {
    // Discretionary Actions (Main Phase, Neutral Open)
    PlayCard,              // play a card from hand/champion zone
    StandardMove,          // exhaust unit(s) to move
    ActivateAbility,       // activate an activated ability
    HideCard,              // hide a card facedown at a battlefield
    EndTurn,               // declare end of main phase

    // Chain Responses (Closed State)
    PlayReaction,          // play a Reaction card
    ActivateReactionAbility, // activate a Reaction ability
    PassPriority,          // pass priority on the chain

    // Showdown Actions (Showdown Open)
    PlayActionCard,        // play Action/Reaction card during showdown
    ActivateActionAbility, // activate Action/Reaction ability
    PassFocus,             // pass focus in showdown

    // Prompted/Limited Actions
    AssignCombatDamage,    // assign damage to enemy units
    MulliganDecision,      // choose cards to mulligan
    ChooseBattlefield,     // select battlefield for game start or staged events
    MakeChoice,            // generic choice (discard, target selection on resolution, etc.)
    SideboardSwap,         // between games of a match
    PlayFirstDecision,     // choose to go first or second

    // Triggered ability responses
    PlaceOptionalTrigger,  // choose to place a "you may" trigger on chain
    DeclineOptionalTrigger,// decline to place it
    PayTriggeredCost,      // pay cost for triggered ability finalization
    DeclineTriggeredCost,  // decline, removing trigger from chain

    // Special
    Concede
};

struct Intent {
    IntentType type;
    PlayerId player;

    // Payload varies by type:
    std::optional<GameObjectId> card;           // for PlayCard, PlayReaction
    std::optional<AbilityId> ability;           // for ActivateAbility
    std::optional<LocationId> destination;      // for StandardMove, PlayCard (unit location)
    std::vector<GameObjectId> targets;          // for spells, abilities
    std::vector<GameObjectId> units_to_move;    // for StandardMove (batch)
    std::optional<int> mode_choice;             // for modal spells

    // Combat damage assignment
    std::vector<DamageAssignment> damage_assignments;

    // Mulligan
    std::vector<GameObjectId> cards_to_mulligan;

    // Sideboard
    std::vector<std::pair<GameObjectId, GameObjectId>> swaps; // main <-> side

    // Generic choice payload
    std::vector<GameObjectId> chosen_objects;
    std::optional<int> chosen_value;
};
```

### 3.2 Legal Action Generation

```cpp
class LegalActionGenerator {
public:
    // Returns all legal intents for the player who currently must act
    std::vector<Intent> generate(const GameState& state) const;

private:
    // Sub-generators for different contexts
    std::vector<Intent> generateMainPhaseActions(const GameState&, PlayerId) const;
    std::vector<Intent> generateChainResponses(const GameState&, PlayerId) const;
    std::vector<Intent> generateShowdownActions(const GameState&, PlayerId) const;
    std::vector<Intent> generateCombatDamageAssignments(const GameState&, PlayerId) const;
    std::vector<Intent> generateMulliganOptions(const GameState&, PlayerId) const;

    // Card playability checks
    bool canPlayCard(const GameState&, PlayerId, GameObjectId card) const;
    bool isLegallyTimed(const GameState&, const GameObject& card) const;
    bool canPayCost(const GameState&, PlayerId, const Cost& cost) const;
    std::vector<LocationId> validPlayLocations(const GameState&, PlayerId, const GameObject& card) const;
    std::vector<std::vector<GameObjectId>> validTargetSets(const GameState&, const GameObject& card) const;
};
```

### 3.3 Turn State Machine

The `GameEngine` drives the turn through a state machine. Transitions are deterministic given the current state and the chosen intent.

```
GAME_START
  → Setup (CR 480 for 1v1 Duel, CR 481 for 1v1 Match):
    1. Place legends in Legend Zones
    2. Place chosen champions in Champion Zones
    3. Each player randomly selects 1 of their 3 battlefields; others are removed
    4. Selected battlefields placed simultaneously in Battlefield Zone (2 total for 1v1)
    5. Shuffle Main Decks and Rune Decks
    6. Draw 4 cards each
  → Mulligan (each player in turn order, may set aside up to 2, draw replacements, recycle set-aside)
  → First Turn Adjustments:
    - 1v1 Duel (CR 480.7): Player going second channels an extra rune on their first Channel Phase
    - 1v1 Match (CR 481.7): Same as Duel
  → TURN_LOOP

TURN_LOOP (for each player in turn order):
  → AwakenPhase: ready all controlled objects
  → BeginningStep: process "at start of turn" triggers
  → ScoringStep: hold all controlled battlefields, score points
  → ChannelPhase: channel 2 runes from rune deck
  → DrawPhase: draw 1 (or burn out), empty rune pools
  → MainPhase: PRIORITY_LOOP
  → EndingStep: process "at end of turn" triggers
  → ExpirationStep: heal all units, expire "this turn" effects, empty pools
  → CHECK_WIN → next player or GAME_OVER

PRIORITY_LOOP (Main Phase):
  → Check for staged showdowns/combats (via cleanup)
  → If Neutral Open: turn player gets priority
    → Player chooses: PlayCard | StandardMove | ActivateAbility | HideCard | EndTurn
    → If action creates Chain → CHAIN_RESOLUTION
    → If action creates Contested → stage Showdown/Combat
    → Cleanup after each action
  → If Showdown staged → SHOWDOWN_LOOP
  → If Combat staged → COMBAT_LOOP

CHAIN_RESOLUTION (FEPR):
  1. Finalize: pending items complete steps 2-5 of Playing Cards
     - Units/Gear/Add abilities resolve immediately on finalize
     - Spells linger as finalized chain items
  2. Execute: priority holder may PlayReaction | ActivateReactionAbility | PassPriority
  3. Pass: if all players passed → Resolve
  4. Resolve: newest chain item resolves, execute effects
     → If chain empty → return to Open State
     → If pending items → return to step 1
     → Else → controller of newest item gets priority → step 2

SHOWDOWN_LOOP:
  → Player with Focus may: PlayActionCard | ActivateActionAbility | PassFocus
  → If action → creates Chain → CHAIN_RESOLUTION
  → When chain resolves, focus passes to next player
  → If all players pass focus sequentially → Showdown closes
    → If Combat Showdown → COMBAT_DAMAGE
    → If Non-Combat Showdown → resolve control

COMBAT_LOOP:
  Step 1 - Combat Showdown: establish attacker/defender, run SHOWDOWN_LOOP
  Step 2 - Combat Damage:
    → Sum attacker might, sum defender might
    → Attacker assigns damage to defender's units (must assign lethal before moving on)
    → Defender assigns damage to attacker's units (same rules)
    → Deal all damage simultaneously
  Step 3 - Resolution:
    → Combat Cleanup (heal, recall attackers if defenders survive)
    → Determine winner (who has units remaining)
    → Establish control, potentially Conquer
    → End combat, remove designations
```

---

## 4. Effect System (Card Object Architecture)

> **NOTE:** The original Effect Parser (`effect_parser.h/cpp`) has been removed and replaced with the Card Object System. Each card is now a C++ class that overrides `onResolve()`, `onTrigger()`, `onActivate()` to implement its behavior. The EffectExecutor is retained as a utility library of atomic game operations that Card objects call.

### 4.0 Card Object Dispatch

All card-specific behavior is encapsulated in Card subclasses, NOT in the engine:

```
Card (abstract base)
├── UnitCard      — onTrigger() for play/combat/death/score triggers
├── SpellCard     — onResolve() for spell effects
├── GearCard      — onEquip(), equippedTriggerType(), onEquippedTrigger()
├── LegendCard    — onActivate() for legend abilities
├── BattlefieldCard — onTrigger() for score/location triggers
└── RuneCard      — minimal
```

CardRegistry maps `CardDefId → Card*`. Engine dispatches: `card_registry_.get(def_id)->onResolve(ctx, targets)`.

Generated cards: `scripts/generate_cards.py` → `src/cards/generated/*.cpp`
Manual overrides: `src/cards/manual/equip_cards.cpp`, `weaponmaster_cards.cpp`

**Key IDs (never confuse):**
- `CardDefId` = static template ID from registry.json (e.g., 601 for Soul Sword). Use `cardDefId()`.
- `GameObjectId` = runtime instance ID (e.g., 70). Use `ctx.source`.

### 4.1 Atomic Effects (EffectExecutor)

Every card ability decomposes into a sequence of these atomic effects, called via `ctx.executor.*`:

```cpp
enum class AtomicEffectType {
    // Resource
    AddEnergy,              // Add N energy to rune pool
    AddPower,               // Add N power of domain D to rune pool
    AddUniversalPower,      // Add [A] to rune pool

    // Card Movement
    Draw,                   // Draw N cards
    Discard,                // Discard N cards (may be chosen or random)
    Recycle,                // Put card on bottom of appropriate deck
    RecycleFromTrash,       // Recycle a card from trash
    Kill,                   // Send permanent to trash (triggers Deathknell)
    Banish,                 // Send card to banishment
    ReturnToHand,           // Return card to owner's hand
    Recall,                 // Return permanent to controller's base (not a Move)
    PlayFromZone,           // Play a card from a non-standard zone
    CreateToken,            // Create and play a token (unit, gear, or spell)
    CreateBattlefieldToken, // Create a battlefield token to the Battlefield Zone (CR 439.2.b.4)
    ReplaceBattlefield,     // Replace a battlefield with a token battlefield (CR 438)
    SwapBackBattlefield,    // Swap a replacement token back to its original (CR 438.7)

    // Unit Manipulation
    Move,                   // Move unit to a location
    DealDamage,             // Deal N damage to a unit
    DealDamageSplit,        // Deal N damage split among targets
    BonusDamage,            // Add bonus damage to next spell/ability
    Heal,                   // Remove damage from a unit
    Stun,                   // Exhaust and prevent readying until next turn
    Exhaust,                // Set to exhausted
    Ready,                  // Set to ready
    GiveMight,              // Modify might (+N or -N, possibly temporary)
    SetMight,               // Set might to specific value
    GiveKeyword,            // Grant a keyword (possibly temporary)
    RemoveKeyword,          // Remove a keyword
    Buff,                   // Place a buff token (+1 might)
    SpendBuff,              // Remove a buff token for cost
    GainControl,            // Change controller of a game object
    Attach,                 // Attach one card to another
    Detach,                 // Detach a card

    // Player Effects
    GainPoints,             // Gain N victory points (not Score)
    GainXP,                 // Gain N XP
    SpendXP,                // Spend N XP

    // Chain Manipulation
    Counter,                // Remove a spell/ability from the chain

    // Information
    Reveal,                 // Reveal hidden information
    LookAtCards,            // Look at top N cards of deck
    Predict,                // Look at top N, reorder/choose

    // Restrictions
    PreventNextDamage,      // Replacement: prevent damage
    CantPlayCards,          // Players can't play cards (condition + duration)
    CantBeChosen,           // This can't be chosen
    AdditionalTurn,         // Take a turn after this one

    // Conditional wrapper
    IfCondition,            // Execute sub-effects only if condition met
    ForEach,                // Execute sub-effects for each qualifying object
    DoThisNTimes,           // Reflexive trigger: add N copies to chain
    Choose,                 // Choose N from a set of objects to affect
};
```

### 4.2 Conditions

Conditions are used by passive abilities, triggered ability conditions, dependent keywords (Legion, Level), and conditional effects:

```cpp
enum class ConditionType {
    // Player state
    PlayedAnotherCardThisTurn,  // Legion
    DiscardedThisTurn,
    ControlsBattlefield,
    OpponentControlsBattlefield,
    HasNOrMoreCardsInHand,
    HasNOrMoreXP,               // Level N
    ScoreIsNOrMore,

    // Unit state
    IsAttacker,                 // for Assault
    IsDefender,                 // for Shield
    IsAlone,
    IsOneOnOne,
    IsMighty,                   // 5+ Might
    IsAtBattlefield,
    IsAtBase,
    HasKeyword,
    HasNOrMoreMight,
    HasDamage,

    // Game state
    IsYourTurn,
    IsShowdown,
    IsCombat,
    ChainExists,
    NthTimeThisTurn,            // "the first time X each turn"

    // Compound
    And,
    Or,
    Not
};
```

### 4.3 Ability Model

```cpp
enum class AbilityType {
    Passive,            // continuous effect while active
    Triggered,          // when condition met, goes on chain
    Activated,          // cost: effect, player chooses when
    ReplacementEffect,  // intercepts and alters another effect
    DelayedTriggered,   // created by another effect, single-use trigger
    DelayedPassive,     // created by another effect, temporary passive
    Reflexive,          // "Do this N times" - creates N chain items
};

struct Ability {
    AbilityId id;
    AbilityType type;

    // For Activated: the cost before the ":"
    std::optional<Cost> activation_cost;

    // For Triggered: the trigger condition ("When X happens")
    std::optional<TriggerCondition> trigger;

    // For Passive: the condition under which it applies ("While X")
    std::optional<Condition> passive_condition;

    // The effects this ability produces
    std::vector<Effect> effects;

    // Timing permissions
    bool has_action = false;
    bool has_reaction = false;

    // Optional ("you may")
    bool is_optional = false;

    // Dependent keyword wrapper
    std::optional<ConditionType> dependent_keyword; // e.g., Legion
};
```

### 4.4 Card Definition

```cpp
struct CardDef {
    CardDefId id;
    std::string set_code;          // "OGN"
    std::string card_number;       // "001/298"
    std::string name;              // "Blazing Scorcher"

    CardType card_type;            // Unit, Gear, Spell, etc.
    SuperType super_type;          // Champion, Signature, Token, None
    std::vector<Domain> domains;   // Fury, Calm, Mind, Body, Chaos, Order
    std::vector<Tag> tags;         // Dragon, Noxus, Jinx, etc.

    int energy_cost = 0;
    std::vector<PowerCost> power_costs;  // domain-specific power costs
    int might = 0;                       // base might (units only)
    int might_bonus = 0;                 // for gear with might bonus

    Rarity rarity;

    // Parsed abilities from the Ability text column
    std::vector<Ability> abilities;

    // Keywords present on this card (extracted from abilities)
    KeywordSet keywords;

    // Raw ability text (for debugging/display)
    std::string raw_ability_text;
};
```

---

## 4A. Token Battlefields, Replace, and Scoring

### Dynamic Battlefields

The board starts with a fixed number of battlefields determined by the Mode of Play (2 for 1v1), but this number can change during the game. The engine must not hardcode the battlefield count.

**Creating battlefield tokens (CR 439):**
- Baron Nashor: "As you play me, add the Baron Pit battlefield token to the board if it's not there already."
- Created via `CreateBattlefieldToken` atomic effect
- Created battlefields are **uncontrolled** (CR 439.4.b) and are added to the Battlefield Zone
- Baron Nashor's text uses "As you play me" timing — this executes during step 2 (Make Choices) of Playing Cards, **before** the unit is finalized. The battlefield must exist before the unit's play location is chosen.

**Replacing battlefields (CR 438):**
- Green Father (Ivern legend): "When you conquer or hold, you may exhaust me to replace that battlefield with a Brush battlefield token."
- The original battlefield card goes to its owner's **Banishment**, marked as "replaced" not "banished" (CR 438.5)
- The token inherits all effects and statuses of the replaced battlefield (CR 438.1)
- Units and hidden cards at the battlefield are **unaffected** and stay in place (CR 652.2.b)
- If the original was applying continuous effects (e.g., "+1 [M] to units here"), those cease when replaced (CR 652.2.c)

**Swapping back (CR 438.7):**
- Brush token: "When you score here, you may replace this with the battlefield it replaced."
- The token stops existing, the original card returns from Banishment
- The returned battlefield inherits all current effects and statuses
- If there is nothing in Banishment to swap back to (e.g., the original was itself a token that ceased to exist), the swap-back can never happen (CR 438.7.c)

**Player removal battlefield replacement (CR 652.2):**
- When a player concedes or is removed, their contributed battlefield is replaced with a **blank token battlefield** (no abilities)
- Only relevant in multiplayer modes but the system should support it

### Winning Point and Battlefield Count

The Winning Point rule (CR 466.1.b) is one of the most strategically important interactions with token battlefields:

```
Scenario: 1v1, Victory Score 8, board has 3 battlefields (2 normal + Baron Pit)
Player is at 7 points.

Via Conquer:
  - Player conquers BF-A → scored 1/3 battlefields → draws a card, stays at 7
  - Player conquers BF-B → scored 2/3 battlefields → draws a card, stays at 7
  - Player conquers Baron Pit → scored 3/3 battlefields → gains Winning Point → wins

Via Hold:
  - Player holds any single battlefield → gains Winning Point → wins (no "every BF" check)

Via card effect (e.g., Tryndamere "you score 1 point"):
  - Direct point gain → not subject to Winning Point restrictions → wins
```

The engine must:
1. Track `battlefields_scored_this_turn` per player as a set of BattlefieldIds
2. When a Score through Conquer would grant the Winning Point, compare `battlefields_scored_this_turn.size()` against the **current total number of battlefields on the board**
3. If they don't match, the player draws a card instead of gaining the point

```cpp
// In the scoring logic:
bool isWinningPointAttempt(const GameState& state, PlayerId player) {
    int victory_score = state.mode_of_play.victory_score;
    return state.players[player].score >= victory_score - 1;
}

bool canGainWinningPointViaConquer(const GameState& state, PlayerId player) {
    size_t total_battlefields = state.battlefields.size(); // includes tokens
    size_t scored_this_turn = state.players[player].battlefields_scored_this_turn.size();
    return scored_this_turn >= total_battlefields;
}

void processScore(GameState& state, PlayerId player, BattlefieldId bf, ScoreMethod method) {
    // Record the score
    state.players[player].battlefields_scored_this_turn.insert(bf);

    if (!isWinningPointAttempt(state, player)) {
        // Normal point gain
        state.players[player].score += 1;
    } else if (method == ScoreMethod::Hold) {
        // Hold always grants Winning Point (CR 466.1.b.1)
        state.players[player].score += 1;
    } else if (method == ScoreMethod::Conquer) {
        if (canGainWinningPointViaConquer(state, player)) {
            // Scored every battlefield this turn (CR 466.1.b.2)
            state.players[player].score += 1;
        } else {
            // Draw a card instead (CR 466.1.b.2)
            drawCards(state, player, 1);
        }
    }
    // Note: Direct point gains (GainPoints effect) bypass this entirely (CR 466.1.a.1)

    // Trigger score abilities (CR 466.2)
    triggerScoreAbilities(state, player, bf, method);
}
```

### Token Definitions in CardDB

Token definitions (Baron Pit, Brush, Gold, Recruit, Sprite, Bird, Reflection) must be loaded into the CardDB alongside regular cards. The gallery data includes them with `T0x` collector codes. They are referenced by name in card ability text.

```cpp
// Token registry for quick lookup by name
class TokenRegistry {
public:
    const CardDef& getTokenDef(const std::string& token_name) const;

    // Predefined token names used in card text:
    // "Baron Pit battlefield token"
    // "Brush battlefield token"
    // "Gold gear token"
    // "1 [M] Recruit unit token"
    // "3 [M] Sprite unit token with Temporary"
    // "1 [M] Bird unit token with Deflect"
    // "0 [M] Reflection unit token"
    // "3 [M] Mech unit token"
    // "2 [M] Sand Soldier unit token"
    // Blank battlefield token (no abilities, for player removal)
};
```

---

## 5. Cleanup System

Cleanups are critical and occur frequently (after nearly every state change). They must be implemented as a reentrant process since cleanups can trigger more cleanups.

```cpp
class CleanupProcessor {
public:
    // Returns true if any state changed (requiring another cleanup pass)
    bool process(GameState& state);

private:
    // Cleanup steps in order (CR 323):
    bool checkWinCondition(GameState&);           // 1. Victory check
    void updateCombatDesignations(GameState&);     // 2. Attacker/Defender
    void processLethalDamage(GameState&);          // 3a-3b. Deathknell then kill
    void updateBattlefieldControl(GameState&);     // 4. Uncontrolled if empty
    void recallMisplacedObjects(GameState&);       // 5. Gear at battlefields, wrong bases
    void stageShowdowns(GameState&);               // 6. Stage showdowns at contested BFs
    void stageCombats(GameState&);                 // 7. Stage combats
    void initiateShowdownOrCombat(GameState&);     // 8-9. Turn player chooses
};
```

---

## 6. Tagged Effects System (replaces Layer System)

The formal 3-layer system (CR 468-475) was replaced with a simpler tagged effects approach. All 787 cards in the card pool work correctly without layer ordering.

### 6.1 Aura Effects

`AuraEffect` struct on `GameObject` tracks continuous effects from aura sources:

```cpp
struct AuraEffect {
    GameObjectId source;    // who grants this
    int might_bonus = 0;    // +/- M from this aura
    int might_minimum = 0;  // minimum M (0 = no min)
    Keyword keyword;        // granted keyword (Count = none)
    int keyword_value = 0;  // e.g., Assault 2
};
```

`recalculateAuras()` runs during every `cleanup()` pass:
1. Clear all aura effects from all objects
2. Scan all objects on board for aura abilities (via `ability_text` pattern matching)
3. Apply keyword-granting auras, then might-modifying auras
4. Evaluate conditional self-effects ("If you've discarded, I have [Ganking]")
5. Apply equipment-granted keywords (Step 3c)
6. Rebuild cached `aura_might_bonus` and `aura_keywords` on all objects
7. `recomputeMight()` on all units

`hasKeyword(kw)` checks both base keywords AND aura-granted keywords.

### 6.2 Equipment System

Equipment triggers and keywords are handled through the Card object system:

- `equippedTriggerType()` — what event fires on the equipped unit
- `onEquippedTrigger()` — gear handles its own effect logic
- `equippedKeywords()` / `equippedAssault()` / `equippedShield()` — keywords granted to unit
- `onEquip()` — gear handles its own cost payment and attachment

TriggerManager checks attached gear on combat, score, move, and death events.

### 6.3 Why Not Formal Layers

The 3-layer iterative recomputation system exists in the rules for generality (handle any future card). The actual 787-card pool has no interactions that require layer ordering. Tagged effects with aura recalculation during cleanup handles every case correctly.

---

## 7. Deck Validation

Before a game begins, both decks must be validated for tournament legality:

```cpp
struct DeckSubmission {
    CardDefId legend;
    CardDefId chosen_champion;
    std::vector<CardDefId> main_deck;     // exactly 40 cards (including chosen champion)
    std::vector<CardDefId> rune_deck;     // exactly 12 runes
    std::vector<CardDefId> battlefields;  // exactly 3, unique names
    std::vector<CardDefId> sideboard;     // 0-8 cards
};

class DeckValidator {
public:
    struct ValidationResult {
        bool is_legal;
        std::vector<std::string> errors;
    };

    ValidationResult validate(const DeckSubmission& deck, const CardDB& db) const;

private:
    bool checkMainDeckSize(const DeckSubmission&) const;           // exactly 40
    bool checkRuneDeckSize(const DeckSubmission&) const;           // exactly 12
    bool checkBattlefieldCount(const DeckSubmission&) const;      // exactly 3
    bool checkBattlefieldUniqueness(const DeckSubmission&) const; // unique names
    bool checkSideboardSize(const DeckSubmission&) const;         // 0-8
    bool checkDomainIdentity(const DeckSubmission&, const CardDB&) const;
    bool checkCopyLimits(const DeckSubmission&, const CardDB&) const;     // max 3 per name across main+side
    bool checkSignatureLimits(const DeckSubmission&, const CardDB&) const; // max 3 total signatures
    bool checkChosenChampionValidity(const DeckSubmission&, const CardDB&) const;
    bool checkRuneDomains(const DeckSubmission&, const CardDB&) const;
    bool checkBannedCards(const DeckSubmission&, const CardDB&) const;
    bool checkSetLegality(const DeckSubmission&, const CardDB&) const;
};
```

### Domain Identity Rules (CR 103.1.b)
- The legend's domains define the deck's **Domain Identity**
- Single-domain cards: allowed if that domain is in the identity
- Multi-domain cards: allowed only if ALL domains are in the identity
- Runes must match domain identity
- "Colorless" cards are always allowed

---

## 8. Card Database & Card Object System

### 8.1 Card Database (CardDB)

```cpp
class CardDB {
public:
    void loadFromRegistry(const std::string& path);  // loads registry.json
    const CardDef& get(CardDefId id) const;
    const CardDef* findByName(const std::string& name) const;
private:
    std::unordered_map<CardDefId, CardDef> cards_;
};
```

CardDef holds static card data (name, cost, might, keywords, ability_text, effect_text, domains, tags). No parsed EffectScript — all behavior is in Card objects.

### 8.2 Card Object System (replaces Effect Parser)

> **The Effect Parser has been removed.** `effect_parser.h/cpp` deleted, `EffectScript` removed from `CardDef`.

Card behavior is implemented via C++ class hierarchy. Python code-gen (`scripts/generate_cards.py`) reads `registry.json` and generates card classes. 209 cards fully auto-generated, 578 have partial implementations. Manual overrides in `src/cards/manual/`.

**Chain resolution:** LIFO stack. Counter spells use peek-and-pop — the counter's `onResolve` pops the next spell off the chain. No flags, no scanning.

### 8.3 Keyword Reference (from ability_text)

The parser originally handled patterns like:

**Keywords** (bracketed):
- `[Accelerate]` → optional additional cost, enter ready
- `[Assault N]` → passive: +N might while attacker
- `[Shield N]` → passive: +N might while defender
- `[Deflect N]` → mandatory additional cost for opponents targeting this
- `[Ganking]` → passive: allow battlefield-to-battlefield standard move
- `[Action]` → permissive: can play during showdowns
- `[Reaction]` → permissive: can play during closed states
- `[Hidden]` → can hide facedown, gains reaction when played from hidden
- `[Legion]` → dependent keyword: active if another card played this turn
- `[Deathknell]` → triggered: when I die, [effect]
- `[Temporary]` → triggered: at end of turn, kill this
- `[Tank]` → passive: must be assigned combat damage first
- `[Vision]` → passive: see opponent's hidden cards here

**Triggered patterns**:
- `"When you play me, ..."` → play trigger
- `"When I attack, ..."` → attack trigger
- `"When I defend, ..."` → defend trigger
- `"When I conquer, ..."` → conquer trigger
- `"When I/you hold, ..."` → hold trigger
- `"When I move, ..."` → move trigger
- `"When you play your Nth card ..."` → play-other trigger with count
- `"When you discard me, ..."` → discard self trigger
- `"The first time X each turn, ..."` → first-time trigger
- `"At the end of your turn, ..."` → end-of-turn trigger
- `"At the start of ..."` → start-of-turn trigger

**Activated patterns**:
- `"Exhaust: [effect]"` → activated, cost = exhaust
- `"[N], Exhaust: [effect]"` → activated, cost = N energy + exhaust
- `"Discard 1, Exhaust: [effect]"` → activated, cost = discard + exhaust
- `"Recycle 1 from your trash: [effect]"` → activated, cost = recycle from trash

**Effect instructions**:
- `"Deal N to a unit at a battlefield"` → DealDamage with targeting
- `"Deal N damage split among ..."` → DealDamageSplit
- `"Kill a unit/gear"` → Kill with targeting
- `"Kill all gear"` → Kill with programmatic selection
- `"Draw N"` → Draw
- `"Discard N"` → Discard
- `"Move a/an [friendly/enemy] unit"` → Move
- `"Recall [it/a unit] [exhausted]"` → Recall
- `"Give [target] +N Might [this turn]"` → GiveMight (temporary)
- `"Buff a friendly unit"` → Buff
- `"Stun a unit"` → Stun
- `"Ready [N] [friendly] [runes/units]"` → Ready
- `"Play a N [M] [Name] token"` → CreateToken (unit/gear)
- `"Add the [Name] battlefield token to the board"` → CreateBattlefieldToken (CR 439.2.b.4)
- `"Replace [battlefield] with a [Name] battlefield token"` → ReplaceBattlefield (CR 438)
- `"Replace this with the battlefield it replaced"` → SwapBackBattlefield (CR 438.7)
- `"Counter a spell"` → Counter
- `"Reveal ..."` → Reveal
- `"Banish ..."` → Banish

**Conditional modifiers**:
- `"If you do, ..."` → conditional on previous optional action
- `"If this kills it, ..."` → conditional on kill result
- `"..., to a minimum of N"` → floor on reduction

The parser should use a **pattern-matching / regex** approach rather than a full grammar parser, since card text follows fairly regular templates.

```cpp
class EffectParser {
public:
    std::vector<Ability> parse(const std::string& raw_text, const CardDef& card_context);

private:
    // Phase 1: Split text into ability blocks
    std::vector<std::string> splitAbilities(const std::string& text);

    // Phase 2: Identify keywords
    KeywordSet extractKeywords(const std::string& block);

    // Phase 3: Parse triggered/activated/passive structure
    Ability parseTriggered(const std::string& block);
    Ability parseActivated(const std::string& block);
    Ability parsePassive(const std::string& block);

    // Phase 4: Parse effect instructions within an ability
    std::vector<Effect> parseInstructions(const std::string& instruction_text);

    // Pattern matchers for specific instruction types
    std::optional<Effect> matchDealDamage(const std::string&);
    std::optional<Effect> matchKill(const std::string&);
    std::optional<Effect> matchDraw(const std::string&);
    std::optional<Effect> matchDiscard(const std::string&);
    std::optional<Effect> matchMove(const std::string&);
    std::optional<Effect> matchBuff(const std::string&);
    std::optional<Effect> matchGiveMight(const std::string&);
    std::optional<Effect> matchCreateToken(const std::string&);
    // ... etc for each atomic effect
};
```

---

## 9. Data Output Format

### 9.1 Per-Decision-Point Snapshot

Every time a player must make a decision, a snapshot is written:

```json
{
    "game_id": "uuid",
    "turn": 5,
    "phase": "MainPhase",
    "state": "NeutralOpen",
    "priority_player": 1,
    "decision_index": 42,

    "player1": {
        "score": 2,
        "xp": 0,
        "hand_size": 4,
        "hand": ["card_id_1", "card_id_2", ...],
        "deck_size": 28,
        "rune_deck_size": 6,
        "energy": 3,
        "power": {"Fury": 1},
        "cards_played_this_turn": 1,
        "base_units": [{"id": "...", "name": "...", "might": 5, "exhausted": true, ...}],
        "base_gear": [...],
        "base_runes": [...]
    },
    "player2": {
        "score": 1,
        "hand_size": 3,
        "deck_size": 30,
        ...
    },

    "battlefields": [
        {
            "id": "bf1",
            "name": "Reaver's Row",
            "controller": 1,
            "units": [
                {"owner": 1, "name": "Vi, Destructive", "might": 4, "exhausted": false, ...},
                {"owner": 2, "name": "Pouty Poro", "might": 2, "exhausted": true, ...}
            ],
            "contested": true,
            "combat_staged": true,
            "has_hidden": false
        },
        ...
    ],

    "chain": [
        {"index": 0, "type": "Spell", "name": "Hextech Ray", "controller": 1, "targets": ["unit_xyz"], "status": "Finalized"}
    ],

    "legal_actions": [
        {"type": "PlayReaction", "card": "Shakedown", "targets": [["unit_abc"]]},
        {"type": "PassPriority"}
    ],

    "chosen_action": {"type": "PassPriority"},

    "metadata": {
        "game_result": null,
        "eventual_winner": 1,
        "turns_remaining": 8
    }
}
```

### 9.2 Game Summary

After each game completes:

```json
{
    "game_id": "uuid",
    "winner": 1,
    "final_scores": [5, 3],
    "total_turns": 13,
    "total_decisions": 186,
    "player1_deck": { "legend": "...", "champion": "...", "cards": [...] },
    "player2_deck": { "legend": "...", "champion": "...", "cards": [...] },
    "termination": "score_victory"
}
```

### 9.3 Output Format

Use **JSON Lines** (`.jsonl`) — one JSON object per line for streaming writes and easy parallel processing:
- `games/{game_id}/decisions.jsonl` — per-decision snapshots
- `games/{game_id}/summary.json` — game summary
- `index.jsonl` — index of all games with metadata

---

## 10. Match Runner

### 10.1 Mode of Play Configuration

```cpp
struct ModeOfPlay {
    int num_players = 2;
    int victory_score = 8;           // 1v1 = 8 (CR 480.3)
    int battlefield_count = 2;       // starting count; can grow via tokens
    int battlefields_per_player = 1; // each player contributes 1 (selected from 3)
    bool first_player_skips_draw = false; // only in some modes
    bool second_player_extra_channel = true; // 1v1: player 2 channels 3 on first turn
    MatchFormat format = MatchFormat::BestOf3; // Duel = BestOf1, Match = BestOf3
};

// 1v1 Duel (CR 480): BestOf1, victory=8, 2 battlefields
// 1v1 Match (CR 481): BestOf3, victory=8, 2 battlefields per game
//   - Used battlefields are removed between games; must pick from remaining
```

### 10.2 Match Flow

A match is best-of-3 with sideboarding between games:

```cpp
class MatchRunner {
public:
    struct MatchResult {
        PlayerId winner;        // or draw
        std::vector<GameResult> games;
        int total_decisions;
    };

    MatchResult run(
        DeckSubmission deck1, DeckSubmission deck2,
        AgentInterface& agent1, AgentInterface& agent2,
        const MatchConfig& config
    );

private:
    GameEngine engine_;
    DeckValidator validator_;
    DataSerializer serializer_;
};

// Agent interface - what selects actions
class AgentInterface {
public:
    virtual ~AgentInterface() = default;
    virtual Intent selectAction(
        const GameState& state,
        const std::vector<Intent>& legal_actions
    ) = 0;
    virtual DeckSubmission sideboard(
        const DeckSubmission& current,
        const DeckSubmission& opponent_seen,
        int games_won, int games_lost
    ) { return current; }  // default: no changes
};

// Random agent for initial corpus generation
class RandomAgent : public AgentInterface {
public:
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        std::uniform_int_distribution<size_t> dist(0, legal.size() - 1);
        return legal[dist(rng_)];
    }
private:
    std::mt19937 rng_;
};
```

---

## 11. Project Structure

```
automated-riftbound/
├── rules/                          # PDF + markdown rules
├── cards/                          # Card CSV data
├── docs/                           # This document + future docs
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                    # CLI entry point
│   ├── core/
│   │   ├── types.h                 # Enums, IDs, basic types
│   │   ├── card_def.h/cpp          # CardDef, CardDB, CSV loader
│   │   ├── game_object.h/cpp       # GameObject runtime representation
│   │   ├── game_state.h/cpp        # Full game state container
│   │   ├── player_state.h/cpp      # Per-player state
│   │   ├── zone.h/cpp              # Zone containers
│   │   ├── battlefield.h/cpp       # Battlefield state
│   │   ├── chain.h/cpp             # Chain + chain items
│   │   ├── rune_pool.h/cpp         # Energy/Power tracking
│   │   └── keywords.h              # Keyword enum + KeywordSet
│   ├── engine/
│   │   ├── game_engine.h/cpp       # Main turn loop + phase sequencing
│   │   ├── action_resolver.h/cpp   # Executes intents against state
│   │   ├── cleanup.h/cpp           # Cleanup processor
│   │   ├── chain_resolver.h/cpp    # FEPR chain resolution
│   │   ├── combat.h/cpp            # Combat steps + damage assignment
│   │   ├── showdown.h/cpp          # Showdown loop
│   │   ├── layer_engine.h/cpp      # Layer system for computed state
│   │   └── effect_executor.h/cpp   # Executes atomic effects
│   ├── rules/
│   │   ├── legal_actions.h/cpp     # Legal action generator
│   │   ├── deck_validator.h/cpp    # Deck legality checker
│   │   ├── targeting.h/cpp         # Target validation
│   │   └── cost_calculator.h/cpp   # Cost determination (base + mods)
│   ├── effects/
│   │   ├── effect.h                # Effect types + Effect struct
│   │   ├── effect_parser.h/cpp     # Parse ability text → Effects
│   │   ├── condition.h/cpp         # Condition evaluation
│   │   ├── ability.h/cpp           # Ability model
│   │   └── replacement.h/cpp      # Replacement effect handling
│   ├── agents/
│   │   ├── agent_interface.h       # Abstract agent
│   │   ├── random_agent.h/cpp      # Random action selection
│   │   └── scripted_agent.h/cpp    # Deterministic for testing
│   ├── io/
│   │   ├── data_serializer.h/cpp   # JSON-lines output
│   │   ├── state_logger.h/cpp      # Per-decision snapshots
│   │   ├── state_renderer.h/cpp    # ASCII game board renderer
│   │   └── match_runner.h/cpp      # Best-of-3 orchestration
│   └── util/
│       ├── rng.h/cpp               # Seedable RNG wrapper
│       └── id_generator.h/cpp      # Unique ID generation
├── tests/
│   ├── test_deck_validator.cpp
│   ├── test_effect_parser.cpp
│   ├── test_cleanup.cpp
│   ├── test_combat.cpp
│   ├── test_chain_resolution.cpp
│   ├── test_legal_actions.cpp
│   └── test_game_scenarios.cpp
├── data/                           # Output directory for replay corpus
└── scripts/
    └── analyze_corpus.py           # Quick stats on generated data
```

---

## 12. Build System

```cmake
cmake_minimum_required(VERSION 3.20)
project(riftbound-engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS
    program_options
    filesystem
    serialization
    json
)

# nlohmann/json for JSON output
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# FTXUI for ASCII game state rendering
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# Google Test
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_executable(riftbound-engine
    src/main.cpp
    # ... all source files
)

target_link_libraries(riftbound-engine
    Boost::program_options
    Boost::filesystem
    Boost::json
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
)

# Tests
enable_testing()
add_executable(riftbound-tests
    tests/test_deck_validator.cpp
    # ... all test files
)
target_link_libraries(riftbound-tests
    GTest::gtest_main
    # ... engine libs
)
```

---

## 13. Implementation Order

### Phase 1: Foundation
1. `types.h` — all enums, type aliases, IDs
2. `card_def.h/cpp` — CardDef struct + CSV loader
3. `keywords.h` — keyword enum and bitset
4. `deck_validator.h/cpp` — full deck legality checking
5. Unit tests for deck validation

### Phase 2: Game State
6. `zone.h/cpp` — zone containers
7. `game_object.h/cpp` — runtime game objects
8. `player_state.h/cpp` — per-player state
9. `battlefield.h/cpp` — battlefield state
10. `rune_pool.h/cpp` — energy/power pool
11. `chain.h/cpp` — chain + chain items
12. `game_state.h/cpp` — full state container
13. `layer_engine.h/cpp` — computed state from layers

### Phase 3: Core Engine
14. `intent.h` — Intent/Command types
15. `legal_actions.h/cpp` — enumerate legal actions
16. `cost_calculator.h/cpp` — cost determination
17. `targeting.h/cpp` — target validation
18. `cleanup.h/cpp` — cleanup processor
19. `chain_resolver.h/cpp` — FEPR resolution
20. `action_resolver.h/cpp` — execute intents
21. `game_engine.h/cpp` — turn loop + phase sequencing
22. Unit tests for turn phases, chain resolution

### Phase 4: Combat & Showdowns
23. `showdown.h/cpp` — showdown loop
24. `combat.h/cpp` — combat steps + damage assignment
25. Integration tests for combat scenarios

### Phase 5: Effect System
26. `effect.h` — atomic effect types
27. `condition.h/cpp` — condition evaluation
28. `ability.h/cpp` — ability model
29. `effect_parser.h/cpp` — parse card text → structured effects
30. `effect_executor.h/cpp` — execute atomic effects
31. `replacement.h/cpp` — replacement effect handling
32. Tests with real card texts from Origins CSV

### Phase 6: Data Output
33. `data_serializer.h/cpp` — JSON-lines snapshots
34. `state_logger.h/cpp` — capture at decision points
35. `state_renderer.h/cpp` — ASCII game board renderer
35. `match_runner.h/cpp` — best-of-3 orchestration
36. `random_agent.h/cpp` — random policy for corpus generation
37. `main.cpp` — CLI with Boost.ProgramOptions

### Phase 7: Integration & Polish
38. End-to-end tests with real decks
39. Performance profiling and optimization
40. Batch game runner for corpus generation

---

## 14. Key Invariants to Enforce

These are rules the engine must always maintain:

1. **Priority exclusivity**: At most one player has Priority at any time.
2. **Focus implies Priority**: A player with Focus always has Priority.
3. **Chain determines state**: Chain exists ↔ Closed State.
4. **Showdown/Combat determines state**: Showdown or Combat in progress ↔ Showdown State.
5. **Units enter exhausted** (unless Accelerate paid or other effect).
6. **Gear enters ready**.
7. **Damage lethal check**: Unit with damage >= might is killed in next cleanup.
8. **Score once per battlefield per turn**: No battlefield can be scored twice by the same player in one turn.
9. **Winning point restrictions (CR 466.1.b)**: When a player is 1 point from Victory Score or higher:
   - **Hold**: Always grants the Winning Point. No extra conditions.
   - **Conquer**: Only grants the Winning Point if the player has **Scored every Battlefield currently on the board** this turn (including token battlefields). Otherwise the player draws a card instead.
   - **Direct point gain** (e.g., Tryndamere, Ivern, Friend to All): Not subject to Winning Point restrictions at all (CR 466.1.a.1).
10. **Copy limit**: Max 3 copies of any named card in main deck + sideboard combined.
11. **Cleanup reentrance**: Cleanups may trigger more cleanups; process until stable.
12. **Tokens cease to exist in non-board zones** (except chain).
13. **Rune pools empty** at end of draw phase and end of turn.
14. **Zone transitions clear temporary modifications**.
15. **Replacement effects apply at most once per event**.
16. **Dynamic battlefield count**: The number of battlefields on the board can change during play. Token battlefields can be added (CR 439.2.b.4, e.g., Baron Pit) or existing battlefields can be replaced with tokens (CR 438, e.g., Brush). The Winning Point check (invariant 9) must count ALL battlefields currently on the board, not just the starting set.
17. **Replaced battlefields go to Banishment** (CR 438.5): The original card is placed in Banishment marked as "replaced" (not "banished"). It can be returned via "swap back" (CR 438.7). If a token is replaced, it ceases to exist in Banishment (CR 438.6).
18. **Created battlefields are uncontrolled** (CR 439.4.b): When a battlefield token is created to the Battlefield Zone, it has no controller.
19. **Token battlefields can't be in starting decks**: Token battlefields like Baron Pit explicitly state "(You can't start the game with a token battlefield.)" They can only enter play through card effects.

---

## 15. ASCII Game State Renderer

### Purpose

Render the full board state as a human-readable ASCII diagram at any point during the game. Used for:
- **Debugging** — inspect game state when testing rules interactions
- **Replay logs** — human-readable game transcripts alongside JSON tensor data
- **CI output** — readable test failure diagnostics
- **Future TUI** — foundation for an interactive terminal client

### Library

Use [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — a modern C++ terminal UI library with:
- Box-drawing characters and borders
- Flexbox-style layout composition
- Color support (domain-colored cards)
- No external dependencies, header-only option available
- Works on Linux/Mac/Windows

If FTXUI is too heavy, a lightweight alternative is to render directly using Unicode box-drawing characters (`┌─┐│└─┘`) with a simple string-buffer approach.

### Render Layout

The renderer produces the full 1v1 board mirroring the physical playmat layout (see `docs/playmat-layout.md`). Both players' boards are shown, with shared battlefields in the center.

```
┌─ TURN 5 ── Main Phase ── Neutral Open ── Priority: P1 ─────────────────────┐
│                                                                              │
│  P2 [Score: 2] [Hand: 4] [Deck: 26] [Runes: 6/12] [Energy: 0] [Power: 0]  │
│  Legend: Bounty Hunter (Body/Chaos)    Champion: Miss Fortune, Captain (5M)  │
│  ┌─ P2 Base ────────────────────────────────────────────────────────────────┐│
│  │ Units: Mindsplitter(6M) Elder Dragon(8M,exh)                            ││
│  │ Gear:  Stacked Deck(rdy) Lunar Boon(exh)                               ││
│  │ Runes: BodyRune(rdy) BodyRune(rdy) ChaosRune(exh) ChaosRune(exh)      ││
│  └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─ Battlefield: Aspirant's Climb [Controlled: P2] ────────────────────────┐│
│  │  P2: Gust(3M,rdy)                                                      ││
│  │  P1: Watchful Sentry(3M,exh) Soaring Scout(2M,exh) ← CONTESTED        ││
│  │  [Facedown: 1 card]                                                     ││
│  └──────────────────────────────────────────────────────────────────────────┘│
│  ┌─ Battlefield: Vilemaw's Lair [Controlled: --] ──────────────────────────┐│
│  │  (empty)                                                                ││
│  └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─ P1 Base ────────────────────────────────────────────────────────────────┐│
│  │ Units: Honest Broker(3M,rdy) Ruined Rex(4M,rdy)                        ││
│  │ Gear:  Hidden Blade(rdy)                                                ││
│  │ Runes: OrderRune(rdy)x3 MindRune(rdy)x2 OrderRune(exh)                ││
│  └──────────────────────────────────────────────────────────────────────────┘│
│  P1 [Score: 3] [Hand: 5] [Deck: 22] [Runes: 8/12] [Energy: 3] [Power: Y1] │
│  Legend: Deceiver (Mind/Order)         Champion: LeBlanc, Fragmented (4M)   │
│                                                                              │
│  ┌─ Chain ──────────────────────────────────────────────────────────────────┐│
│  │  [1] Cull the Weak (P1, targets: Gust) — Finalized                     ││
│  └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  Legal Actions: PlayReaction | PassPriority                                  │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Renderer API

```cpp
class StateRenderer {
public:
    // Render full board to string
    std::string render(const GameState& state) const;

    // Render a specific zone
    std::string renderBattlefield(const GameState& state, BattlefieldId bf) const;
    std::string renderBase(const GameState& state, PlayerId player) const;
    std::string renderChain(const GameState& state) const;
    std::string renderPlayerSummary(const GameState& state, PlayerId player) const;

    // Render game object inline (for use within zone renders)
    // Format: "CardName(NM,exh)" or "CardName(NM,rdy,Assault,Shield)"
    std::string renderGameObject(const GameState& state, GameObjectId obj) const;

    // Configuration
    int max_width = 80;            // terminal width
    bool show_hand = false;        // show hand contents (for debugging, not opponent view)
    bool show_legal_actions = true;
    bool use_color = true;         // ANSI color codes for domains
};
```

### Integration Points

- **StateLogger**: optionally appends `render()` output alongside JSON snapshots
- **GameEngine**: call `render()` after each phase transition or decision point
- **Tests**: dump rendered state on assertion failure for readable diagnostics
- **main.cpp**: `--render` flag to print board state each turn to stdout

### Unit Rendering Shorthand

Game objects are rendered as compact strings:

| Example | Meaning |
|---------|---------|
| `Jinx,Demo(4M,exh,Asl2)` | Jinx, Demolitionist: 4 Might, exhausted, Assault 2 |
| `Pouty Poro(2M,rdy,Dfl)` | Pouty Poro: 2 Might, ready, Deflect |
| `Iron Ballista(exh)` | Gear, exhausted |
| `FuryRune(rdy)` | Rune, ready |
| `FuryRune(exh)` | Rune, exhausted (tapped for energy) |
| `[facedown]` | Hidden card at battlefield |
| `GoldToken(gear)` | Token gear |
| `3M Recruit(exh)` | Token unit, 3 might, exhausted |

Keywords use abbreviations: `Asl`=Assault, `Shd`=Shield, `Dfl`=Deflect, `Gnk`=Ganking, `Tnk`=Tank, `Tmp`=Temporary, `Hdn`=Hidden, `Acc`=Accelerate.

Damage is shown when non-zero: `Volibear(9M,2dmg,rdy,Dfl2)` = 9 might, 2 damage marked, ready, Deflect 2.

---

## 16. Testing Strategy

- **Unit tests** for each subsystem (deck validation, cleanup, combat damage, cost calculation, targeting, effect parser)
- **Scenario tests** using hand-crafted game states to test specific rule interactions
- **Fuzz testing** with random agents to find crashes or illegal states
- **Regression tests** from known rule corner cases in the Core Rules
- **Comparison tests** once card text parser covers enough cards to run full games against expected outcomes

---

## 16. Performance Considerations

- **Object pooling** for GameObjects to avoid allocation churn
- **Bitsets for keywords** — KeywordSet as a `std::bitset<32>` for O(1) keyword checks
- **Incremental layer recomputation** — track dirty objects rather than recomputing all
- **State hashing** for transposition detection (useful for future MCTS)
- **Parallel game execution** — each game is independent, run on separate threads
- Target: **1000+ games/second** on a modern machine with random agents

---

## 17. Current Implementation Status (as of 2026-05-12)

### Completed (v1.0 Engine)
- **Phases 1-3**: Full engine, chain/FEPR, spells, triggers, 23 keywords, combat, scoring
- **Phase 4**: Card Object System — 787 cards registered, EffectParser removed, code-gen + manual overrides
- **Phase 5a**: Burn Out, tokens, cost reduction, predict, delayed abilities, additional turns, BF replace/swap-back, equip/attach
- **Phase 5b**: Tagged aura effects (18 aura cards), conditional self-effects, copy effects, Elder Dragon rule, combat damage agent choice, counter spell peek-and-pop, Challenge dual targeting, channel runes exhausted
- **Phase 5c**: Full equip system (33 gear, 9 Weaponmaster units), equipment-granted keywords/triggers, CardDefId crash fix
- **Phase 5d**: Rune selection agent choice, 25+ manual card implementations, Ambush, Quick-Draw, opponent discard, reveal/search agent choices, equip inactive text
- **Phase 6a**: Batch game runner — `boost::asio::thread_pool`, `GameRunner`/`BatchRunner`, manual DI via shared `AppContext`, `--threads N` CLI, per-game JSONL output
- **Observability**: Trace logging (`--trace`), HTML replay viewer (`--render`), game UUID, card-name logging for draw/discard/rune/mulligan
- **Verification**: 72 tests, 5 decks, 15 matchups, 68 manually implemented cards, zero crashes
- **Performance**: ~18 games/sec (1 thread), ~43 games/sec (4 threads) in Debug build

### Key Architecture Decisions
- **Card mechanics encapsulated in Card objects** — engine is generic dispatch + flow control. Never add card-specific logic to game_engine.cpp.
- **Tagged effects instead of formal layer system** — `AuraEffect` structs, recalculated during `cleanup()`. Handles all 787 cards without ordering issues.
- **Chain is LIFO stack with peek-and-pop counters** — counter spells pop the next item. No flags needed. Counter-of-counter works via LIFO naturally.
- **CardDefId vs GameObjectId** — two distinct ID types. `cardDefId()` = static template. `ctx.source` = runtime instance. Never confuse them.
- **Energy before power** in cost payment — exhaust runes first, then recycle exhausted runes for domain power. Each selection is an agent decision point.
- **Manual DI + thread pool** — `AppContext` holds shared singletons (CardDB, CardRegistry). `BatchRunner` wraps `boost::asio::thread_pool`. `GameRunner` is per-game, fully thread-safe. No Boost.DI (not in standard Boost, fragile on C++20).

### Manual Card Files
| File | Count | Scope |
|------|-------|-------|
| `generated/*.cpp` | 787 | All cards (auto-gen, 209 full / 578 partial) |
| `manual/equip_cards.cpp` | 33 | Equipment gear with cost + triggers |
| `manual/weaponmaster_cards.cpp` | 9 | Weaponmaster units |
| `manual/deck_cards.cpp` | 25 | Key cards from 5 test decks |

### Remaining for v1.0 Polish
- 12 do-nothing cards in test decks (Vex Cheerless, Honest Broker, Overzealous Fan, etc.)
- 578 complex cards with partial implementations (incremental, priority by usage)

### Threading Architecture
```
AppContext (singleton, const after init)
├── CardDB (loaded once, shared read-only)
├── CardRegistry (loaded once, shared read-only)
└── DeckSubmission[2]

BatchRunner (owns boost::asio::thread_pool)
└── posts N GameRunner lambdas

GameRunner (per-game, worker thread)
├── EventBus (per-game)
├── GameEngine (per-game, refs AppContext)
├── RandomAgent[2] (per-game)
├── DataSerializer → game-specific .jsonl file
└── ReplayWriter → game-specific .html file

AggregateResults (shared, atomic counters + mutex for console)
```

### Phase 6b: ML Pipeline (NEXT)
- Training data pipeline, supervised baseline, self-play RL, MCTS/AlphaZero
- Deck construction agent (card selection with tournament legality constraints)
