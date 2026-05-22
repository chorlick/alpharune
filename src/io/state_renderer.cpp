#include "state_renderer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace riftbound {

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string StateRenderer::padRight(const std::string& s, int width) const {
    if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
    return s + std::string(width - s.size(), ' ');
}

std::string StateRenderer::centerPad(const std::string& s, int width) const {
    if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
    int pad = width - static_cast<int>(s.size());
    int left = pad / 2;
    int right = pad - left;
    return std::string(left, ' ') + s + std::string(right, ' ');
}

std::string StateRenderer::horizontalLine(int width, char fill) const {
    return std::string(width, fill);
}

std::string StateRenderer::boxLine(const std::string& content, int width) const {
    int inner = width - 4; // "| " + content + " |"
    return "| " + padRight(content, inner) + " |";
}

std::string StateRenderer::keywordAbbrev(const GameObject& obj) const {
    std::string kws;
    auto add = [&](Keyword kw, const char* abbr, int val = 0) {
        if (!obj.keywords.has(kw)) return;
        if (!kws.empty()) kws += ",";
        kws += abbr;
        if (val > 1) kws += std::to_string(val);
    };
    add(Keyword::Assault, "Asl", obj.assault_value);
    add(Keyword::Shield, "Shd", obj.shield_value);
    add(Keyword::Deflect, "Dfl", obj.deflect_value);
    add(Keyword::Ganking, "Gnk");
    add(Keyword::Tank, "Tnk");
    add(Keyword::Accelerate, "Acc");
    add(Keyword::Hidden, "Hdn");
    add(Keyword::Temporary, "Tmp");
    add(Keyword::Deathknell, "Dkn");
    add(Keyword::Action, "Act");
    add(Keyword::Reaction, "Rxn");
    return kws;
}

// ─── Object rendering ────────────────────────────────────────────────────────

std::string StateRenderer::renderObject(const GameState& state,
                                         GameObjectId id) const {
    if (!state.objectExists(id)) return "???";
    auto& obj = state.getObject(id);

    std::ostringstream ss;

    // Shorten name: "LeBlanc, Fragmented" -> "LeBlanc,Frag"
    std::string name = obj.name;
    if (name.size() > 16) {
        auto comma = name.find(',');
        if (comma != std::string::npos && comma < 12) {
            name = name.substr(0, comma) + "," + name.substr(comma + 2, 4);
        } else {
            name = name.substr(0, 14) + "..";
        }
    }
    ss << name;

    ss << "(";
    if (obj.isUnit()) {
        ss << obj.current_might << "M";
        if (obj.damage_marked > 0) ss << "," << obj.damage_marked << "dmg";
    }
    ss << (obj.is_exhausted ? ",exh" : ",rdy");

    if (obj.buff_count > 0) ss << ",+" << obj.buff_count << "buf";

    auto kws = keywordAbbrev(obj);
    if (!kws.empty()) ss << "," << kws;

    if (obj.combat_designation == CombatDesignation::Attacker) ss << ",ATK";
    if (obj.combat_designation == CombatDesignation::Defender) ss << ",DEF";

    // Show attached equipment
    if (!obj.attachments.empty()) {
        ss << ",EQ:";
        for (size_t i = 0; i < obj.attachments.size(); ++i) {
            if (i > 0) ss << "+";
            if (state.objectExists(obj.attachments[i])) {
                auto& gear = state.getObject(obj.attachments[i]);
                std::string gn = gear.name;
                if (gn.size() > 8) gn = gn.substr(0, 6) + "..";
                ss << gn;
            }
        }
    }

    ss << ")";
    return ss.str();
}

// ─── Player summary ──────────────────────────────────────────────────────────

