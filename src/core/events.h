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
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
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
    // Who killed it ("When you kill ..."): the controller of the spell or
    // ability that killed it, else the player whose damage was lethal
    // (GameObject::LastKnown::killed_by). None when nobody did (a rule killed
    // it: its own [Temporary]).
    PlayerId killer = PlayerId::None;
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

    // Total energy "spent" on this play — base energy cost plus any
    // Repeat increments paid (CR 820). Used by Forgotten Library / Virtuoso
    // for the "if you spent [4] or more" gate. For non-spells (units/gear)
    // this is just the base energy_cost.
    int energy_spent = 0;
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
    // Conquer only: who controlled the battlefield before; nullopt = nobody.
    std::optional<PlayerId> previous_controller = std::nullopt;
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

/// A cleanup has run to its fixed point: continuous effects, Might and
/// control are final for the current state. Emitted by GameEngine::cleanup.
/// Drives triggers that watch a computed value cross a line
/// (WhenMyMightReachesThreshold).
struct StateSettledEvent {};

/// What an ObjectStateChangedEvent says changed: the text of a string literal
/// ("exhausted", "buff_spent", ...). It refers to the literal instead of
/// copying it into a std::string — an event is emitted some 300 times a game,
/// and every key longer than 15 characters was a heap allocation per emit and
/// per subscriber copy (plans/06 item 6.3). Compared with `==` as before; a
/// comparison looks at the length first. Only a literal converts to it, so a
/// key cannot outlive its text. Adding a key still needs no header change.
class StateChangeKey {
public:
    template <std::size_t N>
    constexpr StateChangeKey(const char (&literal)[N]) : text_(literal, N - 1) {}   // NOLINT
    constexpr std::string_view view() const { return text_; }
    std::string str() const { return std::string(text_); }
    friend constexpr bool operator==(StateChangeKey a, std::string_view b) { return a.text_ == b; }

private:
    std::string_view text_;
};

/// A game object's state changed (exhausted, readied, buffed, etc).
struct ObjectStateChangedEvent {
    GameObjectId object;
    StateChangeKey what_changed; // "exhausted", "readied", "buffed", "damaged", etc.
};

/// A unit just became stunned. Emitted by EffectExecutor::stunUnit.
/// Drives the WhenYouStun trigger (used by Vex Mocking etc.). The
/// generic ObjectStateChangedEvent fires too for backwards compat,
/// but trigger matching prefers the typed event.
struct UnitStunnedEvent {
    GameObjectId stunned_unit;
    PlayerId     stunner;        // controller of the source that applied the stun
    PlayerId     victim_controller;
};

/// A unit/object was readied. Emitted by EffectExecutor::readyObject.
/// Drives WhenIAmReadied (Irelia, Fervent).
struct UnitReadiedEvent {
    GameObjectId object;
    PlayerId     controller;
};

/// A card was hidden face-down. Emitted by GameEngine::executeHideCard.
/// Drives WhenYouHideACard (Katarina, Reckless).
struct CardHiddenEvent {
    GameObjectId card;
    PlayerId     player;         // the player who hid the card
};

/// A face-down card was played (revealed and resolved). Emitted by the
/// facedown reveal-play path. Drives WhenYouPlayFromFacedown (Katarina).
struct PlayedFromFacedownEvent {
    GameObjectId card;
    PlayerId     player;
};

/// A unit at a battlefield was returned to its owner's hand. Emitted by
/// EffectExecutor::bounceToHand when the unit left a battlefield. Drives
/// WhenAUnitReturnsToHandHere (Ripper's Bay). `from_battlefield` is the BF
/// the unit was at (kInvalidId if it wasn't at a battlefield).
struct UnitReturnedToHandEvent {
    GameObjectId object;
    PlayerId     owner;
    BattlefieldId from_battlefield;
};

/// The game ended.
struct GameOverEvent {
    PlayerId winner; // PlayerId::None for draw
    std::string reason;
};

// ─── Debug logging ──────────────────────────────────────────────────────────

enum class LogLevel : uint8_t { Trace, Debug, Info, Warning };

