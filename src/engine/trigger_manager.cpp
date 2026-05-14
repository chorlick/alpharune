#include "trigger_manager.h"
#include "cards/card.h"
#include "engine/effect_executor.h"

namespace riftbound {

TriggerManager::TriggerManager(GameState& state, EventBus& events,
                               const CardDB& card_db, ChainManager& chain,
                               const CardRegistry& card_registry)
    : state_(state), events_(events), card_db_(card_db), chain_(chain),
      card_registry_(card_registry) {}

void TriggerManager::subscribe() {
    events_.on_card_played.connect(
        [this](const CardPlayedEvent& e) { onCardPlayed(e); });
    events_.on_entered_board.connect(
        [this](const EnteredBoardEvent& e) { onEnteredBoard(e); });
    events_.on_combat_started.connect(
        [this](const CombatStartedEvent& e) { onCombatStarted(e); });
    events_.on_unit_died.connect(
        [this](const UnitDiedEvent& e) { onUnitDied(e); });
    events_.on_score.connect(
        [this](const ScoreEvent& e) { onScore(e); });
    events_.on_unit_moved.connect(
        [this](const UnitMovedEvent& e) { onUnitMoved(e); });
    events_.on_phase_changed.connect(
        [this](const PhaseChangedEvent& e) { onPhaseChanged(e); });
}

// ═══════════════════════════════════════════════════════════════════════════════
// Delayed abilities (CR 389-392)
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::checkDelayedAbilities(TriggerType trigger,
                                            PlayerId relevant_player) {
    for (auto& da : state_.delayed_abilities) {
        if (da.fired) continue;
        if (da.trigger != trigger) continue;
        if (da.controller != relevant_player) continue;

        // Fire this delayed ability
        da.fired = true;
        events_.logTrace("DELAYED_ABILITY: firing for " +
                         (state_.objectExists(da.source) ? state_.getObject(da.source).name : "?"));
        chain_.addAbility(da.source, da.controller, da.card_def_id);
    }

    // Remove fired abilities
    state_.delayed_abilities.erase(
        std::remove_if(state_.delayed_abilities.begin(), state_.delayed_abilities.end(),
            [](const DelayedAbility& da) { return da.fired; }),
        state_.delayed_abilities.end());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Equipment triggers — check attached gear when unit events fire
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::fireEquippedTriggers(GameObjectId unit_id, PlayerId controller,
                                           TriggerType trigger) {
    if (!state_.objectExists(unit_id)) return;
    auto& unit = state_.getObject(unit_id);

    for (auto gear_id : unit.attachments) {
        if (!state_.objectExists(gear_id)) continue;
        auto& gear = state_.getObject(gear_id);

        Card* gear_card = card_registry_.get(gear.card_def_id);
        if (!gear_card) continue;
        if (gear_card->equippedTriggerType() != trigger) continue;

        events_.logTrace("EQUIP_TRIGGER: " + gear.name + " fires on " +
                         unit.name + " (" + toString(controller) + ")");

        if (effect_executor_) {
            CardContext ctx{state_, events_, *effect_executor_, controller, gear_id};
            gear_card->onEquippedTrigger(ctx, unit_id, {});
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helper — get trigger type from Card object
// ═══════════════════════════════════════════════════════════════════════════════

static TriggerType getTrigger(const CardRegistry& reg, CardDefId def_id) {
    Card* card = reg.get(def_id);
    return card ? card->triggerType() : TriggerType::None;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helper
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::fireTrigger(GameObjectId source, PlayerId controller) {
    if (!state_.objectExists(source)) return;
    auto& obj = state_.getObject(source);

    // Skip triggers on gear with inactive rules text (CR 718.2)
    if (obj.is_rules_text_inactive) return;

    auto trigger = getTrigger(card_registry_, obj.card_def_id);
    if (trigger == TriggerType::None) return;

    events_.logDebug("TRIGGER: " + obj.name + " (" + toString(controller) +
                     ") fires ability");
    chain_.addAbility(source, controller, obj.card_def_id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Event handlers
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::onCardPlayed(const CardPlayedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    // Vision keyword: "When you play me, look at top card, may recycle it"
    if (obj.keywords.has(Keyword::Vision) && obj.isPermanent()) {
        auto& ps = state_.player(e.player);
        if (!ps.main_deck.empty()) {
            auto top_card = ps.main_deck.back();
            auto& top_obj = state_.getObject(top_card);
            if (top_obj.isSpell()) {
                ps.main_deck.pop_back();
                ps.main_deck.insert(ps.main_deck.begin(), top_card);
            }
        }
    }

    // "When you play me" — fire on the card that was just played
    auto trigger = getTrigger(card_registry_, obj.card_def_id);
    if (trigger == TriggerType::WhenYouPlayMe ||
        trigger == TriggerType::AsYouPlayMe) {
        if (obj.isPermanent()) {
            fireTrigger(e.object, e.player);
        }
    }

    // "When you play a unit" / "When you play a spell" — fire on OTHER objects
    for (auto& [id, other] : state_.objects) {
        if (!other.location.has_value()) continue;
        if (other.controller != e.player) continue;
        if (id == e.object) continue;

        auto other_trigger = getTrigger(card_registry_, other.card_def_id);
        if (other_trigger == TriggerType::WhenYouPlayAUnit &&
            e.card_type == CardType::Unit) {
            fireTrigger(id, e.player);
        }
        if (other_trigger == TriggerType::WhenYouPlayASpell &&
            e.card_type == CardType::Spell) {
            fireTrigger(id, e.player);
        }
    }
}

void TriggerManager::onEnteredBoard(const EnteredBoardEvent& e) {
    if (!e.from_play) return;
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    auto trigger = getTrigger(card_registry_, obj.card_def_id);
    if (trigger == TriggerType::WhenYouPlayThis) {
        fireTrigger(e.object, e.controller);
    }
}

void TriggerManager::onCombatStarted(const CombatStartedEvent& e) {
    auto bf_loc = BattlefieldLocation{e.battlefield};

    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
        auto unit_bf = obj.battlefieldId();
        if (!unit_bf || *unit_bf != e.battlefield) continue;

        auto trigger = getTrigger(card_registry_, obj.card_def_id);

        bool is_attacker = (obj.controller == e.attacker);
        bool is_defender = (obj.controller == e.defender);

        if (trigger == TriggerType::WhenIAttack && is_attacker) {
            fireTrigger(id, obj.controller);
        }
        if (trigger == TriggerType::WhenIDefend && is_defender) {
            fireTrigger(id, obj.controller);
        }
        if (trigger == TriggerType::WhenIAttackOrDefend &&
            (is_attacker || is_defender)) {
            fireTrigger(id, obj.controller);
        }

        // Check attached gear for combat triggers
        if (is_attacker) {
            fireEquippedTriggers(id, obj.controller, TriggerType::WhenIAttack);
            fireEquippedTriggers(id, obj.controller, TriggerType::WhenIAttackOrDefend);
        }
        if (is_defender) {
            fireEquippedTriggers(id, obj.controller, TriggerType::WhenIDefend);
            fireEquippedTriggers(id, obj.controller, TriggerType::WhenIAttackOrDefend);
        }
    }
}

void TriggerManager::onUnitDied(const UnitDiedEvent& e) {
    if (state_.objectExists(e.object)) {
        auto& obj = state_.getObject(e.object);
        auto trigger = getTrigger(card_registry_, obj.card_def_id);

        if (trigger == TriggerType::WhenIDie) {
            chain_.addAbility(e.object, e.controller, obj.card_def_id);
        }
    }

    // "When a friendly unit dies" — fire on other objects the controller owns
    for (auto& [id, other] : state_.objects) {
        if (!other.location.has_value()) continue;
        if (other.controller != e.controller) continue;
        if (id == e.object) continue;

        auto other_trigger = getTrigger(card_registry_, other.card_def_id);
        if (other_trigger == TriggerType::WhenAFriendlyUnitDies) {
            fireTrigger(id, e.controller);
        }
    }
}

void TriggerManager::onScore(const ScoreEvent& e) {
    auto bf_loc = BattlefieldLocation{e.battlefield};
    auto units = state_.unitsAt(bf_loc, e.player);

    for (auto uid : units) {
        if (!state_.objectExists(uid)) continue;
        auto& obj = state_.getObject(uid);
        auto trigger = getTrigger(card_registry_, obj.card_def_id);

        if (trigger == TriggerType::WhenIConquer &&
            e.method == ScoreMethod::Conquer) {
            fireTrigger(uid, e.player);
        }
        if (trigger == TriggerType::WhenIHold &&
            e.method == ScoreMethod::Hold) {
            fireTrigger(uid, e.player);
        }
        if (trigger == TriggerType::WhenIConquerOrHold) {
            fireTrigger(uid, e.player);
        }

        // Check attached gear for score triggers
        if (e.method == ScoreMethod::Conquer) {
            fireEquippedTriggers(uid, e.player, TriggerType::WhenIConquer);
            fireEquippedTriggers(uid, e.player, TriggerType::WhenIConquerOrHold);
        }
        if (e.method == ScoreMethod::Hold) {
            fireEquippedTriggers(uid, e.player, TriggerType::WhenIHold);
            fireEquippedTriggers(uid, e.player, TriggerType::WhenIConquerOrHold);
        }
    }

    // Battlefield triggers
    for (auto& bf : state_.battlefields) {
        if (bf.id != e.battlefield) continue;
        if (!state_.objectExists(bf.card_object_id)) continue;

        auto trigger = getTrigger(card_registry_,
            state_.getObject(bf.card_object_id).card_def_id);

        if (trigger == TriggerType::WhenYouConquerHere &&
            e.method == ScoreMethod::Conquer) {
            fireTrigger(bf.card_object_id, e.player);
        }
        if (trigger == TriggerType::WhenYouHoldHere &&
            e.method == ScoreMethod::Hold) {
            fireTrigger(bf.card_object_id, e.player);
        }
        if (trigger == TriggerType::WhenYouScoreHere) {
            fireTrigger(bf.card_object_id, e.player);
        }
    }
}

void TriggerManager::onUnitMoved(const UnitMovedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);
    auto trigger = getTrigger(card_registry_, obj.card_def_id);

    if (trigger == TriggerType::WhenIMove) {
        fireTrigger(e.object, e.controller);
    }
    if (trigger == TriggerType::WhenIMoveToFB &&
        std::holds_alternative<BattlefieldLocation>(e.to)) {
        fireTrigger(e.object, e.controller);
    }

    // Check attached gear for move triggers
    fireEquippedTriggers(e.object, e.controller, TriggerType::WhenIMove);
    if (std::holds_alternative<BattlefieldLocation>(e.to)) {
        fireEquippedTriggers(e.object, e.controller, TriggerType::WhenIMoveToFB);
    }

    // Fan-out: other friendly objects with WhenAFriendlyUnitMovesToFB.
    // Used by cards like Miss Fortune, Captain that watch all friendly moves.
    if (std::holds_alternative<BattlefieldLocation>(e.to)) {
        std::vector<GameObjectId> watchers;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object) continue;  // skip the moving unit itself
            if (other.controller != e.controller) continue;
            if (!other.location.has_value()) continue;
            if (other.card_def_id == kInvalidId) continue;  // skip tokens
            if (getTrigger(card_registry_, other.card_def_id)
                    == TriggerType::WhenAFriendlyUnitMovesToFB) {
                watchers.push_back(id);
            }
        }
        for (auto wid : watchers) fireTrigger(wid, e.controller);
    }
}

void TriggerManager::onPhaseChanged(const PhaseChangedEvent& e) {
    // "At the end of your turn"
    if (e.new_phase == TurnPhase::EndingStep) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            auto trigger = getTrigger(card_registry_, obj.card_def_id);
            if (trigger == TriggerType::AtEndOfTurn) {
                fireTrigger(id, e.turn_player);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            auto trigger = getTrigger(card_registry_,
                state_.getObject(ps.legend_zone).card_def_id);
            if (trigger == TriggerType::AtEndOfTurn) {
                fireTrigger(ps.legend_zone, e.turn_player);
            }
        }
    }

    // "At the start of your Beginning Phase"
    if (e.new_phase == TurnPhase::BeginningStep) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            auto trigger = getTrigger(card_registry_, obj.card_def_id);
            if (trigger == TriggerType::AtStartOfBeginning) {
                fireTrigger(id, e.turn_player);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            auto trigger = getTrigger(card_registry_,
                state_.getObject(ps.legend_zone).card_def_id);
            if (trigger == TriggerType::AtStartOfBeginning) {
                fireTrigger(ps.legend_zone, e.turn_player);
            }
        }
    }
}

} // namespace riftbound