std::string StateRenderer::renderPlayerSummary(const GameState& state,
                                                PlayerId player) const {
    auto& ps = state.player(player);
    auto legend_name = state.objectExists(ps.legend_zone)
        ? state.getObject(ps.legend_zone).name : "???";

    std::ostringstream ss;
    ss << toString(player) << " [Score:" << ps.score
       << "] [Hand:" << ps.hand.size()
       << "] [Deck:" << ps.main_deck.size()
       << "] [Runes:" << state.runesInBase(player).size()
       << "/" << (state.runesInBase(player).size() + ps.rune_deck.size())
       << "] [E:" << ps.rune_pool.energy
       << " P:" << ps.rune_pool.totalPower() << "]";
    ss << "\n  Legend: " << legend_name;

    // Champion Zone — prominent display
    if (ps.champion_zone != kInvalidId && state.objectExists(ps.champion_zone)) {
        auto& champ = state.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone) {
            const auto& def = db_.get(champ.card_def_id);
            ss << "\n  >> Champion Zone: " << champ.name
               << " [" << champ.base_might << "M"
               << " | " << def.energy_cost << "E";
            if (def.power_cost > 0) ss << " " << def.power_cost << "P";
            ss << " | READY TO PLAY]";
        } else {
            ss << "\n  >> Champion: (on board)";
        }
    } else {
        ss << "\n  >> Champion: (on board)";
    }

    return ss.str();
}

// ─── Base rendering ──────────────────────────────────────────────────────────

std::string StateRenderer::renderBase(const GameState& state,
                                       PlayerId player) const {
    std::ostringstream ss;

    // Units in base
    auto base_loc = BaseLocation{player};
    std::vector<std::string> unit_strs, gear_strs, rune_strs;

    for (auto& [id, obj] : state.objects) {
        if (!obj.location.has_value()) continue;
        if (*obj.location != LocationId{base_loc}) continue;

        if (obj.isUnit()) {
            unit_strs.push_back(renderObject(state, id));
        } else if (obj.isGear()) {
            gear_strs.push_back(renderObject(state, id));
        } else if (obj.isRune()) {
            rune_strs.push_back(renderObject(state, id));
        }
    }

    if (!unit_strs.empty()) {
        ss << "  Units: ";
        for (size_t i = 0; i < unit_strs.size(); ++i) {
            if (i > 0) ss << " ";
            ss << unit_strs[i];
        }
        ss << "\n";
    }
    if (!gear_strs.empty()) {
        ss << "  Gear:  ";
        for (size_t i = 0; i < gear_strs.size(); ++i) {
            if (i > 0) ss << " ";
            ss << gear_strs[i];
        }
        ss << "\n";
    }
    if (!rune_strs.empty()) {
        ss << "  Runes: ";
        // Group by name for compactness
        std::map<std::string, int> rune_counts;
        std::map<std::string, bool> rune_exhausted;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value()) continue;
            if (*obj.location != LocationId{base_loc}) continue;
            if (!obj.isRune()) continue;
            std::string key = obj.name + (obj.is_exhausted ? "(exh)" : "(rdy)");
            rune_counts[key]++;
        }
        bool first = true;
        for (auto& [name, count] : rune_counts) {
            if (!first) ss << " ";
            ss << name;
            if (count > 1) ss << "x" << count;
            first = false;
        }
        ss << "\n";
    }

    return ss.str();
}

// ─── Battlefield rendering ───────────────────────────────────────────────────

