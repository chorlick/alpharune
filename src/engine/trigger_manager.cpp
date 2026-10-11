#include "trigger_manager.h"
#include "cards/card.h"
#include "engine/effect_executor.h"
#include "engine/deck_ops.h"
#include "engine/keyword_abilities.h"

namespace riftbound {

// Defined with the other dispatch helpers below.
static bool cardFiresOn(const CardRegistry& reg, CardDefId def_id, TriggerType t);

// Counter on a unit: battlefield id + 1 of the combat it is, or was, in.
static const char* const kCombatParticipantKey = "__combat_participant";

TriggerManager::TriggerManager(GameState& state, EventBus& events,
                               const CardDB& card_db, ChainManager& chain,
                               const CardRegistry& card_registry)
    : state_(state), events_(events), card_db_(card_db), chain_(chain),
      card_registry_(card_registry) {}

void TriggerManager::subscribe() {
    connections_.push_back(events_.on_spell_resolved.connect(
        [this](const SpellResolvedEvent& e) { onSpellResolved(e); }));
    connections_.push_back(events_.on_card_played.connect(
        [this](const CardPlayedEvent& e) { onCardPlayed(e); }));
    connections_.push_back(events_.on_entered_board.connect(
        [this](const EnteredBoardEvent& e) { onEnteredBoard(e); }));
    connections_.push_back(events_.on_combat_started.connect(
        [this](const CombatStartedEvent& e) { onCombatStarted(e); }));
    connections_.push_back(events_.on_combat_ended.connect(
        [this](const CombatEndedEvent& e) { onCombatEnded(e); }));
    connections_.push_back(events_.on_unit_died.connect(
        [this](const UnitDiedEvent& e) { onUnitDied(e); }));
    connections_.push_back(events_.on_score.connect(
        [this](const ScoreEvent& e) { onScore(e); }));
    connections_.push_back(events_.on_unit_moved.connect(
        [this](const UnitMovedEvent& e) { onUnitMoved(e); }));
    connections_.push_back(events_.on_phase_changed.connect(
        [this](const PhaseChangedEvent& e) { onPhaseChanged(e); }));
    connections_.push_back(events_.on_unit_stunned.connect(
        [this](const UnitStunnedEvent& e) { onUnitStunned(e); }));
    connections_.push_back(events_.on_damage_dealt.connect(
        [this](const DamageDealtEvent& e) { onDamageDealt(e); }));
    connections_.push_back(events_.on_unit_readied.connect(
        [this](const UnitReadiedEvent& e) { onUnitReadied(e); }));
    connections_.push_back(events_.on_card_hidden.connect(
        [this](const CardHiddenEvent& e) { onCardHidden(e); }));
    connections_.push_back(events_.on_played_from_facedown.connect(
        [this](const PlayedFromFacedownEvent& e) { onPlayedFromFacedown(e); }));
    connections_.push_back(events_.on_unit_returned_to_hand.connect(
        [this](const UnitReturnedToHandEvent& e) { onUnitReturnedToHand(e); }));
    connections_.push_back(events_.on_showdown_started.connect(
        [this](const ShowdownStartedEvent& e) { onShowdownStarted(e); }));
    connections_.push_back(events_.on_card_revealed.connect(
        [this](const CardRevealedEvent& e) { onCardRevealed(e); }));
    connections_.push_back(events_.on_object_state_changed.connect(
        [this](const ObjectStateChangedEvent& e) { onObjectStateChanged(e); }));
    connections_.push_back(events_.on_cards_drawn.connect(
        [this](const CardsDrawnEvent& e) { onCardsDrawn(e); }));
    connections_.push_back(events_.on_left_board.connect(
        [this](const LeftBoardEvent& e) { onLeftBoard(e); }));
    connections_.push_back(events_.on_token_created.connect(
        [this](const TokenCreatedEvent& e) { onTokenCreated(e); }));
    connections_.push_back(events_.on_state_settled.connect(
        [this](const StateSettledEvent&) { onStateSettled(); }));
}

TriggerManager::~TriggerManager() {
    for (auto& c : connections_) {
        if (c.connected()) c.disconnect();
    }
}

void TriggerManager::onUnitStunned(const UnitStunnedEvent& e) {
    // "When you stun an enemy unit" fires for the stunner's cards and legend
    // when the stunned unit is the other player's (stunning your own unit is
    // not stunning an enemy). The stunned unit is the trigger's subject.
    if (e.stunner == e.victim_controller) return;
    std::vector<GameObjectId> watchers;
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != e.stunner) continue;
        if (obj.card_def_id == kInvalidId) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouStun))
            watchers.push_back(id);
    }
    for (auto wid : watchers)
        fireTrigger(wid, e.stunner, 0, TriggerType::WhenYouStun, /*subject=*/e.stunned_unit);
    checkDelayedAbilities(TriggerType::WhenYouStun, e.stunner, e.stunned_unit);
}

