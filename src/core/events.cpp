#include "core/events.h"

#include <cstdio>

namespace riftbound {

// ── emit ─────────────────────────────────────────────────────────────────────
// One overload per event type; each invokes that event's signal. Kept out of
// the header on purpose (see the note on EventBus).
void EventBus::emit(const UnitMovedEvent& e) { on_unit_moved(e); }
void EventBus::emit(const EnteredBoardEvent& e) { on_entered_board(e); }
void EventBus::emit(const TokenCreatedEvent& e) { on_token_created(e); }
void EventBus::emit(const LeftBoardEvent& e) { on_left_board(e); }
void EventBus::emit(const DamageDealtEvent& e) { on_damage_dealt(e); }
void EventBus::emit(const UnitDiedEvent& e) { on_unit_died(e); }
void EventBus::emit(const CardsDrawnEvent& e) { on_cards_drawn(e); }
void EventBus::emit(const CardsDiscardedEvent& e) { on_cards_discarded(e); }
void EventBus::emit(const CardPlayedEvent& e) { on_card_played(e); }
void EventBus::emit(const RuneChanneledEvent& e) { on_rune_channeled(e); }
void EventBus::emit(const ControlChangedEvent& e) { on_control_changed(e); }
void EventBus::emit(const ContestedEvent& e) { on_contested(e); }
void EventBus::emit(const ScoreEvent& e) { on_score(e); }
void EventBus::emit(const PointsGainedEvent& e) { on_points_gained(e); }
void EventBus::emit(const PhaseChangedEvent& e) { on_phase_changed(e); }
void EventBus::emit(const PriorityGrantedEvent& e) { on_priority_granted(e); }
void EventBus::emit(const CombatStartedEvent& e) { on_combat_started(e); }
void EventBus::emit(const CombatEndedEvent& e) { on_combat_ended(e); }
void EventBus::emit(const ShowdownStartedEvent& e) { on_showdown_started(e); }
void EventBus::emit(const ShowdownEndedEvent& e) { on_showdown_ended(e); }
void EventBus::emit(const StateSettledEvent& e) { on_state_settled(e); }
void EventBus::emit(const ObjectStateChangedEvent& e) { on_object_state_changed(e); }
void EventBus::emit(const UnitStunnedEvent& e) { on_unit_stunned(e); }
void EventBus::emit(const UnitReadiedEvent& e) { on_unit_readied(e); }
void EventBus::emit(const CardHiddenEvent& e) { on_card_hidden(e); }
void EventBus::emit(const PlayedFromFacedownEvent& e) { on_played_from_facedown(e); }
void EventBus::emit(const UnitReturnedToHandEvent& e) { on_unit_returned_to_hand(e); }
void EventBus::emit(const GameOverEvent& e) { on_game_over(e); }
void EventBus::emit(const LogEvent& e) { on_log(e); }
void EventBus::emit(const ChainCreatedEvent& e) { on_chain_created(e); }
void EventBus::emit(const ChainItemFinalizedEvent& e) { on_chain_item_finalized(e); }
void EventBus::emit(const ChainItemResolvedEvent& e) { on_chain_item_resolved(e); }
void EventBus::emit(const ChainEmptiedEvent& e) { on_chain_emptied(e); }
void EventBus::emit(const SpellResolvedEvent& e) { on_spell_resolved(e); }
void EventBus::emit(const CardRevealedEvent& e) { on_card_revealed(e); }

// ── logging ──────────────────────────────────────────────────────────────────
void EventBus::logTrace(const std::string& msg) { emit(LogEvent{LogLevel::Trace, tagWithDecision(msg)}); }
void EventBus::logDebug(const std::string& msg) { emit(LogEvent{LogLevel::Debug, tagWithDecision(msg)}); }
void EventBus::logInfo(const std::string& msg) { emit(LogEvent{LogLevel::Info, tagWithDecision(msg)}); }
void EventBus::logWarn(const std::string& msg) { emit(LogEvent{LogLevel::Warning, tagWithDecision(msg)}); }
void EventBus::logTracePrivate(PlayerId visible_to, const std::string& msg) {
    emit(LogEvent{LogLevel::Trace, tagWithDecision(msg), LogAudience::only(visible_to)});
}
void EventBus::logTraceFor(LogAudience audience, const std::string& msg) {
    logFor(LogLevel::Trace, audience, msg);
}
void EventBus::logFor(LogLevel level, LogAudience audience, const std::string& msg) {
    emit(LogEvent{level, tagWithDecision(msg), audience});
}

void EventBus::disconnectAll() {
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
    on_state_settled.disconnect_all_slots();
    on_object_state_changed.disconnect_all_slots();
    on_unit_stunned.disconnect_all_slots();
    on_unit_readied.disconnect_all_slots();
    on_card_hidden.disconnect_all_slots();
    on_played_from_facedown.disconnect_all_slots();
    on_unit_returned_to_hand.disconnect_all_slots();
    on_game_over.disconnect_all_slots();
    on_chain_created.disconnect_all_slots();
    on_chain_item_finalized.disconnect_all_slots();
    on_chain_item_resolved.disconnect_all_slots();
    on_chain_emptied.disconnect_all_slots();
    on_spell_resolved.disconnect_all_slots();
    on_card_revealed.disconnect_all_slots();
    on_log.disconnect_all_slots();
}

std::string EventBus::tagWithDecision(const std::string& msg) {
    static constexpr const char* kDecisionPrefix = "DECISION #";
    if (msg.compare(0, 10, kDecisionPrefix) == 0) {
        ++log_decision_counter_;
        log_sub_counter_ = 0;
    } else {
        ++log_sub_counter_;
    }
    char buf[24];
    std::snprintf(buf, sizeof(buf), "[D:%03d.%02d] ",
                  log_decision_counter_, log_sub_counter_);
    return std::string(buf) + msg;
}

} // namespace riftbound
