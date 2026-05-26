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
}

TriggerManager::~TriggerManager() {
    for (auto& c : connections_) {
        if (c.connected()) c.disconnect();
    }
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
        // Stamp the originating trigger so the card's onTrigger can branch on
        // ctx.firing_trigger (e.g. Grim Resolve's WhenIWinCombat XP rider,
        // Targon's Peak's AtEndOfTurn ready-runes).
        if (!state_.chain.items.empty()) {
            state_.chain.items.back().fired_trigger = trigger;
        }
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

// Multi-trigger-aware membership test: true if the card declares `t` among
// its trigger types. Use this (not `getTrigger(...) == t`) so cards that
// register more than one trigger fire on each.
static bool cardFiresOn(const CardRegistry& reg, CardDefId def_id, TriggerType t) {
    Card* card = reg.get(def_id);
    return card && card->firesOn(t);
}

void TriggerManager::fireBattlefieldTriggers(TriggerType t, PlayerId player,
                                              BattlefieldId at_battlefield) {
    for (auto& bf : state_.battlefields) {
        if (at_battlefield != kInvalidId && bf.id != at_battlefield) continue;
        if (!state_.objectExists(bf.card_object_id)) continue;
        if (cardFiresOn(card_registry_, state_.getObject(bf.card_object_id).card_def_id, t)) {
            fireTrigger(bf.card_object_id, player, 0, t);
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
                                        int triggering_spell_energy_spent) {
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        if (relevant_player != PlayerId::None && pid != relevant_player) continue;
        auto legend_id = state_.player(pid).legend_zone;
        if (legend_id == kInvalidId || !state_.objectExists(legend_id)) continue;
        auto& legend = state_.getObject(legend_id);
        if (cardFiresOn(card_registry_, legend.card_def_id, t)) {
            fireTrigger(legend_id, pid, triggering_spell_energy_spent, t);
        }
    }
}

void TriggerManager::fireTrigger(GameObjectId source, PlayerId controller,
                                  int triggering_spell_energy_spent,
                                  TriggerType which,
                                  GameObjectId subject) {
    if (!state_.objectExists(source)) return;
    auto& obj = state_.getObject(source);

    // Skip triggers on gear with inactive rules text (CR 718.2)
    if (obj.is_rules_text_inactive) return;

    // For single-trigger cards `which` may be None; resolve it from the
    // card so multi-trigger cards still record which event fired.
    TriggerType fired = which;
    if (fired == TriggerType::None) {
        fired = getTrigger(card_registry_, obj.card_def_id);
    }
    if (fired == TriggerType::None) return;

    events_.logDebug("TRIGGER: " + obj.name + " (" + toString(controller) +
                     ") fires ability");
    chain_.addAbility(source, controller, obj.card_def_id);
    // Snapshot the triggering spell's spent value on the just-added
    // chain item so resume-time reads aren't subject to clobber by a
    // subsequent spell-play. Phase 6q+ engine-audit follow-on for
    // Virtuoso / Forgotten Library correctness.
    if (!state_.chain.items.empty()) {
        state_.chain.items.back().fired_trigger = fired;
        if (triggering_spell_energy_spent > 0) {
            state_.chain.items.back().triggering_spell_energy_spent =
                triggering_spell_energy_spent;
        }
        if (subject != kInvalidId) {
            state_.chain.items.back().triggering_subject = subject;
        }
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
    if (obj.isPermanent()) {
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenYouPlayMe)) {
            fireTrigger(e.object, e.player, 0, TriggerType::WhenYouPlayMe);
        }
        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::AsYouPlayMe)) {
            fireTrigger(e.object, e.player, 0, TriggerType::AsYouPlayMe);
        }
    }

    // "When you play a unit" / "When you play a spell" — fire on OTHER objects
    for (auto& [id, other] : state_.objects) {
        if (!other.location.has_value()) continue;
        if (other.controller != e.player) continue;
        if (id == e.object) continue;

        if (e.card_type == CardType::Unit &&
            cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayAUnit)) {
            fireTrigger(id, e.player, 0, TriggerType::WhenYouPlayAUnit);
        }
        if (e.card_type == CardType::Gear &&
            cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayAGear)) {
            fireTrigger(id, e.player, 0, TriggerType::WhenYouPlayAGear);
        }
        if (e.card_type == CardType::Spell &&
            cardFiresOn(card_registry_, other.card_def_id, TriggerType::WhenYouPlayASpell)) {
            // Snapshot triggering-spell spent so Virtuoso /
            // Forgotten Library read the correct value at resolve
            // time (not whichever PlayerState::last_spell_energy_spent
            // is at chain-drain time).
            fireTrigger(id, e.player, e.energy_spent, TriggerType::WhenYouPlayASpell);
        }
    }

    // Battlefield cards with "When you play a unit here" (e.g. Star Spring).
    // Fire only on the battlefield the just-played unit landed at.
    if (e.card_type == CardType::Unit && state_.objectExists(e.object)) {
        auto played_bf = state_.getObject(e.object).battlefieldId();
        if (played_bf) {
            fireBattlefieldTriggers(TriggerType::WhenYouPlayAUnit, e.player, *played_bf);
        }
    }

    // "When an opponent plays a unit" — fire on the OPPONENT's on-board cards
    // (Vex, Apathetic: gated "while I'm at a battlefield"; stuns the played
    // unit). The played unit's id is captured into the watcher's card_counters
    // so its onTrigger can act on it (triggers receive empty targets).
    if (e.card_type == CardType::Unit) {
        PlayerId opp = opponent(e.player);
        for (auto& [id, other] : state_.objects) {
            if (other.controller != opp || !other.location.has_value()) continue;
            if (cardFiresOn(card_registry_, other.card_def_id,
                            TriggerType::WhenOpponentPlaysAUnit)) {
                other.card_counters["__opp_played_unit_id"] = static_cast<int>(e.object);
                fireTrigger(id, opp, 0, TriggerType::WhenOpponentPlaysAUnit);
            }
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
                if (cardFiresOn(card_registry_, other.card_def_id,
                                TriggerType::WhenYouChooseAFriendlyUnit)) {
                    fireTrigger(id, e.player, 0,
                                TriggerType::WhenYouChooseAFriendlyUnit);
                }
            }
        }
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
    if (state_.objectExists(e.object)) {
        auto& obj = state_.getObject(e.object);

        if (cardFiresOn(card_registry_, obj.card_def_id, TriggerType::WhenIDie)) {
            // Karthus, Eternal: "Your Deathknell effects trigger an
            // additional time." `deathknell_double_count` is bumped per
            // controlled Karthus by recalculateAuras. Each unit adds
            // (1 + count) chain items. Empty in the default case so the
            // existing single-fire path is unchanged.
            int extra = state_.player(e.controller).deathknell_double_count;
            int fires = 1 + std::max(0, extra);
            for (int i = 0; i < fires; ++i) {
                chain_.addAbility(e.object, e.controller, obj.card_def_id);
                if (!state_.chain.items.empty()) {
                    state_.chain.items.back().fired_trigger = TriggerType::WhenIDie;
                }
            }
            if (extra > 0) {
                events_.logTrace("DEATHKNELL_DOUBLE: " + obj.name + " fires " +
                                 std::to_string(fires) + " times (Karthus aura)");
            }
        }
    }

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
            fireTrigger(id, e.controller, 0, TriggerType::WhenAFriendlyUnitDies);
        }
        if (other.controller == opp &&
            cardFiresOn(card_registry_, other.card_def_id,
                        TriggerType::WhenAnEnemyUnitDies)) {
            fireTrigger(id, opp, 0, TriggerType::WhenAnEnemyUnitDies);
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
        // Battlefield cards with an AtEndOfTurn trigger.
        fireBattlefieldTriggers(TriggerType::AtEndOfTurn, e.turn_player);
        // Delayed abilities scheduled for end of turn (e.g. Targon's Peak's
        // "ready up to 2 runes at the end of this turn").
        checkDelayedAbilities(TriggerType::AtEndOfTurn, e.turn_player, kInvalidId);
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
    // Only the "buffed" state change drives triggers today.
    if (e.what_changed != "buffed") return;
    if (!state_.objectExists(e.object)) return;
    const PlayerId controller = state_.getObject(e.object).controller;
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