void TriggerManager::onDamageDealt(const DamageDealtEvent& e) {
    if (e.amount <= 0 || !state_.objectExists(e.target)) return;
    const auto& unit = state_.getObject(e.target);
    if (!unit.isUnit() || !unit.location.has_value()) return;
    checkDelayedAbilities(TriggerType::WhenAUnitTakesDamage, PlayerId::Player1, e.target);
    checkDelayedAbilities(TriggerType::WhenAUnitTakesDamage, PlayerId::Player2, e.target);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Delayed abilities (CR 389-392)
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::checkDelayedAbilities(TriggerType trigger,
                                            PlayerId relevant_player,
                                            GameObjectId event_object) {
    // Decide what fires first, on a copy: queueing an ability emits events,
    // and a handler may add a delayed ability (the vector can reallocate) —
    // one added now did not see this event and must not fire for it.
    std::vector<DelayedAbility> firing;
    for (auto& da : state_.delayed_abilities) {
        if (da.fired) continue;
        if (da.trigger != trigger) continue;
        if (da.watches() != relevant_player) continue;
        // Object-scoped delayed abilities (e.g. Deadly Flourish: "when IT
        // dies") fire only for the specific target. kInvalidId = unscoped.
        if (da.target_filter != kInvalidId &&
            da.target_filter != event_object) continue;
        if (!da.repeats) da.fired = true;
        firing.push_back(da);
    }
    // Remove the one-shot abilities that are about to fire.
    state_.delayed_abilities.erase(
        std::remove_if(state_.delayed_abilities.begin(), state_.delayed_abilities.end(),
            [](const DelayedAbility& da) { return da.fired; }),
        state_.delayed_abilities.end());

    for (const auto& da : firing) {
        events_.logTrace("DELAYED_ABILITY: firing for " +
                         (state_.objectExists(da.source) ? state_.getObject(da.source).name : "?"));
        const ChainItemId item_id = chain_.addAbility(da.source, da.controller, da.card_def_id);
        // Stamp the originating trigger so the card's onTrigger can branch on
        // ctx.firing_trigger (e.g. Grim Resolve's WhenIWinCombat XP rider,
        // Targon's Peak's AtEndOfTurn ready-runes), and the event's object
        // ("buff IT", "move ME"), as for any trigger.
        for (auto& item : state_.chain.items) {
            if (item.id != item_id) continue;
            item.fired_trigger = trigger;
            item.triggering_subject = event_object;
            item.triggering_move = state_.chain.move_being_announced;
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Equipment triggers — check attached gear when unit events fire
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::fireEquippedTriggers(GameObjectId unit_id, PlayerId controller,
                                           TriggerType trigger) {
    if (!state_.objectExists(unit_id)) return;
    // Copy: queueing an ability emits events, and a handler may attach or
    // detach gear.
    const std::vector<GameObjectId> attached = state_.getObject(unit_id).attachments;
    for (auto gear_id : attached)
        queueEquippedTrigger(gear_id, unit_id, controller, trigger);
}

// An Equipment's "When I ..." ability (its effect text, which speaks for the
// unit it is attached to) is a triggered ability like any other: it goes on
// the chain (CR 383.3) and resolves through Card::onEquippedTrigger, where the
// choice helpers and the finalization pass work as they do for onTrigger.
// `is_rules_text_inactive` is deliberately not consulted — it marks the gear's
// PRINTED text as inactive while attached (CR 718.2), which is exactly when
// this ability applies.
void TriggerManager::queueEquippedTrigger(GameObjectId gear_id, GameObjectId unit_id,
                                          PlayerId controller, TriggerType trigger) {
    if (!state_.objectExists(gear_id)) return;
    const auto& gear = state_.getObject(gear_id);
    if (gear.card_def_id == kInvalidId) return;
    const Card* gear_card = card_registry_.get(gear.card_def_id);
    if (!gear_card || gear_card->equippedTriggerType() != trigger) return;
    if (!gear_card->triggerCondition(state_, gear_id, controller, trigger, unit_id)) return;

    events_.logTrace("EQUIP_TRIGGER: " + gear.name + " fires on " +
                     (state_.objectExists(unit_id) ? state_.getObject(unit_id).name
                                                   : std::string("?")) +
                     " (" + toString(controller) + ")");
    const ChainItemId item_id = chain_.addAbility(gear_id, controller, gear.card_def_id);
    for (auto& item : state_.chain.items) {
        if (item.id != item_id) continue;
        item.fired_trigger = trigger;
        item.equipped_unit = unit_id;
        item.triggering_subject = unit_id;
        item.triggering_move = state_.chain.move_being_announced;
        break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Helper — get trigger type from Card object
// ═══════════════════════════════════════════════════════════════════════════════

static TriggerType getTrigger(const CardRegistry& reg, CardDefId def_id) {
    Card* card = reg.get(def_id);
    return card ? card->triggerType() : TriggerType::None;
}

// Multi-trigger-aware membership test: true if the card declares `t` among
// its trigger types. Use this (not `getTrigger(...) == t`) so cards that
// register more than one trigger fire on each.
static bool cardFiresOn(const CardRegistry& reg, CardDefId def_id, TriggerType t) {
    Card* card = reg.get(def_id);
    return card && card->firesOn(t);
}

void TriggerManager::fireBattlefieldTriggers(TriggerType t, PlayerId player,
                                              BattlefieldId at_battlefield,
                                              GameObjectId subject) {
    for (auto& bf : state_.battlefields) {
        if (at_battlefield != kInvalidId && bf.id != at_battlefield) continue;
        if (!state_.objectExists(bf.card_object_id)) continue;
        if (cardFiresOn(card_registry_, state_.getObject(bf.card_object_id).card_def_id, t)) {
            // No player named by the event: the battlefield's controller
            // controls its abilities (CR 190.6.a), the turn player if it is
            // uncontrolled (CR 190.6.b).
            PlayerId who = player;
            if (who == PlayerId::None)
                who = bf.controller.value_or(state_.turn.turn_player);
            if (who == PlayerId::None) continue;
            fireTrigger(bf.card_object_id, who, 0, t, subject);
        }
    }
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
                                        int triggering_spell_energy_spent,
                                        GameObjectId subject) {
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        if (relevant_player != PlayerId::None && pid != relevant_player) continue;
        auto legend_id = state_.player(pid).legend_zone;
        if (legend_id == kInvalidId || !state_.objectExists(legend_id)) continue;
        auto& legend = state_.getObject(legend_id);
        if (cardFiresOn(card_registry_, legend.card_def_id, t)) {
            fireTrigger(legend_id, pid, triggering_spell_energy_spent, t, subject);
        }
    }
}

// LeBlanc, Everywhere at Once: "your [Temporary] effects at my battlefield
// don't trigger." True when `source` is a friendly [Temporary] unit at — or,
// for its own death, last at — a battlefield where a card of its controller
// says so (Card::suppressesTemporaryTriggersHere).
bool TriggerManager::temporaryTriggerSuppressed(GameObjectId source) const {
    const auto& obj = state_.getObject(source);
    if (!obj.isUnit()) return false;
    // A unit that has died is judged as it last existed (CR 383.2.c.3.b).
    const bool dead = !obj.location.has_value() && obj.last_known.valid;
    const bool temporary = dead ? obj.last_known.keywords.has(Keyword::Temporary)
                                : obj.hasKeyword(Keyword::Temporary);
    if (!temporary) return false;
    const std::optional<LocationId> where = dead ? obj.last_known.location : obj.location;
    if (!where.has_value() || !std::holds_alternative<BattlefieldLocation>(*where)) return false;
    const BattlefieldId bf = std::get<BattlefieldLocation>(*where).id;
    for (const auto& [oid, other] : state_.objects) {
        if (oid == source || other.controller != obj.controller) continue;
        const auto obf = other.battlefieldId();
        if (!obf || *obf != bf) continue;
        const Card* oc = card_registry_.get(other.card_def_id);
        if (oc && oc->suppressesTemporaryTriggersHere()) {
            events_.logTrace("TRIGGER_SUPPRESSED: " + obj.name +
                             " (Temporary) at LeBlanc's battlefield");
            return true;
        }
    }
    return false;
}

bool TriggerManager::fireTrigger(GameObjectId source, PlayerId controller,
                                  int triggering_spell_energy_spent,
                                  TriggerType which,
                                  GameObjectId subject) {
    if (!state_.objectExists(source)) return false;
    auto& obj = state_.getObject(source);

    // An attached gear's printed rules text is Inactive (CR 718.2): its own
    // triggered abilities do not trigger. (Abilities in its EFFECT text speak
    // for the unit and go through queueEquippedTrigger instead.)
    if (obj.is_rules_text_inactive) return false;

    if (temporaryTriggerSuppressed(source)) return false;

    // For single-trigger cards `which` may be None; resolve it from the
    // card so multi-trigger cards still record which event fired.
    TriggerType fired = which;
    if (fired == TriggerType::None) {
        fired = getTrigger(card_registry_, obj.card_def_id);
    }
    if (fired == TriggerType::None) return false;

    const Card* card = card_registry_.get(obj.card_def_id);
    // The card's intervening condition ("When a BUFFED friendly unit dies"):
    // if it isn't met the ability does not trigger.
    if (card && !card->triggerCondition(state_, source, controller, fired, subject))
        return false;
    // "When ONE OR MORE ...": events that happen together trigger it once. An
    // ability of this source for this trigger that has not been finalized yet
    // came from the same set of events (nobody has had priority since).
    if (card && card->triggersOncePerBatch(fired)) {
        for (const auto& ci : state_.chain.items)
            if (ci.source == source && ci.status == ChainItemStatus::Pending &&
                ci.is_ability && !ci.is_activated_ability && ci.fired_trigger == fired)
                return false;
    }

    events_.logDebug("TRIGGER: " + obj.name + " (" + toString(controller) +
                     ") fires ability");
    const ChainItemId item_id = chain_.addAbility(source, controller, obj.card_def_id);
    // Snapshot the triggering spell's spent value on the just-added
    // chain item so resume-time reads aren't subject to clobber by a
    // subsequent spell-play. Found by id: adding an item can put others above it.
    for (auto& item : state_.chain.items) {
        if (item.id != item_id) continue;
        item.fired_trigger = fired;
        if (triggering_spell_energy_spent > 0)
            item.triggering_spell_energy_spent = triggering_spell_energy_spent;
        if (subject != kInvalidId) item.triggering_subject = subject;
        item.triggering_move = state_.chain.move_being_announced;
        break;
    }
    // An Equipment on this unit whose effect text is a copy of the unit's
    // text (Card::copiesBearerText) has this same ability, speaking for the
    // unit: it triggers once more per such Equipment, each instance its own
    // pending ability with its own choices.
    if (obj.isUnit() && effect_executor_) {
        const int copies = effect_executor_->textCopiesOn(source);
        ChainItem queued;
        bool found = false;
        for (const auto& item : state_.chain.items)
            if (item.id == item_id) { queued = item; found = true; break; }
        for (int i = 0; found && i < copies; ++i) {
            ChainItem copy = queued;
            copy.id = state_.chain.allocateId();
            state_.chain.items.push_back(std::move(copy));
            events_.logDebug("TRIGGER: " + state_.getObject(source).name +
                             " — the same ability in an Equipment's copied text");
        }
    }
    return true;
}

// A keyword's own "when this is played" ability ([Vision], [Weaponmaster]):
// one chain item per instance of the keyword (CR 817.2, 821.1.c.7), resolved
// by engine/keyword_abilities. `own_card_fires`: the object's Card class
// implements the printed instance itself.
void TriggerManager::queueKeywordAbilities(GameObjectId object, PlayerId controller,
                                           Keyword keyword, bool own_card_fires) {
    if (!state_.objectExists(object) || !keywordAbilityCard(keyword)) return;
    const int instances =
        keywordAbilityInstances(state_.getObject(object), keyword, own_card_fires);
    if (instances <= 0 || temporaryTriggerSuppressed(object)) return;
    for (int i = 0; i < instances; ++i) {
        events_.logDebug("TRIGGER: " + state_.getObject(object).name + " [" +
                         toString(keyword) + "] (" + toString(controller) + ")");
        // Not the source's card: the ability is the keyword's (kInvalidId
        // keeps the source card's own callbacks and finalize pass out of it).
        const ChainItemId item_id = chain_.addAbility(object, controller, kInvalidId);
        for (auto& item : state_.chain.items) {
            if (item.id != item_id) continue;
            item.fired_trigger = TriggerType::WhenYouPlayMe;
            item.keyword_ability = keyword;
            break;
        }
    }
}

// "When you play a unit" and its relatives, for a unit card or a unit token
// (a token is played too, CR 350.2): the player's other cards in play, their
// delayed abilities, the battlefield it was played to, the other player's
// "When an opponent plays a unit", and the player's legend.
void TriggerManager::announceUnitPlayed(GameObjectId unit, PlayerId player) {
    std::vector<GameObjectId> watchers;
    for (auto& [id, other] : state_.objects) {
        if (id == unit || !other.location.has_value()) continue;
        if (other.controller != player) continue;
        if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayAUnit))
            watchers.push_back(id);
    }
    for (auto wid : watchers)
        fireTrigger(wid, player, 0, TriggerType::WhenYouPlayAUnit, /*subject=*/unit);

    // "When a friendly unit is played this turn, ..." (Rally the Troops).
    checkDelayedAbilities(TriggerType::WhenYouPlayAUnit, player, unit);

    // Battlefield cards with "When you play a unit here" (e.g. Star Spring).
    // Fire only on the battlefield the just-played unit landed at.
    if (state_.objectExists(unit)) {
        if (auto played_bf = state_.getObject(unit).battlefieldId())
            fireBattlefieldTriggers(TriggerType::WhenYouPlayAUnit, player, *played_bf,
                                    /*subject=*/unit);
    }

    // "When an opponent plays a unit" — the other player's cards in play; the
    // played unit is the subject (Vex, Apathetic stuns it).
    const PlayerId opp = opponent(player);
    std::vector<GameObjectId> opp_watchers;
    for (auto& [id, other] : state_.objects) {
        if (other.controller != opp || !other.location.has_value()) continue;
        if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenOpponentPlaysAUnit))
            opp_watchers.push_back(id);
    }
    for (auto wid : opp_watchers)
        fireTrigger(wid, opp, 0, TriggerType::WhenOpponentPlaysAUnit, /*subject=*/unit);

    // Legends say "When you play a unit," too; the legend has no location.
    fireLegendTrigger(TriggerType::WhenYouPlayAUnit, player, 0, /*subject=*/unit);
}

// A token is PLAYED (CR 350.2: "Tokens are not cards, but can still be
// Played"), so abilities that trigger on playing a unit or a gear see it.
// Not "When you play a card": a token is not a card.
void TriggerManager::onTokenCreated(const TokenCreatedEvent& e) {
    if (!state_.objectExists(e.object) || e.controller == PlayerId::None) return;
    if (!state_.getObject(e.object).location.has_value()) return;
    const PlayerId player = e.controller;

    if (e.card_type == CardType::Unit) {
        // "When you play a token unit" (Lillia, Protector of Dreams).
        std::vector<GameObjectId> watchers;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || other.controller != player) continue;
            if (!other.location.has_value() && other.zone != ZoneType::LegendZone) continue;
            if (other.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayATokenUnit))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, player, 0, TriggerType::WhenYouPlayATokenUnit, /*subject=*/e.object);
        announceUnitPlayed(e.object, player);
    } else if (e.card_type == CardType::Gear) {
        std::vector<std::pair<GameObjectId, TriggerType>> fires;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || other.card_def_id == kInvalidId) continue;
            if (!other.location.has_value()) continue;
            if (other.controller == player &&
                cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayAGear))
                fires.push_back({id, TriggerType::WhenYouPlayAGear});
            if (other.controller != player &&
                cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenOpponentPlaysAGear))
                fires.push_back({id, TriggerType::WhenOpponentPlaysAGear});
        }
        for (auto& [id, t] : fires)
            fireTrigger(id, state_.getObject(id).controller, 0, t, /*subject=*/e.object);
    }

    // The token's own keyword abilities. It has just been put on the board,
    // after the last aura pass: refresh the continuous effects so a keyword it
    // has from one ("Your Sand Soldiers have [Weaponmaster]") is there.
    if (e.card_type == CardType::Unit || e.card_type == CardType::Gear) {
        if (effect_executor_) effect_executor_->recalculateContinuousEffects();
        if (!state_.objectExists(e.object)) return;
        const bool has_card =
            card_registry_.get(state_.getObject(e.object).card_def_id) != nullptr;
        queueKeywordAbilities(e.object, player, Keyword::Vision, false);
        if (e.card_type == CardType::Unit)
            queueKeywordAbilities(e.object, player, Keyword::Weaponmaster, has_card);
    }
}