std::string StateRenderer::renderBattlefield(const GameState& state,
                                              const BattlefieldState& bf) const {
    std::ostringstream ss;

    // Header
    std::string bf_name = state.objectExists(bf.card_object_id)
        ? state.getObject(bf.card_object_id).name : "???";
    std::string ctrl = bf.controller.has_value()
        ? toString(*bf.controller) : "--";

    ss << "  +-- " << bf_name << " [Ctrl:" << ctrl << "]";
    if (bf.is_contested) ss << " CONTESTED";
    if (bf.combat_in_progress) ss << " COMBAT";
    if (bf.showdown_in_progress) ss << " SHOWDOWN";
    if (bf.combat_staged) ss << " (combat staged)";
    if (bf.showdown_staged) ss << " (showdown staged)";
    ss << " --+\n";

    // Units at this battlefield, grouped by player
    auto bf_loc = BattlefieldLocation{bf.id};
    for (auto player : {PlayerId::Player1, PlayerId::Player2}) {
        auto units = state.unitsAt(bf_loc, player);
        if (units.empty()) continue;

        ss << "  |  " << toString(player) << ": ";
        for (size_t i = 0; i < units.size(); ++i) {
            if (i > 0) ss << " ";
            ss << renderObject(state, units[i]);
        }
        ss << "\n";
    }

    // Facedown cards
    if (!bf.facedown.empty()) {
        ss << "  |  [Facedown: " << bf.facedown.size() << "]\n";
    }

    auto p1 = state.unitsAt(bf_loc, PlayerId::Player1);
    auto p2 = state.unitsAt(bf_loc, PlayerId::Player2);
    if (p1.empty() && p2.empty() && bf.facedown.empty()) {
        ss << "  |  (empty)\n";
    }

    ss << "  +--\n";
    return ss.str();
}

// ─── Chain rendering ─────────────────────────────────────────────────────────

std::string StateRenderer::renderChain(const GameState& state) const {
    if (!state.chain.exists()) return "";

    std::ostringstream ss;
    ss << "  Chain:\n";
    for (int i = static_cast<int>(state.chain.items.size()) - 1; i >= 0; --i) {
        auto& item = state.chain.items[i];
        std::string source_name = state.objectExists(item.source)
            ? state.getObject(item.source).name : "ability";
        ss << "    [" << (i + 1) << "] " << source_name
           << " (" << toString(item.controller) << ") "
           << (item.status == ChainItemStatus::Pending ? "Pending" : "Finalized")
           << "\n";
    }
    return ss.str();
}

// ─── Zone rendering helpers ─────────────────────────────────────────────────

/// Render a list of card names in multi-column format, each line bordered.
/// indent_width is how many chars the caller's prefix occupies (e.g., 8 for " Hand:  ").
static std::string renderCardList(const std::vector<std::string>& labels,
                                   int line_width, int indent_width) {
    if (labels.empty()) return "(empty)";

    std::ostringstream ss;
    // Available width per line: total - borders(2) - indent
    int avail = line_width - 2 - indent_width;
    std::string current_line;
    bool first_line = true;

    for (size_t i = 0; i < labels.size(); ++i) {
        std::string entry = labels[i];
        if (!current_line.empty()) entry = "  " + entry; // separator

        if (!current_line.empty() &&
            static_cast<int>(current_line.size() + entry.size()) > avail) {
            // Flush current line
            if (!first_line) ss << "\n";
            ss << current_line;
            first_line = false;
            current_line = labels[i]; // start new line without separator
        } else {
            current_line += entry;
        }
    }
    // Flush last line
    if (!current_line.empty()) {
        if (!first_line) ss << "\n";
        ss << current_line;
    }

    return ss.str();
}

std::string StateRenderer::renderHandLine(const GameState& state,
                                           PlayerId player) const {
    auto& ps = state.player(player);
    std::vector<std::string> labels;
    for (auto card_id : ps.hand) {
        auto& obj = state.getObject(card_id);
        std::string label = obj.name;
        if (obj.isUnit()) label += "(" + std::to_string(obj.base_might) + "M)";
        else if (obj.isSpell()) label += "[S]";
        else if (obj.isGear()) label += "[G]";
        labels.push_back(label);
    }
    return renderCardList(labels, max_width, 8); // " Hand:  " = 8 chars
}

std::string StateRenderer::renderTrashLine(const GameState& state,
                                            PlayerId player) const {
    auto& ps = state.player(player);
    std::vector<std::string> labels;
    for (auto id : ps.trash) {
        if (state.objectExists(id)) {
            labels.push_back(state.getObject(id).name);
        }
    }
    return renderCardList(labels, max_width, 8); // " Trash: " = 8 chars
}

