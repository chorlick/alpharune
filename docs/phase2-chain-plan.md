# Phase 2: Chain Resolution — Implementation Plan

## Overview

The Chain is the core mechanic that enables spells, reactions, triggered abilities, and activated abilities. It's a recursive state machine where items stack, players pass priority, and items resolve last-in-first-out.

## What the Chain Enables

- **Spells** (~192 cards): Action spells during showdowns, Reaction spells any time
- **Activated abilities**: Gear abilities (e.g., "Exhaust: Deal 2 to a unit"), Legend abilities, Rune abilities (exhaust for Energy, recycle for Power)
- **Triggered abilities**: Play triggers, attack/defend triggers, conquer/hold triggers, death triggers
- **The FEPR process**: Finalize → Execute → Pass → Resolve

## Architecture Changes Needed

### 1. ChainManager (new class, inject into GameEngine)

```cpp
class ChainManager {
public:
    ChainManager(GameState& state, EventBus& events);

    // Add a spell or ability to the chain as a Pending Item
    ChainItemId addPendingItem(GameObjectId source, PlayerId controller);

    // Run the FEPR loop until chain is empty
    void processFEPR(std::function<Intent(PlayerId, std::vector<Intent>)> query_agent);

    // Check if chain exists (determines Open/Closed state)
    bool chainExists() const;

private:
    void stepFinalize();    // Step 1: finalize pending items
    void stepExecute();     // Step 2: priority holder acts (play reaction / pass)
    void stepPass();        // Step 3: check if all passed
    void stepResolve();     // Step 4: resolve top item
};
```

### 2. Changes to GameEngine

- `executePlayCard` for spells: card goes on chain as Pending, then FEPR runs
- `executePlayCard` for permanents (units/gear): current behavior stays (resolve immediately on finalize, CR 337.1.c)
- Main phase: after any action that creates a chain, run FEPR before returning to main phase loop
- Showdown loop: Action spells create chains during showdowns
- New IntentTypes used: `PlayReaction`, `PassPriority` (already defined in types.h)

### 3. Spell Resolution

When a spell resolves (Step 4), execute its effects. Phase 2 effects:

```
DealDamage    — "Deal N to a unit at a battlefield"
Kill          — "Kill a unit/gear"
Draw          — "Draw N"
Discard       — "Discard N"
Move          — "Move a unit"
Ready         — "Ready N runes/units"
Heal          — "Heal a unit"
Buff          — "Buff a friendly unit"
GiveMight     — "Give a unit +N Might this turn"
Counter       — "Counter a spell" (remove from chain)
```

### 4. Effect Executor (new class)

```cpp
class EffectExecutor {
public:
    void execute(const CardDef& card, GameState& state, EventBus& events,
                 PlayerId controller, const std::vector<GameObjectId>& targets);
};
```

For Phase 2, effects can be hardcoded per-card for the most common spells. The general effect parser (from ability text) is Phase 3+.

### 5. Targeting

Spells that target need:
- Legal target enumeration during chain finalization
- Target validation on resolution (targets may have become illegal)
- The Intent needs to carry target selections

### 6. Priority Passing

When chain exists (Closed State):
1. Controller of newest item gets Priority
2. That player can: PlayReaction | ActivateReactionAbility | PassPriority
3. If they pass, next player in turn order gets Priority
4. If all players pass without adding items → Resolve top item
5. If someone adds an item → back to step 1 (Finalize new item first)

### 7. Turn State Changes

- Playing a card/ability → creates chain → state becomes Closed
- All items resolved → chain empty → state becomes Open
- During Showdown + Closed = ShowdownClosed (only Reaction)
- During Showdown + Open = ShowdownOpen (Action or Reaction)
- During Neutral + Closed = NeutralClosed (only Reaction)

### 8. Event Bus Integration

New events needed:
- `ChainCreatedEvent` — when first item placed
- `ChainItemFinalizedEvent` — when pending becomes finalized
- `ChainItemResolvedEvent` — when item resolves
- `ChainEmptiedEvent` — when last item resolves
- `SpellResolvedEvent` — after spell effects execute

Triggered abilities subscribe to existing events:
- `on_card_played` → play triggers ("When you play me")
- `on_unit_moved` → move triggers ("When I move")
- `on_unit_died` → death triggers, Deathknell
- `on_combat_started` → attack/defend triggers
- `on_score` → conquer/hold triggers

When a trigger fires, it adds a new Pending Item to the chain, which restarts FEPR.

### 9. Implementation Order

1. `ChainManager` class with FEPR loop (no effects yet, just the state machine)
2. Wire spell play into main phase: PlayCard for spells → add to chain → FEPR
3. Priority passing in FEPR (PlayReaction / PassPriority intents)
4. Spell resolution: execute effects on resolve
5. Basic effect executor for ~10 common effects (hardcoded per card initially)
6. Targeting: legal target generation + target validation
7. Action/Reaction timing enforcement
8. Triggered abilities: event subscribers that add Pending Items

### 10. Testing Strategy

- Unit test FEPR state machine with mock spells (no effects)
- Test priority passing: P1 plays spell → P2 can react → P1 can react to reaction → both pass → resolves LIFO
- Test targeting: spell with invalid target on resolution does nothing
- Test Action timing: Action spells can play during showdowns
- Test Reaction timing: Reaction spells can play during Closed state
- Integration test: real cards (Hextech Ray, Disintegrate, Charm) in a game

### 11. Cards to Test With

Good starter spells for Phase 2 (simple effects, common patterns):

| Card | Cost | Effect | Pattern |
|------|------|--------|---------|
| Hextech Ray | 1E | Deal 3 to a unit at a BF | Action, DealDamage |
| Disintegrate | 4E | Deal 3, if kills draw 1 | Action, DealDamage + conditional Draw |
| Charm | 1E | Move an enemy unit | simple Move |
| Cleave | 1E | Give unit Assault 3 this turn | Action, GiveMight (temporary) |
| Gust | 1E | Move a friendly unit | simple Move |
| Rebuke | 2E | Move an enemy unit to base | Move to base |
| Discipline | 1E | Give unit +2M this turn | GiveMight (temporary) |
| Salvage | 2E | Kill up to 1 gear, draw 1 | Action, Kill + Draw |
| Highlander | 4E | Replacement: prevent unit death | Reaction, Replacement |
| Shakedown | 2E+R | Deal 6 or opponent draws 2 | Reaction, conditional |

### 12. Estimated Complexity

- ChainManager + FEPR: ~300 lines
- Effect executor (10 effects hardcoded): ~200 lines
- Targeting system: ~150 lines
- Priority passing integration: ~100 lines
- Triggered ability framework: ~200 lines
- Tests: ~300 lines
- Total: ~1250 lines of new code

This is a full session's work. The FEPR loop is the hardest part — getting the state transitions right with reentrant cleanup and priority passing.