// "When my Might becomes N or more" (Card::mightThreshold): fires on the edge
// from below N to N or more, judged each time the state has settled, so it
// sees every source of Might (buffs, auras, Equipment, "this turn" effects).
// Every unit at a battlefield while its combat is in progress is in that
// combat (CR 323.2: it has its controller's designation). Noted on the unit,
// so "a combat that I was in" still knows it after the unit has left.
void TriggerManager::noteCombatParticipants() {
    for (const auto& bf : state_.battlefields) {
        if (!bf.combat_in_progress) continue;
        for (auto& [id, obj] : state_.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            auto here = obj.battlefieldId();
            if (here && *here == bf.id)
                obj.card_counters[kCombatParticipantKey] = static_cast<int>(bf.id) + 1;
        }
    }
}

void TriggerManager::onStateSettled() {
    noteCombatParticipants();
    static const std::string kAtThreshold = "__at_might_threshold";
    std::vector<GameObjectId> crossed;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.location.has_value() || !obj.isUnit() || obj.card_def_id == kInvalidId) continue;
        const Card* card = card_registry_.get(obj.card_def_id);
        const int n = card ? card->mightThreshold() : 0;
        if (n <= 0) continue;
        const bool at = obj.current_might >= n;
        auto it = obj.card_counters.find(kAtThreshold);
        const bool was = it != obj.card_counters.end() && it->second != 0;
        if (at == was) continue;
        obj.card_counters[kAtThreshold] = at ? 1 : 0;
        if (at) crossed.push_back(id);
    }
    for (auto id : crossed)
        fireTrigger(id, state_.getObject(id).controller, 0,
                    TriggerType::WhenMyMightReachesThreshold, /*subject=*/id);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Event handlers
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::onCardPlayed(const CardPlayedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    // Per-turn "what have you played" tracking (Swain, Visionary).
    {
        auto& pps = state_.player(e.player);
        if (e.card_type == CardType::Spell) { pps.played_spell_this_turn = true; pps.spells_played_this_turn++; }
        if (e.card_type == CardType::Unit && !obj.isToken()) pps.played_nontoken_unit_this_turn = true;
        if (e.card_type == CardType::Gear && !obj.isToken()) pps.played_nontoken_gear_this_turn = true;
    }
    // CR 420.4.a: abilities that trigger on a card being played trigger when
    // playing it is COMPLETED by its resolution. A permanent resolves as it is
    // finalized, so its play triggers fire here; a spell's fire from
    // onSpellResolved (and never, if it is countered — 420.4.a.1 / 426.1.b).
    if (e.card_type == CardType::Spell)
        obj.card_counters["__energy_spent_on_play"] = e.energy_spent;
    // Which play of its controller's turn this is ("your first card each
    // turn", "your second card"): kept on the card, because a spell's play
    // triggers fire only when it resolves — by then later plays have moved
    // PlayerState::cards_played_this_turn on. Turn-stamped (turn + 1).
    obj.card_counters["__play_ordinal"] = e.cards_played_this_turn;
    obj.card_counters["__play_ordinal_turn"] = state_.turn.turn_number + 1;

    // "When you play a card" (any type) on the player's other cards + legend,
    // and "When an opponent plays a gear" on the other player's cards.
    if (e.card_type != CardType::Spell) {
        std::vector<std::pair<GameObjectId, TriggerType>> fires;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || other.card_def_id == kInvalidId) continue;
            if (!other.location.has_value() && other.zone != ZoneType::LegendZone) continue;
            if (other.controller == e.player &&
                cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayACard))
                fires.push_back({id, TriggerType::WhenYouPlayACard});
            if (other.controller != e.player && e.card_type == CardType::Gear &&
                cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenOpponentPlaysAGear))
                fires.push_back({id, TriggerType::WhenOpponentPlaysAGear});
        }
        for (auto& [id, t] : fires)
            fireTrigger(id, state_.getObject(id).controller, e.energy_spent, t,
                        /*subject=*/e.object);
    }

    // [Vision] (CR 817): "When this is played, [Predict]." Each instance is a
    // triggered ability of its own (817.2) and the controller decides for each
    // whether to recycle (817.2.a), so each goes on the chain
    // (queueKeywordAbilities). The permanent may have [Vision] from a
    // continuous effect instead of its own text ("Your Mechs have [Vision]");
    // it has just been put where it is going, after the last aura pass, so
    // the continuous effects are refreshed before counting.
    if (obj.isPermanent()) {
        if (effect_executor_) effect_executor_->recalculateContinuousEffects();
        queueKeywordAbilities(e.object, e.player, Keyword::Vision, false);
        if (!state_.objectExists(e.object)) return;
    }

    // "When you play me" — fire on the card that was just played
    if (obj.isPermanent()) {
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouPlayMe)) {
            fireTrigger(e.object, e.player, 0, TriggerType::WhenYouPlayMe);
        }
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::AsYouPlayMe)) {
            fireTrigger(e.object, e.player, 0, TriggerType::AsYouPlayMe);
        }
    }

    // "When you play a gear" — the player's OTHER cards in play.
    if (e.card_type == CardType::Gear) {
        std::vector<GameObjectId> watchers;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || !other.location.has_value()) continue;
            if (other.controller != e.player) continue;
            if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayAGear))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, e.player, 0, TriggerType::WhenYouPlayAGear, /*subject=*/e.object);
    }

    // "When you play a unit" and its relatives.
    if (e.card_type == CardType::Unit) announceUnitPlayed(e.object, e.player);
}