/// Which players may read a log line: both (the default — the line says only
/// what both players can see happen), exactly one (it names a card only that
/// player has seen: a draw, a mulligan, a private look at a deck), or neither
/// (it states something no player knows, e.g. the order a shuffle produced;
/// for a debugging trace or an omniscient replay only).
struct LogAudience {
    bool player1 = true;
    bool player2 = true;

    static LogAudience both() { return {true, true}; }
    static LogAudience neither() { return {false, false}; }
    static LogAudience only(PlayerId p) {
        return {p == PlayerId::Player1, p == PlayerId::Player2};
    }
    bool includes(PlayerId p) const {
        return p == PlayerId::Player1 ? player1 : p == PlayerId::Player2 ? player2 : false;
    }
    bool isBoth() const { return player1 && player2; }
    bool isNeither() const { return !player1 && !player2; }
};

struct LogEvent {
    LogLevel level;
    std::string message;
    // Who may read the line. A consumer showing the log to one seat (the play
    // server in fair-play mode) or building a per-observer history forwards a
    // line to a player only if `audience.includes(player)`.
    LogAudience audience = LogAudience::both();
    bool visibleTo(PlayerId p) const { return audience.includes(p); }
    bool isPublic() const { return audience.isBoth(); }
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

/// The log signal: a Boost.Signals2 signal that can also say, without going
/// through Boost's slot list (a mutex and a shared_ptr copy per ask), whether
/// anyone is connected. `bus.on_log.connect(fn)` is unchanged and returns the
/// connection. A slot disconnected through its connection — or a
/// scoped_connection going out of scope — is noticed the next time
/// listening() is asked, so the answer is never stale.
class LogSignal {
public:
    template <class Slot>
    boost::signals2::connection connect(Slot&& slot) {
        boost::signals2::connection c = signal_.connect(std::forward<Slot>(slot));
        connections_.push_back(c);
        return c;
    }
    void operator()(const LogEvent& e) const;
    void disconnect_all_slots();
    std::size_t num_slots() const;
    bool empty() const { return !listening(); }
    /// True while at least one slot is connected.
    bool listening() const { return !connections_.empty() && anyConnected(); }

private:
    bool anyConnected() const;   // forgets the connections that have ended
    boost::signals2::signal<void(const LogEvent&)> signal_;
    mutable std::vector<boost::signals2::connection> connections_;
};

// Member functions are defined in events.cpp, not inline: an inline body that
// calls a signal makes every translation unit that merely includes this header
// instantiate Boost.Signals2's invocation machinery for that signal (GCC queues
// those instantiations when it parses the body). That was most of a card
// file's compile time.
class EventBus {
public:
    // Signals — subscribers connect to these. Every signal here has an
    // emitter in src/. Several have no subscriber in src/ (on_cards_discarded,
    // on_rune_channeled, on_control_changed, on_contested, on_points_gained,
    // on_priority_granted, on_showdown_ended, on_game_over, the on_chain_*
    // four): they are the published event surface for tests, loggers and
    // external tools, not dead code. A signal nothing emits does not belong
    // here.
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
    boost::signals2::signal<void(const StateSettledEvent&)>       on_state_settled;
    boost::signals2::signal<void(const ObjectStateChangedEvent&)> on_object_state_changed;
    boost::signals2::signal<void(const UnitStunnedEvent&)>        on_unit_stunned;
    boost::signals2::signal<void(const UnitReadiedEvent&)>        on_unit_readied;
    boost::signals2::signal<void(const CardHiddenEvent&)>         on_card_hidden;
    boost::signals2::signal<void(const PlayedFromFacedownEvent&)> on_played_from_facedown;
    boost::signals2::signal<void(const UnitReturnedToHandEvent&)> on_unit_returned_to_hand;
    boost::signals2::signal<void(const GameOverEvent&)>           on_game_over;

    // Logging. A LogSignal, so that "is anyone reading the log?" is a cheap
    // question (logging(), below); subscribers connect to it as to any signal.
    LogSignal                                                      on_log;

    // Chain signals
    boost::signals2::signal<void(const ChainCreatedEvent&)>        on_chain_created;
    boost::signals2::signal<void(const ChainItemFinalizedEvent&)>  on_chain_item_finalized;
    boost::signals2::signal<void(const ChainItemResolvedEvent&)>   on_chain_item_resolved;
    boost::signals2::signal<void(const ChainEmptiedEvent&)>        on_chain_emptied;
    boost::signals2::signal<void(const SpellResolvedEvent&)>       on_spell_resolved;
    boost::signals2::signal<void(const CardRevealedEvent&)>        on_card_revealed;

