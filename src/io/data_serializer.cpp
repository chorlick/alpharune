#include "data_serializer.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace riftbound {

DataSerializer::DataSerializer(const CardDB& db, const std::string& output_path)
    : db_(db), file_(output_path, std::ios::out | std::ios::trunc) {
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }
}

DataSerializer::~DataSerializer() {
    if (file_.is_open()) file_.close();
}

void DataSerializer::flush() {
    file_.flush();
}

// ─── Decision recording ──────────────────────────────────────────────────────

void DataSerializer::recordDecision(const GameState& state,
                                     const std::vector<Intent>& legal_actions,
                                     const Intent& chosen_action) {
    state_ptr_ = &state;  // enable object lookups in serializeIntent

    json record;
    record["type"] = "decision";
    record["decision_index"] = state.decision_index;
    record["turn"] = state.turn.turn_number;
    record["phase"] = toString(state.turn.phase);
    record["turn_player"] = toString(state.turn.turn_player);
    record["state"] = serializeState(state);
    record["legal_action_count"] = legal_actions.size();
    record["legal_actions"] = serializeActions(legal_actions);
    record["chosen_action"] = serializeIntent(chosen_action);

    // Find chosen action index within legal actions. Use full Intent
    // equality — a partial-field match collapses "twin" intents (same
    // primary fields, different destinations/objects/etc.) and biases
    // chosen_index toward 0. See binary_data_serializer.cpp.
    for (int i = 0; i < static_cast<int>(legal_actions.size()); ++i) {
        if (legal_actions[i] == chosen_action) {
            record["chosen_index"] = i;
            break;
        }
    }

    file_ << record.dump(-1) << "\n";  // -1 = compact, no pretty-print
}

void DataSerializer::recordGameSummary(const GameState& state,
                                        const std::string& game_id) {
    json summary;
    summary["type"] = "game_summary";
    summary["game_id"] = game_id;
    summary["winner"] = toString(state.winner);
    summary["final_scores"] = {
        state.players[0].score,
        state.players[1].score
    };
    summary["total_turns"] = state.turn.turn_number;
    summary["total_decisions"] = state.decision_index;
    summary["termination"] = state.game_over_reason;

    file_ << summary.dump(-1) << "\n";
    flush();
}

// ─── State serialization ─────────────────────────────────────────────────────

json DataSerializer::serializeState(const GameState& state) const {
    json s;
    s["starting_player"] = toString(state.turn.starting_player);
    s["ns_state"] = state.turn.ns_state == NeutralShowdownState::Showdown
                    ? "showdown" : "neutral";
    s["oc_state"] = state.turn.oc_state == OpenClosedState::Closed
                    ? "closed" : "open";
    s["is_additional_turn"] = state.turn.is_additional_turn;
    s["delayed_ability_count"] = static_cast<int>(state.delayed_abilities.size());

    s["player1"] = serializePlayer(state, PlayerId::Player1);
    s["player2"] = serializePlayer(state, PlayerId::Player2);

    json bfs = json::array();
    for (auto& bf : state.battlefields) {
        bfs.push_back(serializeBattlefield(state, bf));
    }
    s["battlefields"] = bfs;

    // Chain
    json chain = json::array();
    if (state.chain.exists()) {
        for (auto& item : state.chain.items) {
            json ci;
            ci["id"] = item.id;
            ci["controller"] = toString(item.controller);
            ci["status"] = item.status == ChainItemStatus::Pending
                           ? "pending" : "finalized";
            if (state.objectExists(item.source)) {
                ci["source"] = state.getObject(item.source).name;
                ci["source_card_def_id"] = state.getObject(item.source).card_def_id;
            }
            // Target def_ids for this chain item (Tier 1 feature).
            json tgts = json::array();
            for (auto t : item.targets) {
                if (state.objectExists(t))
                    tgts.push_back(state.getObject(t).card_def_id);
            }
            ci["target_def_ids"] = tgts;
            chain.push_back(ci);
        }
    }
    s["chain"] = chain;

    return s;
}