void TriggerManager::onSpellResolved(const SpellResolvedEvent& e) {
    // CR 420.4.a: "When you play a spell" / "When you play a card" trigger now,
    // when the spell has resolved.
    if (!state_.objectExists(e.spell)) return;
    const auto& spell = state_.getObject(e.spell);
    int energy_spent = 0;
    if (auto it = spell.card_counters.find("__energy_spent_on_play");
        it != spell.card_counters.end()) energy_spent = it->second;

    std::vector<std::pair<GameObjectId, TriggerType>> fires;
    for (auto& [id, other] : state_.objects) {
        if (id == e.spell || other.controller != e.controller) continue;
        if (!other.location.has_value() && other.zone != ZoneType::LegendZone) continue;
        if (other.card_def_id == kInvalidId) continue;
        if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayASpell))
            fires.push_back({id, TriggerType::WhenYouPlayASpell});
        if (cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayACard))
            fires.push_back({id, TriggerType::WhenYouPlayACard});
    }
    for (auto& [id, t] : fires)
        fireTrigger(id, e.controller, energy_spent, t, /*subject=*/e.spell);

    // Battlefield cards ("When a player plays a spell, they ..." — Abandoned
    // Hall; Forgotten Library). A battlefield's card object is not at a
    // location, so the sweep above never sees it. The player who played the
    // spell controls the ability (CR 190.6.c); a card whose text is limited to
    // the battlefield's controller checks that itself.
    for (auto& bf : state_.battlefields) {
        if (!state_.objectExists(bf.card_object_id)) continue;
        if (cardFiresOn(card_registry_, state_.getObject(bf.card_object_id).card_def_id,
                        TriggerType::WhenYouPlayASpell))
            fireTrigger(bf.card_object_id, e.controller, energy_spent,
                        TriggerType::WhenYouPlayASpell, /*subject=*/e.spell);
    }
}

void TriggerManager::onEnteredBoard(const EnteredBoardEvent& e) {
    if (!e.from_play) return;
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouPlayThis)) {
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenYouPlayThis);
    }
}

void TriggerManager::onCombatStarted(const CombatStartedEvent& e) {
    auto bf_loc = BattlefieldLocation{e.battlefield};
    // The units here are in this combat from its start (see
    // noteCombatParticipants for those that join it later).
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        auto here = obj.battlefieldId();
        if (here && *here == e.battlefield)
            obj.card_counters[kCombatParticipantKey] = static_cast<int>(e.battlefield) + 1;
    }

    // Battlefield cards: "When combat starts here," (Threshold of the Gray).
    fireBattlefieldTriggers(TriggerType::WhenCombatStartsHere, e.attacker, e.battlefield);
    // "When you defend here," (Fortified Position, Reaver's Row, Ravenbloom
    // Conservatory): the defending player's ability.
    if (e.defender != PlayerId::None)
        fireBattlefieldTriggers(TriggerType::WhenYouDefendHere, e.defender, e.battlefield);

    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
        auto unit_bf = obj.battlefieldId();
        if (!unit_bf || *unit_bf != e.battlefield) continue;

        bool is_attacker = (obj.controller == e.attacker);
        bool is_defender = (obj.controller == e.defender);

        if (is_attacker &&
            cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIAttack)) {
            fireTrigger(id, obj.controller, 0, TriggerType::WhenIAttack);
        }
        if (is_defender &&
            cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIDefend)) {
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
            fireTrigger(id, obj.controller, 0, TriggerType::WhenIDefend);
        }
        if ((is_attacker || is_defender) &&
            cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIAttackOrDefend)) {
            fireTrigger(id, obj.controller, 0, TriggerType::WhenIAttackOrDefend);
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

    // "When a friendly unit attacks or defends alone" — fires on the side's
    // controller's on-board cards (incl. free-standing gear) when EXACTLY ONE
    // friendly unit participates on that side at this BF; the lone unit is the
    // subject. (Mask of Foresight 060.)
    std::vector<GameObjectId> side_attackers, side_defenders;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
        auto ubf = obj.battlefieldId();
        if (!ubf || *ubf != e.battlefield) continue;
        if (obj.combat_designation == CombatDesignation::Attacker)
            side_attackers.push_back(id);
        else if (obj.combat_designation == CombatDesignation::Defender)
            side_defenders.push_back(id);
    }
    auto fireAlone = [&](GameObjectId lone, PlayerId side) {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != side || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenAUnitAttacksOrDefendsAlone))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, side, 0,
                        TriggerType::WhenAUnitAttacksOrDefendsAlone, /*subject=*/lone);
    };
    if (side_attackers.size() == 1) fireAlone(side_attackers[0], e.attacker);
    if (side_defenders.size() == 1) fireAlone(side_defenders[0], e.defender);

    // "When an enemy unit attacks a battlefield you control" — once per
    // attacking unit (the subject), on the cards and the legend of the player
    // who controls this battlefield. (Nine-Tailed Fox 252.)
    for (const auto& bf : state_.battlefields) {
        if (bf.id != e.battlefield || !bf.controller.has_value()) continue;
        const PlayerId holder = *bf.controller;
        if (holder == e.attacker) break;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != holder || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenAnEnemyUnitAttacksYourBattlefield))
                watchers.push_back(id);
        }
        for (auto attacker : side_attackers) {
            for (auto wid : watchers)
                fireTrigger(wid, holder, 0,
                            TriggerType::WhenAnEnemyUnitAttacksYourBattlefield, attacker);
            fireLegendTrigger(TriggerType::WhenAnEnemyUnitAttacksYourBattlefield, holder, 0,
                              attacker);
        }
        break;
    }

    // "When you defend at a battlefield" — fires on the defending player's
    // on-board cards anywhere (the unit it moves is, by definition, NOT yet at
    // the BF). The defended battlefield id is stashed on each watcher's
    // counters so the card can read it at resume time. Gated on an actual
    // defense (>=1 defending unit). (Loyal Pup 447.)
    if (!side_defenders.empty()) {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != e.defender || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouDefendAtABattlefield)) {
                obj.card_counters["__defend_bf"] = static_cast<int>(e.battlefield);
                watchers.push_back(id);
            }
        }
        for (auto wid : watchers)
            fireTrigger(wid, e.defender, 0,
                        TriggerType::WhenYouDefendAtABattlefield);
    }
}

void TriggerManager::onCombatEnded(const CombatEndedEvent& e) {
    // "When a combat that I was in ends," — fires win, lose or draw, for every
    // unit that was in this combat (noteCombatParticipants), wherever it is
    // now: attackers recalled after a tie were in it too. A unit must still be
    // on the board to have the ability.
    {
        const int mark = static_cast<int>(e.battlefield) + 1;
        std::vector<GameObjectId> participants;
        for (auto& [id, obj] : state_.objects) {
            auto it = obj.card_counters.find(kCombatParticipantKey);
            if (it == obj.card_counters.end() || it->second != mark) continue;
            obj.card_counters.erase(it);
            if (!obj.isUnit() || obj.card_def_id == kInvalidId || !obj.location.has_value())
                continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenACombatIWasInEnds))
                participants.push_back(id);
        }
        std::sort(participants.begin(), participants.end());
        for (auto uid : participants)
            fireTrigger(uid, state_.getObject(uid).controller, 0,
                        TriggerType::WhenACombatIWasInEnds);
    }

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
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIWinCombat)) {
            fireTrigger(uid, obj.controller, 0, TriggerType::WhenIWinCombat);
        }
        // Equipped gear with WhenIWinCombat
        fireEquippedTriggers(uid, obj.controller, TriggerType::WhenIWinCombat);
        // Object-scoped delayed abilities watching "when IT wins a combat"
        // (e.g. Grim Resolve's "gain 2 XP when the buffed unit wins").
        checkDelayedAbilities(TriggerType::WhenIWinCombat, obj.controller, uid);
    }
}

