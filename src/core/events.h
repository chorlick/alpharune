#pragma once
/// @file events.h
/// Game event definitions and the event bus.
///
/// The event bus is the central nervous system of the engine. Game actions
/// emit events, and subsystems subscribe to them. This is how:
///   - Triggered abilities detect their conditions (CR 382)
///   - Cleanup knows when to run (CR 319)
///   - The state logger captures decision points
///   - The ASCII renderer updates
///
/// Uses Boost.Signals2 for thread-safe signal/slot connections.

#include "types.h"

#include <boost/signals2.hpp>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {

// ─── Event types ─────────────────────────────────────────────────────────────
// Each event carries the minimum data needed for subscribers to react.
// Events are emitted AFTER the action has been applied to game state,
// unless noted otherwise.

/// A unit or permanent moved from one location to another.
struct UnitMovedEvent {
    GameObjectId object;
    PlayerId controller;
    LocationId from;
    LocationId to;
    bool is_standard_move;  // true if standard move action, false if effect-driven
};

/// A game object entered the board (played, created, channeled).
struct EnteredBoardEvent {
    GameObjectId object;
    PlayerId controller;
    CardType card_type;
    LocationId location;
    bool from_play;         // true if played from hand/champion zone
};

/// A token was created on the board.
struct TokenCreatedEvent {
    GameObjectId object;
    PlayerId controller;
    CardType card_type;
    std::string token_type;  // "Recruit", "Sprite", etc.
    LocationId location;
};

/// A game object left the board (killed, recycled, returned to hand, etc).
struct LeftBoardEvent {
    GameObjectId object;
    PlayerId controller;
    CardType card_type;
    LocationId was_at;
    ZoneType destination;   // Trash, Hand, MainDeck, RuneDeck, Banishment
    bool was_killed;        // true if death (triggers Deathknell)
};

/// Damage was dealt to a unit.
struct DamageDealtEvent {
    GameObjectId target;
    int amount;
    GameObjectId source;    // kInvalidId if from combat
    bool is_combat_damage;
};

/// A unit was killed (sent to trash from board due to lethal damage or kill effect).
struct UnitDiedEvent {
    GameObjectId object;
    PlayerId controller;
    LocationId was_at;
    int might_at_death;
};

/// A player drew one or more cards.
struct CardsDrawnEvent {
    PlayerId player;
    int count;
};

/// A player discarded one or more cards.
struct CardsDiscardedEvent {
    PlayerId player;
    std::vector<GameObjectId> cards;
};

/// A card was played (finalized onto chain or board).
struct CardPlayedEvent {
    GameObjectId object;
    PlayerId player;
    CardType card_type;
    int cards_played_this_turn; // count after this play
};

/// A rune was channeled from rune deck to base.
struct RuneChanneledEvent {
    GameObjectId rune;
    PlayerId player;
};

/// Battlefield control changed.
struct ControlChangedEvent {
    BattlefieldId battlefield;
    std::optional<PlayerId> old_controller;
    std::optional<PlayerId> new_controller;
};

/// Battlefield became contested.
struct ContestedEvent {
    BattlefieldId battlefield;
    PlayerId contested_by;
};

/// A player scored (conquer or hold).
struct ScoreEvent {
    PlayerId player;
    BattlefieldId battlefield;
    ScoreMethod method;
    int new_score;
};

/// A player gained points directly (from card effect).
struct PointsGainedEvent {
    PlayerId player;
    int amount;
    int new_score;
};

/// Turn phase changed.
struct PhaseChangedEvent {
    TurnPhase old_phase;
    TurnPhase new_phase;
    PlayerId turn_player;
};

/// Priority or focus was granted to a player.
struct PriorityGrantedEvent {
    PlayerId player;
    bool has_focus;
};

/// Combat started at a battlefield.
struct CombatStartedEvent {
    BattlefieldId battlefield;
    PlayerId attacker;
    PlayerId defender;
};

/// Combat ended at a battlefield.
struct CombatEndedEvent {
    BattlefieldId battlefield;
    std::optional<PlayerId> winner;
};

/// Showdown started (combat or non-combat).
struct ShowdownStartedEvent {
    BattlefieldId battlefield;
    bool is_combat_showdown;
};

/// Showdown ended.
struct ShowdownEndedEvent {
    BattlefieldId battlefield;
};

/// Game state change requiring cleanup (CR 319).
struct CleanupNeededEvent {};

/// A game object's state changed (exhausted, readied, buffed, etc).
struct ObjectStateChangedEvent {
    GameObjectId object;
    std::string what_changed; // "exhausted", "readied", "buffed", "damaged", etc.
};

/// The game ended.
struct GameOverEvent {
    PlayerId winner; // PlayerId::None for draw
    std::string reason;
};

// ─── Debug logging ──────────────────────────────────────────────────────────

enum class LogLevel : uint8_t { Trace, Debug, Info, Warning };

struct LogEvent {
    LogLevel level;
    std::string message;
};