std::string StateRenderer::renderBanishmentLine(const GameState& state,
                                                  PlayerId player) const {
    auto& ps = state.player(player);
    std::vector<std::string> labels;
    for (auto id : ps.banishment) {
        if (state.objectExists(id)) {
            labels.push_back(state.getObject(id).name);
        }
    }
    return renderCardList(labels, max_width, 10); // " Banished: " = 10 chars
}

// ─── Player section ─────────────────────────────────────────────────────────

std::string StateRenderer::renderPlayerSection(const GameState& state,
                                                PlayerId player) const {
    auto& ps = state.player(player);
    int w = max_width;
    std::ostringstream ss;

    // Player info line
    auto legend_name = state.objectExists(ps.legend_zone)
        ? state.getObject(ps.legend_zone).name : "???";

    std::ostringstream info;
    info << " " << toString(player) << "  " << legend_name
         << "  Score:" << ps.score
         << "  Deck:" << ps.main_deck.size()
         << "  Runes:" << state.runesInBase(player).size()
         << "/" << (state.runesInBase(player).size() + ps.rune_deck.size())
         << "  E:" << ps.rune_pool.energy
         << " P:" << ps.rune_pool.totalPower();
    ss << "|" << padRight(info.str(), w - 2) << "|\n";

    // Champion zone
    std::ostringstream champ_line;
    if (ps.champion_zone != kInvalidId && state.objectExists(ps.champion_zone)) {
        auto& champ = state.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone) {
            const auto& def = db_.get(champ.card_def_id);
            champ_line << " >> Champion Zone: " << champ.name
                       << " [" << champ.base_might << "M | "
                       << def.energy_cost << "E";
            if (def.power_cost > 0) champ_line << " " << def.power_cost << "P";
            champ_line << " | READY TO PLAY]";
        } else {
            champ_line << " >> Champion: (on board)";
        }
    } else {
        champ_line << " >> Champion: (on board)";
    }
    ss << "|" << padRight(champ_line.str(), w - 2) << "|\n";

    // Hand — may be multi-line
    auto hand_text = renderHandLine(state, player);
    {
        std::istringstream hs(hand_text);
        std::string line;
        bool first = true;
        while (std::getline(hs, line)) {
            if (first) {
                ss << "|" << padRight(" Hand:  " + line, w - 2) << "|\n";
                first = false;
            } else {
                ss << "|" << padRight("        " + line, w - 2) << "|\n";
            }
        }
    }

    // Trash — may be multi-line
    auto trash_text = renderTrashLine(state, player);
    {
        std::istringstream ts(trash_text);
        std::string line;
        bool first = true;
        while (std::getline(ts, line)) {
            if (first) {
                ss << "|" << padRight(" Trash: " + line, w - 2) << "|\n";
                first = false;
            } else {
                ss << "|" << padRight("        " + line, w - 2) << "|\n";
            }
        }
    }

    // Banished — only show if non-empty
    auto& ps_ban = state.player(player);
    if (!ps_ban.banishment.empty()) {
        auto ban_text = renderBanishmentLine(state, player);
        std::istringstream bs(ban_text);
        std::string line;
        bool first = true;
        while (std::getline(bs, line)) {
            if (first) {
                ss << "|" << padRight(" Banished: " + line, w - 2) << "|\n";
                first = false;
            } else {
                ss << "|" << padRight("           " + line, w - 2) << "|\n";
            }
        }
    }

    return ss.str();
}

// ─── Full board render ───────────────────────────────────────────────────────