void TriggerManager::onUnitDied(const UnitDiedEvent& e) {
    // "When an enemy unit here dies," — watchers at the location it died at.
    {
        std::vector<GameObjectId> watchers;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || other.card_def_id == kInvalidId) continue;
            if (other.controller == e.controller) continue;
            if (!other.location.has_value() || *other.location != e.was_at) continue;
            if (cardFiresOn(card_registry_, other.card_def_id,
                            TriggerType::WhenAnEnemyUnitDiesHere))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, state_.getObject(wid).controller, 0,
                        TriggerType::WhenAnEnemyUnitDiesHere, /*subject=*/e.object);
    }
    // Deathknell an Equipment gave the unit ("[Deathknell] — Draw 1." in the
    // gear's effect text): the gear it had as it died (last-known information).
    if (state_.objectExists(e.object)) {
        const auto& dead = state_.getObject(e.object);
        const auto gear_at_death = dead.last_known.valid ? dead.last_known.attachments
                                                         : dead.attachments;
        for (auto gear_id : gear_at_death)
            queueEquippedTrigger(gear_id, e.object, e.controller, TriggerType::WhenIDie);
    }
    if (state_.objectExists(e.object) &&
        cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                    TriggerType::WhenIDie)) {
        queueOwnDeathTrigger(e.object, e.controller);
    }
    // 'This turn, it has "When I die, ..."' set on the unit by an effect
    // (delayed abilities scoped to it) is checked at the end of this handler.

    // "When a friendly unit dies" — fire on other objects the controller owns.
    // "When an enemy unit dies" — fire on the OPPONENT's objects (Pyke,
    // Returned: gated "while I'm at a battlefield" + once/turn in the card).
    PlayerId opp = opponent(e.controller);
    for (auto& [id, other] : state_.objects) {
        if (!other.location.has_value()) continue;
        if (id == e.object) continue;
        if (other.controller == e.controller &&
            cardFiresOn(card_registry_, other.card_def_id,
                        TriggerType::WhenAFriendlyUnitDies)) {
            fireTrigger(id, e.controller, 0, TriggerType::WhenAFriendlyUnitDies,
                        /*subject=*/e.object);
        }
        if (other.controller == opp &&
            cardFiresOn(card_registry_, other.card_def_id,
                        TriggerType::WhenAnEnemyUnitDies)) {
            fireTrigger(id, opp, 0, TriggerType::WhenAnEnemyUnitDies, /*subject=*/e.object);
        }
    }
    // Legends have no location, so the sweep above never reaches them
    // (Battle Mistress: "When one or more enemy units die, ready me.").
    fireLegendTrigger(TriggerType::WhenAFriendlyUnitDies, e.controller, 0, e.object);
    fireLegendTrigger(TriggerType::WhenAnEnemyUnitDies, opp, 0, e.object);

    // "When you kill a unit with a spell" (Immortal Phoenix 037): the spell
    // that is resolving killed it, or it died in a cleanup of lethal damage a
    // spell dealt (LastKnown::killed_with_spell / killed_by). Fans to that
    // player's on-board cards AND cards in their TRASH (Phoenix triggers from
    // trash).
    if (state_.objectExists(e.object) && state_.getObject(e.object).last_known.valid &&
        state_.getObject(e.object).last_known.killed_with_spell &&
        state_.getObject(e.object).last_known.killed_by != PlayerId::None) {
        const PlayerId killer = state_.getObject(e.object).last_known.killed_by;
        std::vector<GameObjectId> watchers;
        for (auto& [id, other] : state_.objects) {
            if (id == e.object || other.card_def_id == kInvalidId) continue;
            bool on_board = other.location.has_value() && other.controller == killer;
            bool in_trash = other.zone == ZoneType::Trash && other.owner == killer;
            if (!on_board && !in_trash) continue;
            if (cardFiresOn(card_registry_, other.card_def_id,
                            TriggerType::WhenYouKillAUnitWithASpell))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, killer, 0, TriggerType::WhenYouKillAUnitWithASpell);
    }

    // Delayed abilities scoped to this dying unit (Deadly Flourish:
    // "When IT dies this turn, ..."). The opposing player created the
    // delayed ability, so we check both controllers' scoped triggers.
    checkDelayedAbilities(TriggerType::WhenIDie, e.controller, e.object);
    checkDelayedAbilities(TriggerType::WhenIDie,
                           opponent(e.controller), e.object);
}

// "When I die" / [Deathknell] on the object that died. It triggers like any
// other ability — through fireTrigger, so the card's triggerCondition and the
// "[Temporary] effects here don't trigger" suppression apply. Karthus,
// Eternal: "Your Deathknell effects trigger an additional time." —
// `deathknell_double_count` is bumped per controlled Karthus by
// recalculateAuras, and each adds one more chain item. Zero in the default case.
void TriggerManager::queueOwnDeathTrigger(GameObjectId object, PlayerId controller) {
    if (!fireTrigger(object, controller, 0, TriggerType::WhenIDie, /*subject=*/object)) return;
    const auto& obj = state_.getObject(object);
    // Only a [Deathknell] ability is repeated: a plain "When I die ..." (Draven,
    // Audacious "When I die in combat") is not a Deathknell effect.
    const int extra = obj.hasKeyword(Keyword::Deathknell)
        ? std::max(0, state_.player(controller).deathknell_double_count) : 0;
    for (int i = 0; i < extra; ++i)
        fireTrigger(object, controller, 0, TriggerType::WhenIDie, /*subject=*/object);
    if (extra > 0) {
        events_.logTrace("DEATHKNELL_DOUBLE: " + obj.name + " fires " +
                         std::to_string(1 + extra) + " times (Karthus aura)");
    }
}

void TriggerManager::onLeftBoard(const LeftBoardEvent& e) {
    // "When you recycle a rune" — a rune went from the board to its Rune Deck
    // (recycled for Power, or by an effect). Fires on its controller's cards
    // and legend. (Battle Mistress 517.)
    if (e.card_type == CardType::Rune && e.destination == ZoneType::RuneDeck &&
        e.controller != PlayerId::None) {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != e.controller) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouRecycleARune))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, e.controller, 0, TriggerType::WhenYouRecycleARune,
                        /*subject=*/e.object);
    }
    // A killed gear "dies" like a unit does, but no UnitDiedEvent is emitted
    // for it: this is where its own "When this is killed" / [Deathknell] fires
    // (Scrapheap, Sprite Fountain, Treasure Trove). Units are handled by
    // onUnitDied.
    if (!state_.objectExists(e.object)) return;
    // Off the board the Might-threshold edge is forgotten (onStateSettled).
    state_.getObject(e.object).card_counters.erase("__at_might_threshold");
    const auto& gear = state_.getObject(e.object);
    if (gear.card_def_id == kInvalidId || e.controller == PlayerId::None) return;
    // "When this leaves the board," — any departure, a kill included
    // (Treasure Trove). The card is already in its new zone.
    if (cardFiresOn(card_registry_, gear.card_def_id, TriggerType::WhenILeaveTheBoard))
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenILeaveTheBoard, e.object);
    if (!e.was_killed || e.card_type != CardType::Gear) return;
    if (!cardFiresOn(card_registry_, gear.card_def_id, TriggerType::WhenIDie)) return;
    queueOwnDeathTrigger(e.object, e.controller);
}

