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
    s["player1"] = serializePlayer(state, PlayerId::Player1);
    s["player2"] = serializePlayer(state, PlayerId::Player2);

    json bfs = json::array();
    for (auto& bf : state.battlefields) {
        bfs.push_back(serializeBattlefield(state, bf));
    }
    s["battlefields"] = bfs;

    // Chain
    if (state.chain.exists()) {
        json chain = json::array();
        for (auto& item : state.chain.items) {
            json ci;
            ci["id"] = item.id;
            ci["controller"] = toString(item.controller);
            ci["status"] = item.status == ChainItemStatus::Pending
                           ? "pending" : "finalized";
            if (state.objectExists(item.source)) {
                ci["source"] = state.getObject(item.source).name;
            }
            chain.push_back(ci);
        }
        s["chain"] = chain;
    }

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

    // Hand card IDs (for the training model — private info, but the model
    // controlling this player needs to see it)
    json hand_ids = json::array();
    for (auto id : ps.hand) {
        auto& obj = state.getObject(id);
        hand_ids.push_back(obj.card_def_id);
    }
    p["hand_card_ids"] = hand_ids;

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

    // Channeled runes
    json runes = json::array();
    auto rune_ids = state.runesInBase(player);
    for (auto id : rune_ids) {
        auto& obj = state.getObject(id);
        json r;
        r["card_def_id"] = obj.card_def_id;
        r["exhausted"] = obj.is_exhausted;
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

    b["facedown_count"] = bf.facedown.size();

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
    }

    o["exhausted"] = obj.is_exhausted;
    o["keywords"] = obj.keywords.bits;
    o["buff_count"] = obj.buff_count;

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

    switch (intent.type) {
        case IntentType::PlayCard:
            i["card"] = intent.card;
            break;
        case IntentType::StandardMove:
            i["unit_count"] = intent.units_to_move.size();
            break;
        case IntentType::AssignCombatDamage:
            i["assignment_count"] = intent.damage_assignments.size();
            break;
        case IntentType::MulliganDecision:
            i["mulligan_count"] = intent.cards_to_mulligan.size();
            break;
        default:
            break;
    }

    return i;
}

json DataSerializer::serializeActions(const std::vector<Intent>& actions) const {
    // Compact: just type counts to avoid massive JSON
    std::map<std::string, int> type_counts;
    for (auto& a : actions) {
        type_counts[toString(a.type)]++;
    }

    json summary;
    for (auto& [type, count] : type_counts) {
        summary[type] = count;
    }
    return summary;
}

} // namespace riftbound