std::string StateRenderer::render(const GameState& state) const {
    std::ostringstream ss;
    int w = max_width;
    auto divider = "+" + horizontalLine(w - 2, '-') + "+\n";
    auto thick_divider = "+" + horizontalLine(w - 2, '=') + "+\n";

    // ── Header ──
    ss << thick_divider;
    {
        std::ostringstream hdr;
        hdr << " TURN " << state.turn.turn_number
            << " | " << toString(state.turn.phase)
            << " | " << toString(state.turn.turn_player) << "'s turn";
        if (state.turn.ns_state == NeutralShowdownState::Showdown) hdr << " | SHOWDOWN";
        if (state.turn.oc_state == OpenClosedState::Closed) hdr << " | CLOSED";
        hdr << " | D:" << state.decision_index
            << " (P1:" << state.turn.turn_decisions_p1
            << " P2:" << state.turn.turn_decisions_p2 << ")";
        ss << "|" << padRight(hdr.str(), w - 2) << "|\n";
    }
    ss << thick_divider;

    // ── P2 player section (top — opponent) ──
    ss << renderPlayerSection(state, PlayerId::Player2);
    ss << divider;

    // ── P2 Base ──
    {
        ss << "|" << padRight(" P2 BASE", w - 2) << "|\n";
        auto base_text = renderBase(state, PlayerId::Player2);
        if (base_text.empty()) {
            ss << "|" << padRight("   (empty)", w - 2) << "|\n";
        } else {
            // Wrap each line of base_text in borders
            std::istringstream bs(base_text);
            std::string line;
            while (std::getline(bs, line)) {
                if (!line.empty()) {
                    ss << "|" << padRight(" " + line, w - 2) << "|\n";
                }
            }
        }
    }
    ss << thick_divider;

    // ── Battlefields (shared) ──
    for (size_t bi = 0; bi < state.battlefields.size(); ++bi) {
        auto& bf = state.battlefields[bi];

        // BF header
        std::string bf_name = state.objectExists(bf.card_object_id)
            ? state.getObject(bf.card_object_id).name : "???";
        std::string ctrl = bf.controller.has_value()
            ? toString(*bf.controller) : "--";

        std::ostringstream bf_hdr;
        bf_hdr << " " << bf_name << "  [Ctrl:" << ctrl << "]";
        if (bf.is_contested) bf_hdr << "  CONTESTED";
        if (bf.combat_in_progress) bf_hdr << "  COMBAT";
        if (bf.showdown_in_progress) bf_hdr << "  SHOWDOWN";
        if (bf.combat_staged) bf_hdr << "  (combat staged)";
        if (bf.showdown_staged) bf_hdr << "  (showdown staged)";
        ss << "|" << padRight(bf_hdr.str(), w - 2) << "|\n";

        // Units at this battlefield
        auto bf_loc = BattlefieldLocation{bf.id};
        for (auto player : {PlayerId::Player1, PlayerId::Player2}) {
            auto units = state.unitsAt(bf_loc, player);
            std::ostringstream ul;
            ul << "   " << toString(player) << ": ";
            if (units.empty()) {
                ul << "--";
            } else {
                for (size_t i = 0; i < units.size(); ++i) {
                    if (i > 0) ul << "  ";
                    ul << renderObject(state, units[i]);
                }
            }
            ss << "|" << padRight(ul.str(), w - 2) << "|\n";
        }

        // Facedown cards
        if (!bf.facedown.empty()) {
            for (auto card_id : bf.facedown) {
                if (!state.objectExists(card_id)) continue;
                auto& card = state.getObject(card_id);
                std::ostringstream fd;
                fd << "   [HIDDEN: " << toString(card.controller) << " \xe2\x80\x94 facedown";
                if (show_hand) fd << ": " << card.name; // reveal in debug mode
                fd << "]";
                ss << "|" << padRight(fd.str(), w - 2) << "|\n";
            }
        }

        if (bi + 1 < state.battlefields.size()) {
            ss << divider;
        }
    }
    ss << thick_divider;

    // ── P1 Base ──
    {
        ss << "|" << padRight(" P1 BASE", w - 2) << "|\n";
        auto base_text = renderBase(state, PlayerId::Player1);
        if (base_text.empty()) {
            ss << "|" << padRight("   (empty)", w - 2) << "|\n";
        } else {
            std::istringstream bs(base_text);
            std::string line;
            while (std::getline(bs, line)) {
                if (!line.empty()) {
                    ss << "|" << padRight(" " + line, w - 2) << "|\n";
                }
            }
        }
    }
    ss << divider;

    // ── P1 player section (bottom — local player) ──
    ss << renderPlayerSection(state, PlayerId::Player1);
    ss << thick_divider;

    // ── Chain (if exists) ──
    if (state.chain.exists()) {
        std::ostringstream ch;
        ch << " Chain:";
        for (int i = static_cast<int>(state.chain.items.size()) - 1; i >= 0; --i) {
            auto& item = state.chain.items[i];
            std::string source_name = state.objectExists(item.source)
                ? state.getObject(item.source).name : "ability";
            ch << "  [" << (i + 1) << "] " << source_name
               << " (" << toString(item.controller) << ")";
        }
        ss << "|" << padRight(ch.str(), w - 2) << "|\n";
        ss << thick_divider;
    }

    // ── Game over ──
    if (state.game_over) {
        std::ostringstream go;
        go << " >>> GAME OVER: " << state.game_over_reason << " <<<";
        ss << "|" << padRight(go.str(), w - 2) << "|\n";
        ss << thick_divider;
    }

    return ss.str();
}