void TriggerManager::onScore(const ScoreEvent& e) {
    auto bf_loc = BattlefieldLocation{e.battlefield};
    auto units = state_.unitsAt(bf_loc, e.player);
    // Everything queued from here on is one of this score's abilities.
    const ChainItemId first_new_item = state_.chain.next_id;

    // Legend-zone sweep first: legends like Green Father / Gloomist /
    // Deceiver have "When you conquer or hold," text encoded as the
    // WhenIConquerOrHold trigger on a LegendCard subclass. The legend
    // lives in champion zone, so the unit loop below won't reach it.
    // The subject is the scored battlefield's card object, so "there" /
    // "if you assigned N excess damage" can tell which battlefield it was
    // (card_helpers.h: triggeringBattlefield).
    GameObjectId scored_bf_card = kInvalidId;
    for (const auto& bf : state_.battlefields)
        if (bf.id == e.battlefield) scored_bf_card = bf.card_object_id;
    if (e.method == ScoreMethod::Conquer) {
        fireLegendTrigger(TriggerType::WhenIConquer, e.player, 0, scored_bf_card);
    }
    if (e.method == ScoreMethod::Hold) {
        fireLegendTrigger(TriggerType::WhenIHold, e.player, 0, scored_bf_card);
    }
    fireLegendTrigger(TriggerType::WhenIConquerOrHold, e.player, 0, scored_bf_card);
    // "When they hold, ..." as a delayed ability (Ashe, Focused), keyed to the
    // player who held.
    if (e.method == ScoreMethod::Hold)
        checkDelayedAbilities(TriggerType::WhenIHold, e.player, kInvalidId);

    // "When you conquer, ... return this from your trash to your hand": a
    // spell whose ability works from the trash (Super Mega Death Rocket!)
    // declares WhenIConquer; it triggers for its owner's conquers while it is
    // in their trash.
    if (e.method == ScoreMethod::Conquer) {
        const auto trash = state_.player(e.player).trash;   // copy: fireTrigger may edit zones
        for (auto cid : trash) {
            if (!state_.objectExists(cid)) continue;
            const auto& c = state_.getObject(cid);
            if (!c.isSpell() || c.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, c.card_def_id, TriggerType::WhenIConquer))
                fireTrigger(cid, e.player, 0, TriggerType::WhenIConquer, scored_bf_card);
        }
    }

    for (auto uid : units) {
        if (!state_.objectExists(uid)) continue;
        auto& obj = state_.getObject(uid);
        CardDefId def = obj.card_def_id;

        // Generic "this unit conquered this turn" marker. Stamped with the
        // current turn number so a stale value from a prior turn never reads
        // as a conquer (no reset needed). Cards like Blighted Battleaxe read
        // it to ask "did the equipped unit conquer this turn?". Mirrors the
        // __defend_attacker_id capture pattern.
        if (e.method == ScoreMethod::Conquer) {
            obj.card_counters["__conquered_turn"] = state_.turn.turn_number;
        }

        if (e.method == ScoreMethod::Conquer &&
            cardFiresOn(card_registry_, def, TriggerType::WhenIConquer)) {
            fireTrigger(uid, e.player, 0, TriggerType::WhenIConquer);
        }
        // 'This turn, that unit has "When I conquer, ..."' (Relentless Pursuit):
        // a delayed ability scoped to this unit.
        if (e.method == ScoreMethod::Conquer)
            checkDelayedAbilities(TriggerType::WhenIConquer, e.player, uid);
        if (e.method == ScoreMethod::Hold &&
            cardFiresOn(card_registry_, def, TriggerType::WhenIHold)) {
            fireTrigger(uid, e.player, 0, TriggerType::WhenIHold);
        }
        if (cardFiresOn(card_registry_, def, TriggerType::WhenIConquerOrHold)) {
            fireTrigger(uid, e.player, 0, TriggerType::WhenIConquerOrHold);
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

        // Skyfall of Areion (CR): "My hold effects are also conquer effects,
        // and vice versa." If an attached gear declares the cross-fire
        // property, also fire the OPPOSITE score-type triggers on this unit
        // and its equipped gear.
        bool crossfire = false;
        for (auto gid : obj.attachments) {
            if (!state_.objectExists(gid)) continue;
            Card* gc = card_registry_.get(state_.getObject(gid).card_def_id);
            if (gc && gc->crossesHoldConquerTriggers()) { crossfire = true; break; }
        }
        if (crossfire) {
            if (e.method == ScoreMethod::Conquer) {
                // Conquer also counts as hold → fire hold effects.
                if (cardFiresOn(card_registry_, def, TriggerType::WhenIHold))
                    fireTrigger(uid, e.player, 0, TriggerType::WhenIHold);
                fireEquippedTriggers(uid, e.player, TriggerType::WhenIHold);
            } else if (e.method == ScoreMethod::Hold) {
                // Hold also counts as conquer → fire conquer effects.
                if (cardFiresOn(card_registry_, def, TriggerType::WhenIConquer))
                    fireTrigger(uid, e.player, 0, TriggerType::WhenIConquer);
                fireEquippedTriggers(uid, e.player, TriggerType::WhenIConquer);
            }
        }
    }

    // Battlefield triggers
    for (auto& bf : state_.battlefields) {
        if (bf.id != e.battlefield) continue;
        if (!state_.objectExists(bf.card_object_id)) continue;

        CardDefId bf_def = state_.getObject(bf.card_object_id).card_def_id;

        if (e.method == ScoreMethod::Conquer &&
            cardFiresOn(card_registry_, bf_def, TriggerType::WhenYouConquerHere)) {
            fireTrigger(bf.card_object_id, e.player, 0, TriggerType::WhenYouConquerHere);
        }
        if (e.method == ScoreMethod::Hold &&
            cardFiresOn(card_registry_, bf_def, TriggerType::WhenYouHoldHere)) {
            fireTrigger(bf.card_object_id, e.player, 0, TriggerType::WhenYouHoldHere);
        }
        if (cardFiresOn(card_registry_, bf_def, TriggerType::WhenYouScoreHere)) {
            fireTrigger(bf.card_object_id, e.player, 0, TriggerType::WhenYouScoreHere);
        }
    }

    // "Your hold effects for holding here trigger an additional time." (Blue
    // Sentinel) / "Your conquer effects for conquering here ..." (Red
    // Brambleback): each card that says so (Card::extraScoreTriggers) makes
    // every ability the scoring player's hold / conquer just triggered — on
    // units here, their Equipment, the legend, the battlefield, delayed
    // abilities — trigger once more. Each copy is its own pending ability:
    // its choices are made separately when it is finalized.
    {
        int extra = 0;
        for (const auto& [id, obj] : state_.objects) {
            if (obj.controller != e.player || !obj.location.has_value()) continue;
            const Card* card = card_registry_.get(obj.card_def_id);
            // (Once more per Equipment that copies the unit's text.)
            if (card)
                extra += std::max(0, card->extraScoreTriggers(
                    state_, id, e.player, e.battlefield, e.method == ScoreMethod::Hold)) *
                    (1 + (effect_executor_ ? effect_executor_->textCopiesOn(id) : 0));
        }
        if (extra > 0) {
            std::vector<ChainItem> triggered;
            for (const auto& ci : state_.chain.items)
                if (ci.id >= first_new_item && ci.controller == e.player &&
                    ci.is_ability && !ci.is_activated_ability)
                    triggered.push_back(ci);
            for (int n = 0; n < extra; ++n)
                for (ChainItem copy : triggered) {
                    copy.id = state_.chain.allocateId();
                    state_.chain.items.push_back(std::move(copy));
                }
            if (!triggered.empty())
                events_.logTrace("SCORE_TRIGGERS: " + std::to_string(triggered.size()) +
                                 " ability(ies) trigger " + std::to_string(extra) +
                                 " additional time(s)");
        }
    }

    // "When an opponent scores" — fires on the non-scoring player's on-board cards.
    PlayerId opp = opponent(e.player);
    std::vector<GameObjectId> opp_watchers;
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != opp || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id,
                        TriggerType::WhenOpponentScores)) {
            opp_watchers.push_back(id);
        }
    }
    for (auto wid : opp_watchers)
        fireTrigger(wid, opp, 0, TriggerType::WhenOpponentScores);
}

void TriggerManager::onUnitMoved(const UnitMovedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    auto& obj = state_.getObject(e.object);

    // Where it moved from and to, for every ability this move triggers
    // (ChainItem::triggering_move): "play a token there", "a battlefield I
    // moved to or from", "to the same battlefield". Cleared when this returns.
    struct Announcing {
        ChainState& chain;
        std::optional<ChainItem::Move> outer;
        Announcing(ChainState& c, const UnitMovedEvent& ev)
            : chain(c), outer(c.move_being_announced) {
            chain.move_being_announced = ChainItem::Move{ev.from, ev.to};
        }
        ~Announcing() { chain.move_being_announced = outer; }
    } announcing(state_.chain, e);
    if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIMove)) {
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenIMove);
    }
    if (std::holds_alternative<BattlefieldLocation>(e.to) &&
        cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIMoveToFB)) {
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenIMoveToFB);
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
            if (cardFiresOn(card_registry_, other.card_def_id,
                            TriggerType::WhenAFriendlyUnitMovesToFB)) {
                watchers.push_back(id);
            }
        }
        for (auto wid : watchers)
            fireTrigger(wid, e.controller, 0, TriggerType::WhenAFriendlyUnitMovesToFB);
    }

    // "When a unit moves from here" — battlefield-card trigger on the FROM bf;
    // the moved unit is the subject (e.g. Back-Alley Bar gives it +1 [M]).
    if (std::holds_alternative<BattlefieldLocation>(e.from)) {
        BattlefieldId from_bf = std::get<BattlefieldLocation>(e.from).id;
        for (auto& bf : state_.battlefields) {
            if (bf.id != from_bf || !state_.objectExists(bf.card_object_id)) continue;
            CardDefId bf_def = state_.getObject(bf.card_object_id).card_def_id;
            if (cardFiresOn(card_registry_, bf_def,
                            TriggerType::WhenAUnitMovesFromHere)) {
                fireTrigger(bf.card_object_id, e.controller, 0,
                            TriggerType::WhenAUnitMovesFromHere, /*subject=*/e.object);
            }
        }
    }

    // "When an opponent moves to a battlefield" — fires on the non-mover's
    // on-board cards; the moved unit is the subject (read its location for the
    // destination). The card gates (e.g. Volibear "...other than mine").
    if (std::holds_alternative<BattlefieldLocation>(e.to)) {
        PlayerId opp = opponent(e.controller);
        std::vector<GameObjectId> opp_watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != opp || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenAnOpponentMovesToBattlefield)) {
                opp_watchers.push_back(id);
            }
        }
        for (auto wid : opp_watchers)
            fireTrigger(wid, opp, 0, TriggerType::WhenAnOpponentMovesToBattlefield,
                        /*subject=*/e.object);
    }

    // "When you move an enemy unit" — a spell or ability moved a unit its
    // controller does not control; the mover is the controller of the item
    // that is resolving (as for "can't be moved by enemy spells and
    // abilities"). The moved unit is the subject. (Blast Cone 695.)
    if (!e.is_standard_move && state_.chain.resuming.has_value() &&
        state_.chain.resuming->controller != e.controller) {
        const PlayerId mover = state_.chain.resuming->controller;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != mover) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouMoveAnEnemyUnit))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, mover, 0, TriggerType::WhenYouMoveAnEnemyUnit,
                        /*subject=*/e.object);
    }

    // "When a friendly unit moves from my location" — fires on the controller's
    // OTHER on-board cards that shared the FROM location (Stealthy Pursuer);
    // the moved unit is the subject (read its new location to follow it).
    {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (id == e.object) continue;
            if (obj.controller != e.controller || !obj.location.has_value()) continue;
            if (!(*obj.location == e.from)) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenAFriendlyUnitMovesFromMyLocation))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, e.controller, 0,
                        TriggerType::WhenAFriendlyUnitMovesFromMyLocation,
                        /*subject=*/e.object);
    }
}