// ─── Chain events ───────────────────────────────────────────────────────────

/// A chain was created (first item placed, state becomes Closed).
struct ChainCreatedEvent {
    ChainItemId first_item;
    PlayerId controller;
};

/// A pending chain item was finalized.
struct ChainItemFinalizedEvent {
    ChainItemId item;
    GameObjectId source;
    PlayerId controller;
};

/// A chain item resolved (top of chain executed).
struct ChainItemResolvedEvent {
    ChainItemId item;
    GameObjectId source;
    PlayerId controller;
};

/// The chain emptied (last item resolved, state becomes Open).
struct ChainEmptiedEvent {};

/// A spell resolved (effects executed, spell goes to trash).
struct SpellResolvedEvent {
    GameObjectId spell;
    PlayerId controller;
};

/// A card's identity was revealed to one or more observers. Fired by
/// Aurora's top-of-deck reveal, Mindsplitter / Sabotage hand reveals,
/// Vision / Predict peek-ahead, Stacked Deck searches, etc.
///
/// Observers (PlayerStates that have `revealed_to_player` in their bit)
/// can incorporate the revealed identity into their state representation
/// via the (currently unfeaturized) PlayerState::observed_cards vector.
///
/// Phase B-2 follow-up: wire this from Aurora / Mindsplitter / Sabotage /
/// Vision / Predict card implementations and expose observed_cards as
/// new feature dims in the trainer (see Phase 10 — Memory-Augmented
/// Agents in CLAUDE.md).
struct CardRevealedEvent {
    GameObjectId card;            // the card that was revealed
    CardDefId    card_def_id;     // its identity (registry index)
    PlayerId     owner;           // who owns/owned the card
    /// Who is allowed to know the identity now. If `all`, every player
    /// learns the identity (full publicity, e.g., facedown flip on combat).
    /// Otherwise this is the single player observing the reveal.
    bool         revealed_to_all = false;
    PlayerId     revealed_to     = PlayerId::None;
    /// Zone the card was in when revealed (top of deck, hand, facedown).
    /// Useful when masking — the model may handle "saw it in opponent's
    /// hand" differently from "saw it briefly on top of deck."
    ZoneType     source_zone     = ZoneType::MainDeck;
};

// ─── Event Bus ───────────────────────────────────────────────────────────────
/// Central event dispatcher using Boost.Signals2.
///
/// Subsystems subscribe during construction (dependency injection).
/// The engine emits events as game actions are processed.
///
/// Usage:
///   bus.on_unit_moved.connect([](const UnitMovedEvent& e) { ... });
///   bus.emit(UnitMovedEvent{...});

class EventBus {
public:
    // Signals — subscribers connect to these
    boost::signals2::signal<void(const UnitMovedEvent&)>          on_unit_moved;
    boost::signals2::signal<void(const EnteredBoardEvent&)>       on_entered_board;
    boost::signals2::signal<void(const TokenCreatedEvent&)>      on_token_created;
    boost::signals2::signal<void(const LeftBoardEvent&)>          on_left_board;
    boost::signals2::signal<void(const DamageDealtEvent&)>        on_damage_dealt;
    boost::signals2::signal<void(const UnitDiedEvent&)>           on_unit_died;
    boost::signals2::signal<void(const CardsDrawnEvent&)>         on_cards_drawn;
    boost::signals2::signal<void(const CardsDiscardedEvent&)>     on_cards_discarded;
    boost::signals2::signal<void(const CardPlayedEvent&)>         on_card_played;
    boost::signals2::signal<void(const RuneChanneledEvent&)>      on_rune_channeled;
    boost::signals2::signal<void(const ControlChangedEvent&)>     on_control_changed;
    boost::signals2::signal<void(const ContestedEvent&)>          on_contested;
    boost::signals2::signal<void(const ScoreEvent&)>              on_score;
    boost::signals2::signal<void(const PointsGainedEvent&)>       on_points_gained;
    boost::signals2::signal<void(const PhaseChangedEvent&)>       on_phase_changed;
    boost::signals2::signal<void(const PriorityGrantedEvent&)>    on_priority_granted;
    boost::signals2::signal<void(const CombatStartedEvent&)>      on_combat_started;
    boost::signals2::signal<void(const CombatEndedEvent&)>        on_combat_ended;
    boost::signals2::signal<void(const ShowdownStartedEvent&)>    on_showdown_started;
    boost::signals2::signal<void(const ShowdownEndedEvent&)>      on_showdown_ended;
    boost::signals2::signal<void(const CleanupNeededEvent&)>      on_cleanup_needed;
    boost::signals2::signal<void(const ObjectStateChangedEvent&)> on_object_state_changed;
    boost::signals2::signal<void(const GameOverEvent&)>           on_game_over;

    // Logging
    boost::signals2::signal<void(const LogEvent&)>                 on_log;