// ─── Action rendering ────────────────────────────────────────────────────────

/// Format a single action as a short label with card name and destination.
static std::string formatActionShort(const GameState& state,
                                      const Intent& a) {
    std::ostringstream ss;

    auto locationName = [&](const LocationId& loc) -> std::string {
        if (std::holds_alternative<BaseLocation>(loc)) return "Base";
        auto bf_id = std::get<BattlefieldLocation>(loc).id;
        for (auto& bf : state.battlefields) {
            if (bf.id == bf_id && state.objectExists(bf.card_object_id)) {
                auto& name = state.getObject(bf.card_object_id).name;
                return name.size() > 16 ? name.substr(0, 14) + ".." : name;
            }
        }
        return "BF#" + std::to_string(bf_id);
    };

    auto cardLabel = [&](GameObjectId id) -> std::string {
        if (id == kInvalidId || !state.objectExists(id)) return "???";
        auto& obj = state.getObject(id);
        std::string name = obj.name;
        if (name.size() > 18) name = name.substr(0, 16) + "..";
        if (obj.isUnit()) name += "(" + std::to_string(obj.base_might) + "M)";
        if (obj.isSpell()) name += "[S]";
        if (obj.isGear()) name += "[G]";
        return name;
    };

    switch (a.type) {
        case IntentType::PlayCard:
        case IntentType::PlayActionCard:
        case IntentType::PlayReaction: {
            ss << "Play " << cardLabel(a.card);
            if (a.play_location.has_value()) {
                ss << " -> " << locationName(*a.play_location);
            }
            if (!a.targets.empty() && state.objectExists(a.targets[0])) {
                ss << " @ " << state.getObject(a.targets[0]).name;
            }
            break;
        }
        case IntentType::StandardMove: {
            if (!a.units_to_move.empty() && state.objectExists(a.units_to_move[0])) {
                ss << "Move " << state.getObject(a.units_to_move[0]).name;
            } else {
                ss << "Move";
            }
            if (a.move_destination.has_value()) {
                ss << " -> " << locationName(*a.move_destination);
            }
            break;
        }
        case IntentType::MulliganDecision: {
            if (a.cards_to_mulligan.empty()) {
                ss << "Keep hand";
            } else {
                ss << "Mull:";
                for (auto cid : a.cards_to_mulligan) {
                    if (state.objectExists(cid)) {
                        auto& name = state.getObject(cid).name;
                        ss << " " << (name.size() > 12 ? name.substr(0, 10) + ".." : name);
                    }
                }
            }
            break;
        }
        case IntentType::EndTurn:
            ss << "End Turn";
            break;
        case IntentType::PassFocus:
            ss << "Pass Focus";
            break;
        case IntentType::PassPriority:
            ss << "Pass Priority";
            break;
        case IntentType::AssignCombatDamage:
            ss << "Assign Damage (" << a.damage_assignments.size() << " targets)";
            break;
        case IntentType::ActivateAbility:
        case IntentType::ActivateActionAbility:
        case IntentType::ActivateReactionAbility: {
            ss << "Activate ";
            if (a.ability_source != kInvalidId && state.objectExists(a.ability_source)) {
                auto& src = state.getObject(a.ability_source);
                std::string name = src.name;
                if (name.size() > 16) name = name.substr(0, 14) + "..";
                ss << name;
            }
            if (!a.targets.empty() && state.objectExists(a.targets[0])) {
                ss << " @ " << state.getObject(a.targets[0]).name;
            }
            break;
        }
        case IntentType::HideCard: {
            ss << "Hide ";
            ss << cardLabel(a.card);
            if (a.chosen_battlefield != kInvalidId) {
                for (auto& bf : state.battlefields) {
                    if (bf.id == a.chosen_battlefield && state.objectExists(bf.card_object_id)) {
                        ss << " @ " << state.getObject(bf.card_object_id).name;
                    }
                }
            }
            break;
        }
        case IntentType::Concede:
            ss << "Concede";
            break;
        default:
            ss << toString(a.type);
            break;
    }
    return ss.str();
}