void TriggerManager::onPhaseChanged(const PhaseChangedEvent& e) {
    // "At the end of your turn"
    if (e.new_phase == TurnPhase::EndingStep) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::AtEndOfTurn)) {
                fireTrigger(id, e.turn_player, 0, TriggerType::AtEndOfTurn);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            if (cardFiresOn(card_registry_,
                    state_.getObject(ps.legend_zone).card_def_id,
                    TriggerType::AtEndOfTurn)) {
                fireTrigger(ps.legend_zone, e.turn_player, 0, TriggerType::AtEndOfTurn);
            }
        }
        // An Equipment's "At the end of your turn, ..." in its effect text
        // speaks for the unit it is attached to (Blighted Battleaxe).
        {
            std::vector<GameObjectId> bearers;
            for (auto& [id, obj] : state_.objects)
                if (obj.isUnit() && obj.location.has_value() &&
                    obj.controller == e.turn_player && !obj.attachments.empty())
                    bearers.push_back(id);
            for (auto uid : bearers)
                fireEquippedTriggers(uid, e.turn_player, TriggerType::AtEndOfTurn);
        }
        // Battlefield cards with an AtEndOfTurn trigger.
        fireBattlefieldTriggers(TriggerType::AtEndOfTurn, e.turn_player);
        // Delayed abilities scheduled for end of turn (e.g. Targon's Peak's
        // "ready up to 2 runes at the end of this turn").
        checkDelayedAbilities(TriggerType::AtEndOfTurn, e.turn_player, kInvalidId);
        // "At the end of this turn" is not "your turn": one set up by the
        // other player (Hostile Takeover played as a reaction, a conquer on
        // the opponent's turn) fires too, after the turn player's.
        checkDelayedAbilities(TriggerType::AtEndOfTurn, opponent(e.turn_player), kInvalidId);
    }

    // "At the start of your Beginning Phase"
    if (e.new_phase == TurnPhase::BeginningStep) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::AtStartOfBeginning)) {
                fireTrigger(id, e.turn_player, 0, TriggerType::AtStartOfBeginning);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            if (cardFiresOn(card_registry_,
                    state_.getObject(ps.legend_zone).card_def_id,
                    TriggerType::AtStartOfBeginning)) {
                fireTrigger(ps.legend_zone, e.turn_player, 0, TriggerType::AtStartOfBeginning);
            }
        }
        // Battlefield cards with an AtStartOfBeginning trigger (Arena's
        // Greatest, Dusk Rose Lab), attributed to the turn player.
        fireBattlefieldTriggers(TriggerType::AtStartOfBeginning, e.turn_player);
    }

    // "At the start of your Main Phase" — used by Iascylla's delayed
    // "move enemy unit here" and similar. Both static triggers on cards
    // AND delayed abilities (registered via state.delayed_abilities) fire.
    if (e.new_phase == TurnPhase::MainPhase) {
        for (auto& [id, obj] : state_.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != e.turn_player) continue;

            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::AtStartOfMain)) {
                fireTrigger(id, e.turn_player, 0, TriggerType::AtStartOfMain);
            }
        }
        // Legend zone
        auto& ps = state_.player(e.turn_player);
        if (ps.legend_zone != kInvalidId && state_.objectExists(ps.legend_zone)) {
            if (cardFiresOn(card_registry_,
                    state_.getObject(ps.legend_zone).card_def_id,
                    TriggerType::AtStartOfMain)) {
                fireTrigger(ps.legend_zone, e.turn_player, 0, TriggerType::AtStartOfMain);
            }
        }
        // Battlefield cards with an AtStartOfMain trigger.
        fireBattlefieldTriggers(TriggerType::AtStartOfMain, e.turn_player);
        // Delayed abilities (Iascylla schedules one of these on hold)
        checkDelayedAbilities(TriggerType::AtStartOfMain, e.turn_player,
                              kInvalidId);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phase-2 trigger events
// ═══════════════════════════════════════════════════════════════════════════════

void TriggerManager::onUnitReadied(const UnitReadiedEvent& e) {
    // "When you ready me" — fires on the readied object itself (Irelia, Fervent).
    if (!state_.objectExists(e.object)) return;
    if (cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                    TriggerType::WhenIAmReadied)) {
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenIAmReadied);
    }
    // "When I become ready" — effect readies here; Awaken-phase readies arrive
    // as ObjectStateChangedEvent "became_ready".
    if (cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                    TriggerType::WhenIBecomeReady)) {
        fireTrigger(e.object, e.controller, 0, TriggerType::WhenIBecomeReady);
    }
    // "When you ready a friendly unit" — fires on the controller's OTHER on-board
    // cards (e.g. Pirate's Haven), with the readied unit as the subject.
    const PlayerId controller = e.controller;
    std::vector<GameObjectId> others;
    for (auto& [id, obj] : state_.objects) {
        if (id == e.object) continue;
        if (obj.controller != controller || !obj.location.has_value()) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id,
                        TriggerType::WhenYouReadyAFriendlyUnit)) {
            others.push_back(id);
        }
    }
    for (GameObjectId id : others) {
        fireTrigger(id, controller, 0, TriggerType::WhenYouReadyAFriendlyUnit,
                    /*subject=*/e.object);
    }
}

void TriggerManager::onCardsDrawn(const CardsDrawnEvent& e) {
    // Fire WhenYouDrawACard on the drawing player's on-board watchers. The card
    // decides "is this my 2nd draw this turn" via PlayerState::draws_this_turn.
    std::vector<GameObjectId> watchers;
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != e.player || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouDrawACard))
            watchers.push_back(id);
    }
    for (auto wid : watchers)
        fireTrigger(wid, e.player, 0, TriggerType::WhenYouDrawACard);
}

