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
    events_.on_combat_ended.connect(
        [this](const CombatEndedEvent& e) { onCombatEnded(e); });
    events_.on_unit_died.connect(
        [this](const UnitDiedEvent& e) { onUnitDied(e); });
    events_.on_score.connect(
        [this](const ScoreEvent& e) { onScore(e); });
    events_.on_unit_moved.connect(
        [this](const UnitMovedEvent& e) { onUnitMoved(e); });
    events_.on_phase_changed.connect(
        [this](const PhaseChangedEvent& e) { onPhaseChanged(e); });
    events_.on_unit_stunned.connect(
        [this](const UnitStunnedEvent& e) { onUnitStunned(e); });
}

void TriggerManager::onUnitStunned(const UnitStunnedEvent& e) {
    // WhenYouStun fires for cards controlled by the stunner when the
    // stunned unit's controller is the opposite side (you can't trigger
    // "stun an enemy" by stunning your own unit). Mirrors the pattern in
    // onUnitDied / onCombatEnded.
    if (e.stunner == e.victim_controller) return;  // self-stun, not "enemy"
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != e.stunner) continue;
        if (obj.card_def_id == kInvalidId) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
        const Card* card = card_registry_.get(obj.card_def_id);
        if (!card) continue;
        if (card->triggerType() != TriggerType::WhenYouStun) continue;
        events_.logTrace("TRIGGER: " + obj.name + " WhenYouStun (stunned=" +
                         std::to_string(e.stunned_unit) + ")");
        // Capture the stunned-unit id into card_counters so the trigger's
        // onTrigger can read it post-resolution (combat designations get
        // cleared by the time the chain item resolves — same pattern as
        // __defend_attacker_id).
        obj.card_counters["__stunned_unit_id"] =
            static_cast<int>(e.stunned_unit);
        chain_.addAbility(id, e.stunner, obj.card_def_id);
    }
    checkDelayedAbilities(TriggerType::WhenYouStun, e.stunner, e.stunned_unit);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Delayed abilities (CR 389-392)
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::checkDelayedAbilities(TriggerType trigger,
                                            PlayerId relevant_player,
                                            GameObjectId event_object) {
    for (auto& da : state_.delayed_abilities) {
        if (da.fired) continue;
        if (da.trigger != trigger) continue;
        if (da.controller != relevant_player) continue;
        // Object-scoped delayed abilities (e.g. Deadly Flourish: "when IT
        // dies") fire only for the specific target. kInvalidId = unscoped.
        if (da.target_filter != kInvalidId &&
            da.target_filter != event_object) continue;

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

// Legend cards live in PlayerState::legend_zone, not on the board, so they
// have no `location` and are skipped by every state_.objects iteration that
// gates on it. The card text on legends frequently uses "When you …"
// (player-scoped) phrasing but the engine encodes those as WhenIConquerOrHold
// / WhenIWinCombat / WhenYouPlayASpell on the legend's Card subclass. This
// helper sweeps both players' legend zones and fires the trigger if the
// legend's declared TriggerType matches `t`. `relevant_player` controls
// scoping — for events that only fire for one player (score, combat win),
// pass that player's id; pass PlayerId::None to sweep both legends (e.g.
// "when YOU play a spell" fires only on your legend, but we still need to
// check both legend zones since either player may be the one acting).
void TriggerManager::fireLegendTrigger(TriggerType t, PlayerId relevant_player,
                                        int triggering_spell_energy_spent) {
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        if (relevant_player != PlayerId::None && pid != relevant_player) continue;
        auto legend_id = state_.player(pid).legend_zone;
        if (legend_id == kInvalidId || !state_.objectExists(legend_id)) continue;
        auto& legend = state_.getObject(legend_id);
        auto declared = getTrigger(card_registry_, legend.card_def_id);
        if (declared == t) {
            fireTrigger(legend_id, pid, triggering_spell_energy_spent);
        }
    }
}

void TriggerManager::fireTrigger(GameObjectId source, PlayerId controller,
                                  int triggering_spell_energy_spent) {
    if (!state_.objectExists(source)) return;
    auto& obj = state_.getObject(source);

    // Skip triggers on gear with inactive rules text (CR 718.2)
    if (obj.is_rules_text_inactive) return;

    auto trigger = getTrigger(card_registry_, obj.card_def_id);
    if (trigger == TriggerType::None) return;

    events_.logDebug("TRIGGER: " + obj.name + " (" + toString(controller) +
                     ") fires ability");
    chain_.addAbility(source, controller, obj.card_def_id);
    // Snapshot the triggering spell's spent value on the just-added
    // chain item so resume-time reads aren't subject to clobber by a
    // subsequent spell-play. Phase 6q+ engine-audit follow-on for
    // Virtuoso / Forgotten Library correctness.
    if (triggering_spell_energy_spent > 0 && !state_.chain.items.empty()) {
        state_.chain.items.back().triggering_spell_energy_spent =
            triggering_spell_energy_spent;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Event handlers
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::onCardPlayed(const CardPlayedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    // Vision keyword: "When you play me, look at top card, may recycle it"
    // CR 817.1.b — the recycle decision is a "may" (optional). Phase 6q+:
    // we surface the peek via CardRevealedEvent (so observation tracking
    // works) and auto-recycle spells as a deterministic placeholder for
    // the agent's "yes". Wiring as a true agent-choice would need a
    // synthetic Card-less trigger (keyword has no Card subclass to
    // attach confirmOptional to) — queued as a follow-up. Today's
    // behavior is CR-correct in effect for spells (always recycled);
    // non-spell top cards are left in place (auto-no).
    if (obj.keywords.has(Keyword::Vision) && obj.isPermanent()) {
        auto& ps = state_.player(e.player);
        if (!ps.main_deck.empty()) {
            auto top_card = ps.main_deck.back();
            auto& top_obj = state_.getObject(top_card);
            // Reveal the peeked card to the controller only (private).
            events_.emit(CardRevealedEvent{
                top_card, top_obj.card_def_id, top_obj.owner,
                /*revealed_to_all=*/false, /*revealed_to=*/e.player,
                ZoneType::MainDeck});
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
            // Snapshot triggering-spell spent so Virtuoso /
            // Forgotten Library read the correct value at resolve
            // time (not whichever PlayerState::last_spell_energy_spent
            // is at chain-drain time).
            fireTrigger(id, e.player, e.energy_spent);
        }
    }

    // Legend-zone sweep: legends say "When you play a spell/unit," — the
    // legend itself has no location, so the loop above skips it. Fire any
    // matching legend on the acting player's side.
    if (e.card_type == CardType::Spell) {
        fireLegendTrigger(TriggerType::WhenYouPlayASpell, e.player,
                          e.energy_spent);
    } else if (e.card_type == CardType::Unit) {
        fireLegendTrigger(TriggerType::WhenYouPlayAUnit, e.player);
    }

    // Phase 6o (2026-05-18) — "When you choose a friendly unit."
    // The just-played card's targets are visible on chain.items.back().
    // For each target that is a friendly unit of the playing player,
    // fire WhenYouChooseAFriendlyUnit on all of that player's on-board
    // cards with the trigger (e.g. Spirit Wheel). The triggers are
    // added on top of the just-played item, so they resolve first
    // (LIFO) — meaning Spirit Wheel's draw happens BEFORE the
    // chosen-unit effect resolves, matching the card's text.
    if (!state_.chain.items.empty()) {
        const auto& just_played = state_.chain.items.back();
        bool any_friendly_target = false;
        for (auto tgt_id : just_played.targets) {
            if (!state_.objectExists(tgt_id)) continue;
            const auto& tgt = state_.getObject(tgt_id);
            if (!tgt.isUnit()) continue;
            if (tgt.controller != e.player) continue;
            any_friendly_target = true;
            break;
        }
        if (any_friendly_target) {
            for (auto& [id, other] : state_.objects) {
                if (!other.location.has_value()) continue;
                if (other.controller != e.player) continue;
                if (id == e.object) continue;  // don't self-trigger
                auto other_trigger =
                    getTrigger(card_registry_, other.card_def_id);
                if (other_trigger == TriggerType::WhenYouChooseAFriendlyUnit) {
                    fireTrigger(id, e.player);
                }
            }
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
            // Capture the current attacker UNIT (not just attacker player)
            // onto the defender's card_counters BEFORE adding the ability
            // to the chain. By the time the chain resolves the ability,
            // `combat_designation` may have been cleared (per combat
            // resolution flow), so a card that wants to act on "the
            // attacker that triggered me" can't scan the board — it has
            // to read this captured ID. See `MOverzealousFan::onTrigger`
            // for the consumer.
            for (auto& [att_id, att_obj] : state_.objects) {
                if (!att_obj.isUnit() || !att_obj.isAtBattlefield()) continue;
                auto abf = att_obj.battlefieldId();
                if (!abf || *abf != e.battlefield) continue;
                if (att_obj.controller != e.attacker) continue;
                if (att_obj.combat_designation != CombatDesignation::Attacker) continue;
                obj.card_counters["__defend_attacker_id"] =
                    static_cast<int>(att_id);
                break;
            }
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

void TriggerManager::onCombatEnded(const CombatEndedEvent& e) {
    // No fire on indecisive combat (both sides emptied, or no recall outcome).
    if (!e.winner.has_value()) return;
    PlayerId winner = *e.winner;

    // Legend-zone sweep: legends like Glorious Executioner / Voidreaver use
    // "When you win a combat," text encoded as WhenIWinCombat. Their legend
    // object isn't on the BF, so we need to fire them explicitly here.
    fireLegendTrigger(TriggerType::WhenIWinCombat, winner);

    // Snapshot the surviving winner-side units at this BF. We snapshot first
    // because fireTrigger / fireEquippedTriggers mutate the chain and could
    // re-enter the object map during iteration.
    std::vector<GameObjectId> winners;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit()) continue;
        if (obj.controller != winner) continue;
        auto unit_bf = obj.battlefieldId();
        if (!unit_bf || *unit_bf != e.battlefield) continue;
        winners.push_back(id);
    }

    for (auto uid : winners) {
        if (!state_.objectExists(uid)) continue;
        const auto& obj = state_.getObject(uid);
        auto trigger = getTrigger(card_registry_, obj.card_def_id);
        if (trigger == TriggerType::WhenIWinCombat) {
            fireTrigger(uid, obj.controller);
        }
        // Equipped gear with WhenIWinCombat
        fireEquippedTriggers(uid, obj.controller, TriggerType::WhenIWinCombat);
    }
}

void TriggerManager::onUnitDied(const UnitDiedEvent& e) {
    if (state_.objectExists(e.object)) {
        auto& obj = state_.getObject(e.object);
        auto trigger = getTrigger(card_registry_, obj.card_def_id);

        if (trigger == TriggerType::WhenIDie) {
            // Karthus, Eternal: "Your Deathknell effects trigger an
            // additional time." `deathknell_double_count` is bumped per
            // controlled Karthus by recalculateAuras. Each unit adds
            // (1 + count) chain items. Empty in the default case so the
            // existing single-fire path is unchanged.
            int extra = state_.player(e.controller).deathknell_double_count;
            int fires = 1 + std::max(0, extra);
            for (int i = 0; i < fires; ++i) {
                chain_.addAbility(e.object, e.controller, obj.card_def_id);
            }
            if (extra > 0) {
                events_.logTrace("DEATHKNELL_DOUBLE: " + obj.name + " fires " +
                                 std::to_string(fires) + " times (Karthus aura)");
            }
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

    // Delayed abilities scoped to this dying unit (Deadly Flourish:
    // "When IT dies this turn, ..."). The opposing player created the
    // delayed ability, so we check both controllers' scoped triggers.
    checkDelayedAbilities(TriggerType::WhenIDie, e.controller, e.object);
    checkDelayedAbilities(TriggerType::WhenIDie,
                           opponent(e.controller), e.object);
}

void TriggerManager::onScore(const ScoreEvent& e) {
    auto bf_loc = BattlefieldLocation{e.battlefield};
    auto units = state_.unitsAt(bf_loc, e.player);

    // Legend-zone sweep first: legends like Green Father / Gloomist /
    // Deceiver have "When you conquer or hold," text encoded as the
    // WhenIConquerOrHold trigger on a LegendCard subclass. The legend
    // lives in champion zone, so the unit loop below won't reach it.
    if (e.method == ScoreMethod::Conquer) {
        fireLegendTrigger(TriggerType::WhenIConquer, e.player);
    }
    if (e.method == ScoreMethod::Hold) {
        fireLegendTrigger(TriggerType::WhenIHold, e.player);
    }
    fireLegendTrigger(TriggerType::WhenIConquerOrHold, e.player);

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

    // "At the start of your Main Phase" — used by Iascylla's delayed
    // "move enemy unit here" and similar. Both static triggers on cards
    // AND delayed abilities (registered via state.delayed_abilities) fire.
    if (e.new_phase == TurnPhase::MainPhase) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            auto trigger = getTrigger(card_registry_, obj.card_def_id);
            if (trigger == TriggerType::AtStartOfMain) {
                fireTrigger(id, e.turn_player);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            auto trigger = getTrigger(card_registry_,
                state_.getObject(ps.legend_zone).card_def_id);
            if (trigger == TriggerType::AtStartOfMain) {
                fireTrigger(ps.legend_zone, e.turn_player);
            }
        }
        // Delayed abilities (Iascylla schedules one of these on hold)
        checkDelayedAbilities(TriggerType::AtStartOfMain, e.turn_player,
                              kInvalidId);
    }
}

} // namespace riftbound