    // Convenience emit methods
    void emit(const UnitMovedEvent& e);
    void emit(const EnteredBoardEvent& e);
    void emit(const TokenCreatedEvent& e);
    void emit(const LeftBoardEvent& e);
    void emit(const DamageDealtEvent& e);
    void emit(const UnitDiedEvent& e);
    void emit(const CardsDrawnEvent& e);
    void emit(const CardsDiscardedEvent& e);
    void emit(const CardPlayedEvent& e);
    void emit(const RuneChanneledEvent& e);
    void emit(const ControlChangedEvent& e);
    void emit(const ContestedEvent& e);
    void emit(const ScoreEvent& e);
    void emit(const PointsGainedEvent& e);
    void emit(const PhaseChangedEvent& e);
    void emit(const PriorityGrantedEvent& e);
    void emit(const CombatStartedEvent& e);
    void emit(const CombatEndedEvent& e);
    void emit(const ShowdownStartedEvent& e);
    void emit(const ShowdownEndedEvent& e);
    void emit(const StateSettledEvent& e);
    void emit(const ObjectStateChangedEvent& e);
    void emit(const UnitStunnedEvent& e);
    void emit(const UnitReadiedEvent& e);
    void emit(const CardHiddenEvent& e);
    void emit(const PlayedFromFacedownEvent& e);
    void emit(const UnitReturnedToHandEvent& e);
    void emit(const GameOverEvent& e);
    void emit(const LogEvent& e);
    void emit(const ChainCreatedEvent& e);
    void emit(const ChainItemFinalizedEvent& e);
    void emit(const ChainItemResolvedEvent& e);
    void emit(const ChainEmptiedEvent& e);
    void emit(const SpellResolvedEvent& e);
    void emit(const CardRevealedEvent& e);

    // ── Holding ──
    // While held, nothing emitted (log lines included) reaches a subscriber:
    // it is kept, in order. release() delivers it all; discardHeld() drops it.
    // For an action whose outcome is not known until it has run and that must
    // leave no trace if it turns out not to happen (GameEngine::
    // executePlaySpell: a spell whose choices did not make it payable).
    void hold();
    void release();
    void discardHeld();
    bool holding() const { return holding_; }
    /// A stretch whose emissions are dropped, whatever the bus was doing
    /// before (it may itself be inside a hold): for running game code only to
    /// see what it would do (GameEngine's pre-play probes).
    struct Quiet { bool was_holding; size_t held_before; };
    Quiet beginQuiet();
    void endQuiet(const Quiet& q);

    // Convenience logging. Every line is prefixed with [D:NNN.NN]
    // where NNN is a per-EventBus decision counter (bumped + reset
    // sub-counter whenever a "DECISION #" line is emitted) and NN is
    // the sub-counter within the current decision. Lines emitted
    // before the first DECISION show as [D:000.NN].
    //
    // A line nobody will read is not built (plans/06 item 6.1): call sites
    // use the RIFT_TRACE / RIFT_DEBUG / ... macros at the end of this file,
    // which evaluate their message only while logging() is true. The
    // functions below may still be called directly; with nobody listening
    // they format nothing and emit nothing. Either way the two counters
    // advance exactly as if the line had been delivered, so a listener that
    // attaches in mid-game sees the prefixes it would have seen from the
    // start.
    /// Whether a log line emitted now would reach a subscriber: someone is
    /// connected to on_log and the bus is not in a quiet stretch.
    bool logging() const { return quiet_depth_ == 0 && on_log.listening(); }
    /// The counter bookkeeping of a line that was not built: any line, and a
    /// "DECISION #" line (which starts a new decision).
    /// Advances with every log line, heard or not (the [D:NNN.NN] counters):
    /// equal stamps mean the engine has logged nothing in between.
    uint64_t lineStamp() const {
        return (static_cast<uint64_t>(static_cast<uint32_t>(log_decision_counter_)) << 32) |
               static_cast<uint32_t>(log_sub_counter_);
    }
    void noteUnheardLine() { ++log_sub_counter_; ++activity_; }
    void noteUnheardDecision() { ++log_decision_counter_; log_sub_counter_ = 0; ++activity_; }
    /// Advances with everything the bus is told: every event emitted (kept,
    /// dropped in a quiet stretch or delivered — and again when a held event
    /// is delivered, since that is when its subscribers act) and every log
    /// line, heard or not. Equal values mean nothing was announced in
    /// between; GameEngine::cleanup uses it to recognise a Cleanup that
    /// directly follows one that had nothing to do.
    uint64_t activity() const { return activity_; }
    void logTrace(const std::string& msg);
    void logDebug(const std::string& msg);
    void logInfo(const std::string& msg);
    void logWarn(const std::string& msg);
    /// A trace line only `visible_to` may read (LogAudience::only): it names
    /// a card the other player has not seen.
    void logTracePrivate(PlayerId visible_to, const std::string& msg);
    /// A trace line for an explicit audience (LogAudience::neither() for what
    /// no player knows).
    void logTraceFor(LogAudience audience, const std::string& msg);
    void logFor(LogLevel level, LogAudience audience, const std::string& msg);