void TriggerManager::onObjectStateChanged(const ObjectStateChangedEvent& e) {
    if (!state_.objectExists(e.object)) return;
    const PlayerId controller = state_.getObject(e.object).controller;

    // "When you use an activated ability of a gear" — fire on the gear
    // controller's on-board cards (Prize of Progress).
    if (e.what_changed == "gear_ability_used") {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != controller || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouActivateAGearAbility))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, 0, TriggerType::WhenYouActivateAGearAbility);
        return;
    }

    // "When a unit you control becomes [Mighty]" — fires on the controller's
    // on-board cards AND the controller's legend; the unit that crossed the
    // 5+ [M] threshold is the subject. (Fiora, Worthy 500 / Grand Duelist 519.)
    if (e.what_changed == "became_mighty") {
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != controller || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenAUnitBecomesMighty))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, 0, TriggerType::WhenAUnitBecomesMighty,
                        /*subject=*/e.object);
        // Legend zone (Grand Duelist is a legend, off-board).
        auto legend_id = state_.player(controller).legend_zone;
        if (legend_id != kInvalidId && state_.objectExists(legend_id) &&
            cardFiresOn(card_registry_, state_.getObject(legend_id).card_def_id,
                        TriggerType::WhenAUnitBecomesMighty))
            fireTrigger(legend_id, controller, 0,
                        TriggerType::WhenAUnitBecomesMighty, /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "chosen_by_controller") {
        // "When you choose a friendly unit" — a spell or ability its own
        // controller played chose this unit (at play or at resolution).
        // The chosen unit is a watcher too ("When you choose me" — Irelia,
        // Fervent; Jae Medarda): the subject tells a card which unit it was,
        // and one that only cares about itself says so in triggerCondition.
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != controller) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouChooseAFriendlyUnit))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, 0, TriggerType::WhenYouChooseAFriendlyUnit,
                        /*subject=*/e.object);
        // Battlefield cards: "when a friendly unit is chosen here".
        if (auto bf = state_.getObject(e.object).battlefieldId())
            fireBattlefieldTriggers(TriggerType::WhenAFriendlyUnitChosenHere, controller, *bf,
                                    /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "became_ready") {
        if (state_.getObject(e.object).card_def_id != kInvalidId &&
            cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                        TriggerType::WhenIBecomeReady))
            fireTrigger(e.object, controller, 0, TriggerType::WhenIBecomeReady);
        return;
    }

    if (e.what_changed == "discarded") {
        // "When you discard me," — the card is already in its owner's trash.
        const auto& dobj = state_.getObject(e.object);
        if (dobj.card_def_id != kInvalidId &&
            cardFiresOn(card_registry_, dobj.card_def_id, TriggerType::WhenIAmDiscarded))
            fireTrigger(e.object, dobj.owner, 0, TriggerType::WhenIAmDiscarded);

        // "When you discard one or more cards," (Jinx, Rebel) on the discarding
        // player's cards. One discard of several cards announces each card, so
        // a source whose ability from this discard is still waiting to be
        // finalized does not trigger again.
        const PlayerId discarder = dobj.owner;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != discarder || obj.card_def_id == kInvalidId) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (!cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouDiscard)) continue;
            bool pending = false;
            for (const auto& ci : state_.chain.items)
                if (ci.source == id && ci.status == ChainItemStatus::Pending &&
                    ci.fired_trigger == TriggerType::WhenYouDiscard) pending = true;
            if (!pending) watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, discarder, 0, TriggerType::WhenYouDiscard, /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "ability_activated") {
        // "When you play ... an activated ability" (Curator of the Sands).
        int energy = 0;
        if (auto it = state_.getObject(e.object).card_counters.find("__ability_energy_cost");
            it != state_.getObject(e.object).card_counters.end()) energy = it->second;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != controller) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouActivateAnAbility))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, energy, TriggerType::WhenYouActivateAnAbility,
                        /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "played_not_from_hand") {
        // "When you play a card from anywhere other than your hand," (trash via
        // Flow, face down, Champion Zone).
        state_.player(controller).cards_played_not_from_hand_this_turn++;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (id == e.object || obj.controller != controller) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouPlayACardNotFromHand))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, 0, TriggerType::WhenYouPlayACardNotFromHand,
                        /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "empowered") {
        // "When I become [Empowered]" on the object itself, then "When you
        // empower something else" on the controller's other cards + legend.
        if (state_.getObject(e.object).card_def_id != kInvalidId &&
            cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                        TriggerType::WhenIBecomeEmpowered))
            fireTrigger(e.object, controller, 0, TriggerType::WhenIBecomeEmpowered);
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (id == e.object || obj.controller != controller) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouEmpower))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, controller, 0, TriggerType::WhenYouEmpower,
                        /*subject=*/e.object);
        return;
    }

    if (e.what_changed == "burned" || e.what_changed == "banished_owned") {
        // Fires for the card's OWNER ("When you burn ...", "When you banish a
        // card you own"); subject = the burned / banished card.
        const PlayerId owner = state_.getObject(e.object).owner;
        const TriggerType t = e.what_changed == "burned"
            ? TriggerType::WhenYouBurn : TriggerType::WhenYouBanishACard;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != owner) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id, t)) watchers.push_back(id);
        }
        for (auto wid : watchers) fireTrigger(wid, owner, 0, t, /*subject=*/e.object);
        return;
    }

    // "When you attach an Equipment to me" — fires on the equipped unit itself
    // (EffectExecutor::attachGear emits "equipped"). (Aphelios, Exalted 372.)
    if (e.what_changed == "equipped") {
        if (cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                        TriggerType::WhenEquipmentAttachedToMe))
            fireTrigger(e.object, controller, 0,
                        TriggerType::WhenEquipmentAttachedToMe);
        return;
    }

    // "When you recycle one or more cards to your Main Deck" — the recycled
    // card's OWNER is the recycling player (it landed in their deck); fire on
    // their on-board cards. (Karma, Channeler 235.)
    if (e.what_changed == "recycled_main") {
        const PlayerId owner = state_.getObject(e.object).owner;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != owner || !obj.location.has_value()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id,
                            TriggerType::WhenYouRecycle))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, owner, 0, TriggerType::WhenYouRecycle);
        return;
    }

    // "When you spend a buff" (EffectExecutor::spendBuff) — the spender's
    // cards in play and legend; the unit that had the buff is the subject.
    if (e.what_changed == "buff_spent") {
        PlayerId spender = controller;
        auto& counters = state_.getObject(e.object).card_counters;
        if (auto it = counters.find("__buff_spent_by"); it != counters.end()) {
            spender = static_cast<PlayerId>(it->second);
            counters.erase(it);
        }
        if (spender == PlayerId::None) return;
        std::vector<GameObjectId> watchers;
        for (auto& [id, obj] : state_.objects) {
            if (obj.controller != spender || obj.card_def_id == kInvalidId) continue;
            if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
            if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouSpendABuff))
                watchers.push_back(id);
        }
        for (auto wid : watchers)
            fireTrigger(wid, spender, 0, TriggerType::WhenYouSpendABuff, /*subject=*/e.object);
        return;
    }

    // Only the "buffed" state change drives the remaining triggers today.
    if (e.what_changed != "buffed") return;
    // "When you buff me" — fires on the buffed object itself (Simian Ancestor).
    if (cardFiresOn(card_registry_, state_.getObject(e.object).card_def_id,
                    TriggerType::WhenIAmBuffed)) {
        fireTrigger(e.object, controller, 0, TriggerType::WhenIAmBuffed);
    }
    // "When you buff a friendly unit" — fires on the controller's OTHER on-board
    // cards (e.g. Mistfall gear). Snapshot ids first; fireTrigger mutates the chain.
    std::vector<GameObjectId> others;
    for (auto& [id, obj] : state_.objects) {
        if (id == e.object) continue;
        if (obj.controller != controller || !obj.location.has_value()) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id,
                        TriggerType::WhenYouBuffAFriendlyUnit)) {
            others.push_back(id);
        }
    }
    for (GameObjectId id : others) {
        fireTrigger(id, controller, 0, TriggerType::WhenYouBuffAFriendlyUnit,
                    /*subject=*/e.object);
    }
}

void TriggerManager::onCardHidden(const CardHiddenEvent& e) {
    // "You can look at their facedown cards this turn." (Scuttle Crab) covers
    // a card hidden after the permission was given: the other player is shown
    // it as it is hidden. A look, not an ability — nothing goes on the chain.
    {
        const PlayerId looker = opponent(e.player);
        if (state_.player(looker).may_look_at_opp_facedown_turn == state_.turn.turn_number &&
            state_.objectExists(e.card)) {
            const auto& hidden = state_.getObject(e.card);
            events_.emit(CardRevealedEvent{e.card, hidden.card_def_id, hidden.owner,
                                           /*revealed_to_all=*/false, /*revealed_to=*/looker,
                                           ZoneType::FacedownZone});
            knowledge::seen(state_, e.card, deck::Place::Facedown, knowledge::only(looker));
        }
    }
    checkDelayedAbilities(TriggerType::WhenAnOpponentHidesACard, opponent(e.player), e.card);
    // "When you hide a card" — fires on the hiding player's on-board cards
    // (Katarina, Reckless: "ready me").
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != e.player || !obj.location.has_value()) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouHideACard)) {
            fireTrigger(id, e.player, 0, TriggerType::WhenYouHideACard);
        }
    }
}

void TriggerManager::onPlayedFromFacedown(const PlayedFromFacedownEvent& e) {
    // "When you play a card from face down" — fires on the controller's
    // on-board cards (Katarina, Reckless: "deal 2 to an enemy unit").
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != e.player || !obj.location.has_value()) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouPlayFromFacedown)) {
            fireTrigger(id, e.player, 0, TriggerType::WhenYouPlayFromFacedown);
        }
    }
}

void TriggerManager::onUnitReturnedToHand(const UnitReturnedToHandEvent& e) {
    // "When a unit here is returned to a player's hand" — battlefield-card
    // trigger (Ripper's Bay). Attributed to the bounced unit's owner ("that
    // player may pay [1] …").
    for (auto& bf : state_.battlefields) {
        if (bf.id != e.from_battlefield) continue;
        if (!state_.objectExists(bf.card_object_id)) continue;
        if (cardFiresOn(card_registry_, state_.getObject(bf.card_object_id).card_def_id,
                        TriggerType::WhenAUnitReturnsToHandHere)) {
            fireTrigger(bf.card_object_id, e.owner, 0, TriggerType::WhenAUnitReturnsToHandHere);
        }
    }
}

void TriggerManager::onShowdownStarted(const ShowdownStartedEvent& e) {
    // "When a showdown begins here" — fires on units at the showdown's
    // battlefield (Diana, Lunari) and on the battlefield card itself.
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit()) continue;
        auto bf = obj.battlefieldId();
        if (!bf || *bf != e.battlefield) continue;
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenAShowdownBeginsHere)) {
            fireTrigger(id, obj.controller, 0, TriggerType::WhenAShowdownBeginsHere);
        }
    }
    fireBattlefieldTriggers(TriggerType::WhenAShowdownBeginsHere, PlayerId::None,
                            e.battlefield);
}

void TriggerManager::onCardRevealed(const CardRevealedEvent& e) {
    // "As you look at or reveal me from the top of your deck" — fires on the
    // revealed card itself when it was on top of the Main Deck (Nocturne).
    if (e.source_zone != ZoneType::MainDeck) return;
    if (!state_.objectExists(e.card)) return;
    if (cardFiresOn(card_registry_, state_.getObject(e.card).card_def_id,
                    TriggerType::WhenIRevealedFromTop)) {
        fireTrigger(e.card, e.owner, 0, TriggerType::WhenIRevealedFromTop);
    }
}

} // namespace riftbound