    // Chain signals
    boost::signals2::signal<void(const ChainCreatedEvent&)>        on_chain_created;
    boost::signals2::signal<void(const ChainItemFinalizedEvent&)>  on_chain_item_finalized;
    boost::signals2::signal<void(const ChainItemResolvedEvent&)>   on_chain_item_resolved;
    boost::signals2::signal<void(const ChainEmptiedEvent&)>        on_chain_emptied;
    boost::signals2::signal<void(const SpellResolvedEvent&)>       on_spell_resolved;
    boost::signals2::signal<void(const CardRevealedEvent&)>        on_card_revealed;

    // Convenience emit methods
    void emit(const UnitMovedEvent& e)          { on_unit_moved(e); }
    void emit(const EnteredBoardEvent& e)       { on_entered_board(e); }
    void emit(const TokenCreatedEvent& e)      { on_token_created(e); }
    void emit(const LeftBoardEvent& e)          { on_left_board(e); }
    void emit(const DamageDealtEvent& e)        { on_damage_dealt(e); }
    void emit(const UnitDiedEvent& e)           { on_unit_died(e); }
    void emit(const CardsDrawnEvent& e)         { on_cards_drawn(e); }
    void emit(const CardsDiscardedEvent& e)     { on_cards_discarded(e); }
    void emit(const CardPlayedEvent& e)         { on_card_played(e); }
    void emit(const RuneChanneledEvent& e)      { on_rune_channeled(e); }
    void emit(const ControlChangedEvent& e)     { on_control_changed(e); }
    void emit(const ContestedEvent& e)          { on_contested(e); }
    void emit(const ScoreEvent& e)              { on_score(e); }
    void emit(const PointsGainedEvent& e)       { on_points_gained(e); }
    void emit(const PhaseChangedEvent& e)       { on_phase_changed(e); }
    void emit(const PriorityGrantedEvent& e)    { on_priority_granted(e); }
    void emit(const CombatStartedEvent& e)      { on_combat_started(e); }
    void emit(const CombatEndedEvent& e)        { on_combat_ended(e); }
    void emit(const ShowdownStartedEvent& e)    { on_showdown_started(e); }
    void emit(const ShowdownEndedEvent& e)      { on_showdown_ended(e); }
    void emit(const CleanupNeededEvent& e)      { on_cleanup_needed(e); }
    void emit(const ObjectStateChangedEvent& e) { on_object_state_changed(e); }
    void emit(const GameOverEvent& e)           { on_game_over(e); }
    void emit(const LogEvent& e)                  { on_log(e); }
    void emit(const ChainCreatedEvent& e)        { on_chain_created(e); }
    void emit(const ChainItemFinalizedEvent& e)  { on_chain_item_finalized(e); }
    void emit(const ChainItemResolvedEvent& e)   { on_chain_item_resolved(e); }
    void emit(const ChainEmptiedEvent& e)        { on_chain_emptied(e); }
    void emit(const SpellResolvedEvent& e)       { on_spell_resolved(e); }
    void emit(const CardRevealedEvent& e)        { on_card_revealed(e); }

    // Convenience logging
    void logTrace(const std::string& msg) { emit(LogEvent{LogLevel::Trace, msg}); }
    void logDebug(const std::string& msg) { emit(LogEvent{LogLevel::Debug, msg}); }
    void logInfo(const std::string& msg)  { emit(LogEvent{LogLevel::Info, msg}); }
    void logWarn(const std::string& msg)  { emit(LogEvent{LogLevel::Warning, msg}); }

    /// Disconnect all subscribers (useful between games).
    void disconnectAll() {
        on_unit_moved.disconnect_all_slots();
        on_entered_board.disconnect_all_slots();
        on_token_created.disconnect_all_slots();
        on_left_board.disconnect_all_slots();
        on_damage_dealt.disconnect_all_slots();
        on_unit_died.disconnect_all_slots();
        on_cards_drawn.disconnect_all_slots();
        on_cards_discarded.disconnect_all_slots();
        on_card_played.disconnect_all_slots();
        on_rune_channeled.disconnect_all_slots();
        on_control_changed.disconnect_all_slots();
        on_contested.disconnect_all_slots();
        on_score.disconnect_all_slots();
        on_points_gained.disconnect_all_slots();
        on_phase_changed.disconnect_all_slots();
        on_priority_granted.disconnect_all_slots();
        on_combat_started.disconnect_all_slots();
        on_combat_ended.disconnect_all_slots();
        on_showdown_started.disconnect_all_slots();
        on_showdown_ended.disconnect_all_slots();
        on_cleanup_needed.disconnect_all_slots();
        on_object_state_changed.disconnect_all_slots();
        on_game_over.disconnect_all_slots();
        on_chain_created.disconnect_all_slots();
        on_chain_item_finalized.disconnect_all_slots();
        on_chain_item_resolved.disconnect_all_slots();
        on_chain_emptied.disconnect_all_slots();
        on_spell_resolved.disconnect_all_slots();
        on_card_revealed.disconnect_all_slots();
        on_log.disconnect_all_slots();
    }
};

} // namespace riftbound