    /// Disconnect all subscribers (useful between games).
    void disconnectAll();

private:
    bool holding_ = false;
    int quiet_depth_ = 0;   // > 0 inside beginQuiet / endQuiet: emissions are dropped
    std::vector<std::function<void()>> held_;
    int log_decision_counter_ = 0;
    int log_sub_counter_      = 0;
    uint64_t activity_        = 0;   // see activity()

    /// Prefix the message with `[D:NNN.NN] `. Bumps the decision
    /// counter (and resets the sub-counter to 0) when `msg` begins
    /// with `"DECISION #"`; otherwise bumps the sub-counter.
    std::string tagWithDecision(const std::string& msg);
    static bool isDecisionLine(const std::string& msg);
};

} // namespace riftbound

// ─── Lazy log lines (plans/06 item 6.1) ──────────────────────────────────────
// RIFT_TRACE(bus, "MOVE: " + unit.name + ...) is bus.logTrace(...) whose
// message expression is evaluated only while bus.logging() is true. Most games
// (training, search, batches) run with nobody reading the log; building a
// string per line for them was a measurable share of every decision.
//
// Rules for a message expression: it must have no side effects (it does not
// run when nobody listens), and a line that starts with "DECISION #" must use
// RIFT_TRACE_DECISION — the bus numbers its lines per decision and has to be
// told about a decision line it never sees the text of.
#define RIFT_LOG_LAZY_(bus, call, unheard)                           \
    do {                                                             \
        ::riftbound::EventBus& rift_log_bus_ = (bus);                \
        if (rift_log_bus_.logging()) rift_log_bus_.call;             \
        else rift_log_bus_.unheard();                                \
    } while (0)
#define RIFT_TRACE(bus, ...)          RIFT_LOG_LAZY_(bus, logTrace(__VA_ARGS__), noteUnheardLine)
#define RIFT_TRACE_DECISION(bus, ...) RIFT_LOG_LAZY_(bus, logTrace(__VA_ARGS__), noteUnheardDecision)
#define RIFT_DEBUG(bus, ...)          RIFT_LOG_LAZY_(bus, logDebug(__VA_ARGS__), noteUnheardLine)
#define RIFT_INFO(bus, ...)           RIFT_LOG_LAZY_(bus, logInfo(__VA_ARGS__), noteUnheardLine)
#define RIFT_WARN(bus, ...)           RIFT_LOG_LAZY_(bus, logWarn(__VA_ARGS__), noteUnheardLine)
/// (bus, visible_to, message) — see EventBus::logTracePrivate.
#define RIFT_TRACE_PRIVATE(bus, ...)  RIFT_LOG_LAZY_(bus, logTracePrivate(__VA_ARGS__), noteUnheardLine)
/// (bus, audience, message) — see EventBus::logTraceFor.
#define RIFT_TRACE_FOR(bus, ...)      RIFT_LOG_LAZY_(bus, logTraceFor(__VA_ARGS__), noteUnheardLine)
/// (bus, level, audience, message) — see EventBus::logFor.
#define RIFT_LOG_FOR(bus, ...)        RIFT_LOG_LAZY_(bus, logFor(__VA_ARGS__), noteUnheardLine)