std::string StateRenderer::renderActions(const GameState& state,
                                          const std::vector<Intent>& actions) const {
    std::ostringstream ss;

    // Build short labels for each action
    std::vector<std::string> labels;
    for (auto& a : actions) {
        labels.push_back(formatActionShort(state, a));
    }

    // Find max label width for column alignment
    int col_width = 0;
    for (auto& l : labels) {
        col_width = std::max(col_width, static_cast<int>(l.size()));
    }
    col_width = std::min(col_width + 4, 38); // cap + padding

    int cols = std::max(1, (max_width - 4) / col_width);

    ss << "  " << actions.size() << " options:\n";
    for (size_t i = 0; i < labels.size(); ++i) {
        if (i % cols == 0) ss << "    ";
        ss << padRight("[" + std::to_string(i) + "] " + labels[i], col_width);
        if ((i + 1) % cols == 0 || i + 1 == labels.size()) ss << "\n";
    }

    return ss.str();
}

std::string StateRenderer::renderChosenAction(
    const GameState& state, const Intent& intent) const {
    return formatActionShort(state, intent);
}

std::string StateRenderer::renderDecision(
    const GameState& state,
    const std::vector<Intent>& actions,
    const Intent& chosen) const {

    // Skip trivial decisions (only 1 legal action — forced/auto)
    // But always show EndTurn so the turn flow is visible
    if (actions.size() <= 1) {
        if (chosen.type == IntentType::EndTurn) {
            std::ostringstream ss;
            ss << "\n  ==> " << toString(chosen.player) << ": End Turn\n";
            return ss.str();
        }
        return "";
    }

    std::ostringstream ss;

    // Header
    ss << "\n  --- " << toString(chosen.player) << " Decision #"
       << state.decision_index << " [" << toString(state.turn.phase) << "]";
    if (state.turn.oc_state == OpenClosedState::Closed) ss << " CLOSED";
    if (state.turn.ns_state == NeutralShowdownState::Showdown) ss << " SHOWDOWN";
    ss << " ---\n";

    // Legal actions in columns
    ss << renderActions(state, actions);

    // Chosen action — prominent
    ss << "  ==> " << toString(chosen.player) << ": "
       << renderChosenAction(state, chosen) << "\n";

    return ss.str();
}

} // namespace riftbound