json DataSerializer::serializePlayer(const GameState& state,
                                      PlayerId player) const {
    auto& ps = state.player(player);
    json p;
    p["score"] = ps.score;
    p["xp"] = ps.xp;
    p["hand_size"] = ps.hand.size();
    p["deck_size"] = ps.main_deck.size();
    p["rune_deck_size"] = ps.rune_deck.size();
    p["energy"] = ps.rune_pool.energy;
    p["cards_played_this_turn"] = ps.cards_played_this_turn;
    p["burned_out"] = ps.burned_out;

    // Power breakdown
    json power;
    for (int i = 0; i < static_cast<int>(Domain::Count); ++i) {
        if (ps.rune_pool.power[i] > 0) {
            power[toString(static_cast<Domain>(i))] = ps.rune_pool.power[i];
        }
    }
    if (ps.rune_pool.universal_power > 0) {
        power["universal"] = ps.rune_pool.universal_power;
    }
    p["power"] = power;

    p["has_discarded_this_turn"] = ps.has_discarded_this_turn;
    p["bfs_scored_this_turn"] = static_cast<int>(ps.battlefields_scored_this_turn.size());
    p["cost_modifier_count"] = static_cast<int>(ps.cost_modifiers.size());
    p["cant_play_cards_this_turn"] = ps.cant_play_cards_this_turn;

    // Per-BF scored flags (Tier 1) — list of battlefield IDs scored this turn.
    json bfs_scored = json::array();
    for (auto bf_id : ps.battlefields_scored_this_turn)
        bfs_scored.push_back(bf_id);
    p["bfs_scored_ids"] = bfs_scored;

    // Cost-modifier type breakdown (Tier 1).
    bool cost_next_spell = false, cost_next_unit = false;
    for (auto& mod : ps.cost_modifiers) {
        if (mod.next_spell_only) cost_next_spell = true;
        if (mod.next_unit_only)  cost_next_unit  = true;
    }
    p["cost_mod_next_spell_only"] = cost_next_spell;
    p["cost_mod_next_unit_only"] = cost_next_unit;

    // Hand card IDs + costs
    json hand_ids = json::array();
    json hand_costs = json::array();
    for (auto id : ps.hand) {
        auto& obj = state.getObject(id);
        hand_ids.push_back(obj.card_def_id);
        if (obj.card_def_id != kInvalidId) {
            auto& def = db_.get(obj.card_def_id);
            hand_costs.push_back(def.energy_cost);
        } else {
            hand_costs.push_back(0);
        }
    }
    p["hand_card_ids"] = hand_ids;
    p["hand_card_costs"] = hand_costs;

    // Trash card IDs (public — both players can see)
    json trash_ids = json::array();
    for (auto id : ps.trash) {
        if (state.objectExists(id))
            trash_ids.push_back(state.getObject(id).card_def_id);
    }
    p["trash_card_ids"] = trash_ids;

    // Deck contents (self-only; opponent's deck is hidden per CR). Order
    // doesn't matter for multi-hot featurization — just the bag of cards.
    json deck_ids = json::array();
    for (auto id : ps.main_deck) {
        if (state.objectExists(id))
            deck_ids.push_back(state.getObject(id).card_def_id);
    }
    p["deck_card_ids"] = deck_ids;

    // Banishment card IDs (public)
    json banish_ids = json::array();
    for (auto id : ps.banishment) {
        if (state.objectExists(id))
            banish_ids.push_back(state.getObject(id).card_def_id);
    }
    p["banishment_card_ids"] = banish_ids;

    // Units in base
    json base_units = json::array();
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.controller == player && obj.isAtBase()) {
            base_units.push_back(serializeObject(state, id));
        }
    }
    p["base_units"] = base_units;

    // Gear in base
    json base_gear = json::array();
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == player && obj.isAtBase()) {
            base_gear.push_back(serializeObject(state, id));
        }
    }
    p["base_gear"] = base_gear;

    // Channeled runes with domain info
    json runes = json::array();
    auto rune_ids = state.runesInBase(player);
    for (auto id : rune_ids) {
        auto& obj = state.getObject(id);
        json r;
        r["card_def_id"] = obj.card_def_id;
        r["exhausted"] = obj.is_exhausted;
        if (obj.card_def_id != kInvalidId) {
            auto& def = db_.get(obj.card_def_id);
            json doms = json::array();
            for (auto d : def.domains) doms.push_back(toString(d));
            r["domains"] = doms;
        }
        runes.push_back(r);
    }
    p["runes"] = runes;

    // Legend
    if (ps.legend_zone != kInvalidId && state.objectExists(ps.legend_zone)) {
        auto& legend = state.getObject(ps.legend_zone);
        p["legend_card_id"] = legend.card_def_id;
        p["legend_exhausted"] = legend.is_exhausted;
    }

    // Champion zone
    if (ps.champion_zone != kInvalidId && state.objectExists(ps.champion_zone)) {
        auto& champ = state.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone) {
            p["champion_in_zone"] = true;
            p["champion_card_id"] = champ.card_def_id;
        } else {
            p["champion_in_zone"] = false;
        }
    }

    return p;
}

json DataSerializer::serializeBattlefield(const GameState& state,
                                           const BattlefieldState& bf) const {
    json b;
    b["id"] = bf.id;
    if (state.objectExists(bf.card_object_id)) {
        auto& card = state.getObject(bf.card_object_id);
        b["name"] = card.name;
        b["card_def_id"] = card.card_def_id;
    }

    b["controller"] = bf.controller.has_value()
        ? toString(*bf.controller) : "none";
    b["contested"] = bf.is_contested;
    b["combat"] = bf.combat_in_progress;
    b["showdown"] = bf.showdown_in_progress;
    b["facedown_count"] = bf.facedown.size();
    b["is_token"] = bf.is_token;

    // Combat context
    if (bf.combat_in_progress) {
        b["attacker"] = bf.attacker.has_value() ? toString(*bf.attacker) : "none";
        int cp = 0;
        switch (bf.combat_phase) {
            case CombatPhase::ShowdownStep: cp = 1; break;
            case CombatPhase::DamageStep: cp = 2; break;
            case CombatPhase::ResolutionStep: cp = 3; break;
            default: break;
        }
        b["combat_phase"] = cp;
    }

    // Units at this battlefield
    json units = json::array();
    auto bf_loc = BattlefieldLocation{bf.id};
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.location.has_value() &&
            *obj.location == LocationId{bf_loc}) {
            units.push_back(serializeObject(state, id));
        }
    }
    b["units"] = units;

    return b;
}

json DataSerializer::serializeObject(const GameState& state,
                                      GameObjectId id) const {
    auto& obj = state.getObject(id);
    json o;
    o["card_def_id"] = obj.card_def_id;
    o["name"] = obj.name;
    o["controller"] = toString(obj.controller);

    if (obj.isUnit()) {
        o["might"] = obj.current_might;
        o["base_might"] = obj.base_might;
        o["damage"] = obj.damage_marked;
        o["temp_might_bonus"] = obj.temp_might_bonus;
    }

    o["exhausted"] = obj.is_exhausted;
    o["stunned"] = obj.is_stunned;
    o["keywords"] = obj.keywords.bits;
    o["buff_count"] = obj.buff_count;
    o["has_attachment"] = !obj.attachments.empty();
    // First attached gear's def_id (0 if none). Tier 1 feature.
    CardDefId att_def = 0;
    for (auto aid : obj.attachments) {
        if (state.objectExists(aid)) {
            att_def = state.getObject(aid).card_def_id;
            break;
        }
    }
    o["attachment_def_id"] = att_def;

    if (obj.combat_designation != CombatDesignation::None) {
        o["combat"] = toString(obj.combat_designation);
    }

    return o;
}

// ─── Intent serialization ────────────────────────────────────────────────────

json DataSerializer::serializeIntent(const Intent& intent) const {
    json i;
    i["type"] = toString(intent.type);
    i["player"] = toString(intent.player);

    // Card involved (card_def_id for ML features)
    if (intent.card != kInvalidId && state_ptr_) {
        if (state_ptr_->objectExists(intent.card)) {
            i["card_def_id"] = state_ptr_->getObject(intent.card).card_def_id;
        }
    }

    // Targets (card_def_ids)
    if (!intent.targets.empty() && state_ptr_) {
        json tgts = json::array();
        for (auto t : intent.targets) {
            if (state_ptr_->objectExists(t))
                tgts.push_back(state_ptr_->getObject(t).card_def_id);
        }
        i["target_def_ids"] = tgts;
    }

    // Units to move
    if (!intent.units_to_move.empty() && state_ptr_) {
        json units = json::array();
        for (auto u : intent.units_to_move) {
            if (state_ptr_->objectExists(u))
                units.push_back(state_ptr_->getObject(u).card_def_id);
        }
        i["unit_def_ids"] = units;
    }

    // Destination
    if (intent.move_destination.has_value()) {
        auto& dest = *intent.move_destination;
        if (std::holds_alternative<BattlefieldLocation>(dest))
            i["dest_bf"] = std::get<BattlefieldLocation>(dest).id;
    }
    if (intent.play_location.has_value()) {
        auto& loc = *intent.play_location;
        if (std::holds_alternative<BattlefieldLocation>(loc))
            i["play_bf"] = std::get<BattlefieldLocation>(loc).id;
    }

    // Ability source
    if (intent.ability_source != kInvalidId && state_ptr_) {
        if (state_ptr_->objectExists(intent.ability_source))
            i["ability_source_def_id"] = state_ptr_->getObject(intent.ability_source).card_def_id;
    }

    // Mulligan
    if (!intent.cards_to_mulligan.empty()) {
        i["mulligan_count"] = intent.cards_to_mulligan.size();
    }

    // Chosen objects (MakeChoice — discard, predict, etc.)
    if (!intent.chosen_objects.empty() && state_ptr_) {
        json chosen = json::array();
        for (auto c : intent.chosen_objects) {
            if (state_ptr_->objectExists(c))
                chosen.push_back(state_ptr_->getObject(c).card_def_id);
        }
        i["chosen_def_ids"] = chosen;
    }

    // Chosen battlefield
    if (intent.chosen_battlefield != kInvalidId) {
        i["chosen_bf"] = intent.chosen_battlefield;
    }

    return i;
}

json DataSerializer::serializeActions(const std::vector<Intent>& actions) const {
    // Full action list with details for ML per-action scoring
    json action_list = json::array();
    for (auto& a : actions) {
        action_list.push_back(serializeIntent(a));
    }
    return action_list;
}

} // namespace riftbound
