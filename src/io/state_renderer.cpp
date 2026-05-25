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

    // Header. Use the disambiguating label so mirror-match BFs with
    // identical card names ("Vilemaw's Lair" × 2) render as
    // "Vilemaw's Lair (P1's)" / "Vilemaw's Lair (P2's)".
    std::string bf_name = battlefieldLabel(state, bf.id);
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
        // Append cost annotation — critical for verifying rune-exhaust math
        // from a static snapshot ("did paying for this 2E card really
        // exhaust 2 runes between the previous decision and this one?").
        // Costs come from CardDB (the static def), not the runtime object.
        std::string cost_tag;
        if (obj.card_def_id != kInvalidId) {
            const auto& def = db_.get(obj.card_def_id);
            if (def.energy_cost > 0) cost_tag = std::to_string(def.energy_cost) + "E";
            if (def.power_cost > 0) {
                if (!cost_tag.empty()) cost_tag += "+";
                cost_tag += std::to_string(def.power_cost) + "P";
            }
        }
        if (obj.isUnit())       label += "(" + std::to_string(obj.base_might) + "M)";
        else if (obj.isSpell()) label += "[S]";
        else if (obj.isGear())  label += "[G]";
        if (!cost_tag.empty())  label += " " + cost_tag;
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

    // Resource summary line. The "Runes" count is split into ready (can
    // be exhausted to pay 1E each) vs exhausted (can be recycled to pay
    // 1P of their domain each). The per-domain ready breakdown lets a
    // reader trace exactly which rune(s) get exhausted to pay a card's
    // cost across consecutive snapshots — without it, "did this 2E play
    // exhaust 2 runes?" is unanswerable from the board alone. The
    // `Pool:E/P` block at the end is the *in-flight* rune pool (drained
    // between plays); shown explicitly with the "Pool:" prefix so it
    // isn't confused with available spend.
    int ready_count = 0;
    int exh_count = 0;
    int ready_by_dom[static_cast<int>(Domain::Count)] = {};
    int exh_by_dom[static_cast<int>(Domain::Count)] = {};
    for (auto rid : state.runesInBase(player)) {
        if (!state.objectExists(rid)) continue;
        const auto& rune = state.getObject(rid);
        if (rune.is_exhausted) ++exh_count; else ++ready_count;
        if (!rune.domains.empty()) {
            int d = static_cast<int>(rune.domains.front());
            if (rune.is_exhausted) ++exh_by_dom[d]; else ++ready_by_dom[d];
        }
    }
    auto domAbbr = [](Domain d) -> char {
        switch (d) {
            case Domain::Fury:  return 'R'; // Fury  = Red
            case Domain::Calm:  return 'G'; // Calm  = Green
            case Domain::Mind:  return 'B'; // Mind  = Blue
            case Domain::Body:  return 'O'; // Body  = Orange
            case Domain::Chaos: return 'P'; // Chaos = Purple
            case Domain::Order: return 'Y'; // Order = Yellow
            default: return '?';
        }
    };
    std::ostringstream dom_breakdown;  // " (rdy: 2O, exh: 1B)"
    bool first_dom = true;
    auto appendDoms = [&](const int (&arr)[static_cast<int>(Domain::Count)],
                           const char* label) {
        bool any = false;
        for (int i = 0; i < static_cast<int>(Domain::Count); ++i) {
            if (arr[i] <= 0) continue;
            if (!any) {
                dom_breakdown << (first_dom ? " (" : ", ") << label << ":";
                first_dom = false;
                any = true;
            } else dom_breakdown << " ";
            dom_breakdown << arr[i] << domAbbr(static_cast<Domain>(i));
        }
    };
    appendDoms(ready_by_dom, "rdy");
    appendDoms(exh_by_dom,   "exh");
    if (!first_dom) dom_breakdown << ")";

    std::ostringstream info;
    info << " " << toString(player) << "  " << legend_name
         << "  Score:" << ps.score
         << "  Deck:" << ps.main_deck.size()
         << "  Runes:" << ready_count << "/" << (ready_count + exh_count)
         << "+" << ps.rune_deck.size()
         << dom_breakdown.str()
         << "  Pool:E" << ps.rune_pool.energy
         << "/P" << ps.rune_pool.totalPower();
    ss << "|" << padRight(info.str(), w - 2) << "|\n";

    // Legend zone — always show the legend, marked as exhausted if it's
    // been used this turn (the legend is a global, persistent game object;
    // showing its state explicitly is useful for V&V even though the name
    // also appears on the player-info line above).
    std::ostringstream legend_line;
    if (ps.legend_zone != kInvalidId && state.objectExists(ps.legend_zone)) {
        auto& legend = state.getObject(ps.legend_zone);
        legend_line << " >> Legend Zone:   " << legend.name
                    << (legend.is_exhausted ? " [EXHAUSTED]" : " [READY]");
    } else {
        legend_line << " >> Legend Zone:   (none)";
    }
    ss << "|" << padRight(legend_line.str(), w - 2) << "|\n";

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
            champ_line << " >> Champion Zone: (on board)";
        }
    } else {
        champ_line << " >> Champion Zone: (on board)";
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

    // Banished — always rendered (per user feedback). Empty case shows
    // "(empty)" so the row presence is consistent across decisions and
    // the user knows where to look when a banish actually lands.
    auto& ps_ban = state.player(player);
    if (ps_ban.banishment.empty()) {
        ss << "|" << padRight(" Banish: (empty)", w - 2) << "|\n";
    } else {
        auto ban_text = renderBanishmentLine(state, player);
        std::istringstream bs(ban_text);
        std::string line;
        bool first = true;
        while (std::getline(bs, line)) {
            if (first) {
                ss << "|" << padRight(" Banish: " + line, w - 2) << "|\n";
                first = false;
            } else {
                ss << "|" << padRight("         " + line, w - 2) << "|\n";
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

    // ── P1 player section (top) ──
    // P1 is rendered on top so the natural "you/active first" reading
    // order matches the trace log (which numbers P1 decisions before
    // P2's in a turn). Swapping from the old bottom-P1 layout.
    ss << renderPlayerSection(state, PlayerId::Player1);
    ss << divider;

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
    ss << thick_divider;

    // ── Battlefields (shared) ──
    for (size_t bi = 0; bi < state.battlefields.size(); ++bi) {
        auto& bf = state.battlefields[bi];

        // BF header — disambiguate mirror-match names.
        std::string bf_name = battlefieldLabel(state, bf.id);
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

    // ── P2 Base ──
    {
        ss << "|" << padRight(" P2 BASE", w - 2) << "|\n";
        auto base_text = renderBase(state, PlayerId::Player2);
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

    // ── P2 player section (bottom) ──
    ss << renderPlayerSection(state, PlayerId::Player2);
    ss << thick_divider;

    // ── Chain (if exists) — rendered as a vertical stack ──
    // Top of stack at the top (resolves first, LIFO). Bottom item at
    // the bottom. The "resuming" slot (an item currently mid-resolution)
    // appears above the chain proper so you can see what's actively
    // being processed.
    if (state.chain.exists() || state.chain.resuming.has_value()) {
        ss << "|" << padRight(" Chain (top resolves next):", w - 2) << "|\n";
        // Resuming first (above the stack — actively being resolved)
        if (state.chain.resuming.has_value()) {
            auto& r = state.chain.resuming.value();
            std::string n = state.objectExists(r.source)
                ? state.getObject(r.source).name : "ability";
            std::ostringstream rl;
            rl << "    >>> RESOLVING: " << n
               << " (" << toString(r.controller) << ")"
               << " step=" << r.resume_point;
            ss << "|" << padRight(rl.str(), w - 2) << "|\n";
        }
        // Top of stack first (back of vector → top), bottom last.
        for (int i = static_cast<int>(state.chain.items.size()) - 1; i >= 0; --i) {
            auto& item = state.chain.items[i];
            std::string source_name = state.objectExists(item.source)
                ? state.getObject(item.source).name : "ability";
            std::ostringstream il;
            // ┌── for top, ├── for middle, └── for bottom
            const char* corner =
                (i == static_cast<int>(state.chain.items.size()) - 1)
                    ? "    [top] " :
                (i == 0) ? "    [bot] " : "    [   ] ";
            il << corner << source_name
               << " (" << toString(item.controller) << ")";
            ss << "|" << padRight(il.str(), w - 2) << "|\n";
        }
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

    // Full names (no truncation) so the HTML replay's card-link
    // wrapper can match them in CARD_IMAGES. The board ASCII still
    // truncates in fixed-width playmat columns; only decision-action
    // descriptions need the full names here.
    auto locationName = [&](const LocationId& loc) -> std::string {
        if (std::holds_alternative<BaseLocation>(loc)) return "Base";
        auto bf_id = std::get<BattlefieldLocation>(loc).id;
        return StateRenderer::battlefieldLabel(state, bf_id);
    };

    // Append "#<def_id>" so duplicate-named cards (Sett, Brawler OGN vs SFD;
    // Karma, Channeler OGN vs SFD; six Seal-of-* gears; etc.) disambiguate
    // in the decision panel and post-decision summary line. Uses the
    // engine's integer card_def_id, not the string registry def_id (which
    // would be too verbose like "ogn-164-298").
    auto nameWithDefId = [&](const GameObject& obj) -> std::string {
        if (obj.card_def_id == kInvalidId) return obj.name;  // tokens
        return obj.name + " #" + std::to_string(obj.card_def_id);
    };
    auto nameOfId = [&](GameObjectId id) -> std::string {
        if (id == kInvalidId || !state.objectExists(id)) return "???";
        return nameWithDefId(state.getObject(id));
    };

    auto cardLabel = [&](GameObjectId id) -> std::string {
        if (id == kInvalidId || !state.objectExists(id)) return "???";
        auto& obj = state.getObject(id);
        std::string name = nameWithDefId(obj);
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
                ss << " @ " << nameWithDefId(state.getObject(a.targets[0]));
            }
            // CR 820 Repeat: chosen_value carries the number of extra
            // tranches the agent is paying for. Surface it in the label
            // so otherwise-identical "Play Hard Bargain" variants are
            // distinguishable.
            if (a.chosen_value.has_value() && *a.chosen_value > 0) {
                ss << " [Repeat ×" << *a.chosen_value << "]";
            }
            break;
        }
        case IntentType::StandardMove: {
            if (!a.units_to_move.empty() && state.objectExists(a.units_to_move[0])) {
                ss << "Move " << nameWithDefId(state.getObject(a.units_to_move[0]));
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
                        // Emit full name (no truncation) so the HTML
                        // replay's card-link wrapper can match it
                        // against CARD_IMAGES. A truncated "Flurry of
                        // .." won't match and won't be clickable.
                        ss << " " << nameWithDefId(state.getObject(cid));
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
        case IntentType::AssignCombatDamage: {
            // Per-unit breakdown so the rendered HTML actually shows the
            // allocation the agent picked — e.g. "Assign Damage: Sett(3),
            // Tank(2)" instead of the opaque "(2 targets)". Lets you spot-
            // check CR-legal cascades without cross-referencing the trace.
            ss << "Assign Damage";
            bool first = true;
            for (const auto& da : a.damage_assignments) {
                if (da.damage <= 0) continue;
                ss << (first ? ": " : ", ");
                if (state.objectExists(da.target_unit)) {
                    ss << nameWithDefId(state.getObject(da.target_unit));
                } else {
                    ss << "id=" << da.target_unit;
                }
                ss << "(" << da.damage << ")";
                first = false;
            }
            if (first) ss << " (empty)";  // shouldn't happen, but safe
            break;
        }
        case IntentType::ActivateAbility:
        case IntentType::ActivateActionAbility:
        case IntentType::ActivateReactionAbility: {
            ss << "Activate ";
            if (a.ability_source != kInvalidId && state.objectExists(a.ability_source)) {
                ss << nameWithDefId(state.getObject(a.ability_source));
            }
            if (!a.targets.empty() && state.objectExists(a.targets[0])) {
                ss << " @ " << nameWithDefId(state.getObject(a.targets[0]));
            }
            break;
        }
        case IntentType::HideCard: {
            ss << "Hide ";
            ss << cardLabel(a.card);
            if (a.chosen_battlefield != kInvalidId) {
                ss << " @ " << StateRenderer::battlefieldLabel(state, a.chosen_battlefield);
            }
            break;
        }
        case IntentType::Concede:
            ss << "Concede";
            break;
        case IntentType::MakeChoice: {
            // MakeChoice is used for many in-resolution decisions: discard
            // a card, recycle vs keep a peeked card (Predict), pick a unit
            // for a card effect, exhaust a specific rune for energy/power
            // payment, etc. The Intent itself doesn't carry the question
            // — only the answer (chosen_objects). Surfacing the chosen
            // card name(s) at least lets the reader see WHAT was picked,
            // even if the WHY needs the surrounding trace lines for
            // context.
            // Int-coded answer (yes/no, mode index, X amount) — set by the
            // resumable card helpers confirmOptional / pickMode /
            // pickXAmount. Show "Choose: =<n>" so the replay distinguishes
            // these from card-pick MakeChoice (which renders below as
            // "Choose: <card name>"). The trace lines MODE_PROMPT /
            // X_PROMPT / MAY_PROMPT carry the human-readable mode labels
            // and prompt text.
            if (a.chosen_value.has_value()) {
                // Prefer the card-supplied human-readable label ("Yes",
                // "Deal 2 damage", "X = 3") when present; the card stamps
                // it onto the option Intent so the engine can render the
                // meaning without knowing the card. Fall back to the bare
                // "=<n>" for legacy/test intents that don't carry one.
                if (!a.choice_label.empty()) {
                    ss << a.choice_label;
                } else {
                    ss << "Choose: =" << *a.chosen_value;
                }
                break;
            }
            if (a.chosen_objects.empty()) {
                ss << "Choose: (skip/none)";
            } else {
                ss << "Choose:";
                for (auto cid : a.chosen_objects) {
                    if (state.objectExists(cid)) {
                        auto& obj = state.getObject(cid);
                        // nameWithDefId adds "#<card_def_id>" (card identity
                        // disambiguation). Then "(obj=N)" carries the
                        // runtime GameObjectId so multiple board copies of
                        // the same card are still distinguishable.
                        ss << " " << nameWithDefId(obj)
                           << " (obj=" << cid << ")";
                        // Zone hint: "@Hand", "@Trash", "@Deck", "@Base",
                        // "@BF", "@Chain", "@Banish". Spelled out so a
                        // tag never collides with single-letter domain
                        // codes used elsewhere in the render ([B]=Body
                        // for Blue, etc.).
                        switch (obj.zone) {
                            case ZoneType::Hand:            ss << "@Hand"; break;
                            case ZoneType::Trash:           ss << "@Trash"; break;
                            case ZoneType::MainDeck:        ss << "@Deck"; break;
                            case ZoneType::Base:            ss << "@Base"; break;
                            case ZoneType::BattlefieldZone: ss << "@BF"; break;
                            case ZoneType::Chain:           ss << "@Chain"; break;
                            case ZoneType::Banishment:      ss << "@Banish"; break;
                            default: break;
                        }
                    } else {
                        ss << " ?";
                    }
                }
            }
            if (a.chosen_battlefield != kInvalidId) {
                ss << " @BF#" << static_cast<int>(a.chosen_battlefield);
            }
            break;
        }
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

    // Skip trivial decisions (only 1 legal action — forced/auto). The
    // replay viewer renders an empty decision panel as
    // "(trivial decision)" — see replay_writer.cpp's HTML fallback. We
    // collapse the panel because rendering "[0] Pass Priority ==> Pass
    // Priority" for every forced chain-priority handoff would bury the
    // interesting decisions in a wall of noise.
    //
    // Whether the trivial decision is also visible in the surrounding
    // trace log depends on which engine path triggered it:
    //   - main queryAgent / mulligan / rune-pay / mid-resolve choice
    //     paths each emit their own "DECISION #N ..." + "CHOSE: ..."
    //     trace lines, so the reader can see what was forced even though
    //     the panel is empty.
    //   - queryAgentForChain (chain priority pass) and the showdown
    //     focus-pass site emit NO decision trace — they only fire
    //     on_decision (which produces the snapshot). For those, a
    //     "(trivial decision)" panel really is the only marker; the
    //     reader has to infer "the only legal action was PassPriority /
    //     PassFocus" from context. If those silent paths become a
    //     visibility problem, the fix is adding `DECISION #N` trace
    //     lines at game_engine.cpp:1030 and game_engine.cpp:1907 (and
    //     for combat damage assignment at 2124, which has ASSIGN_DAMAGE
    //     traces but no DECISION header).
    //
    // Trivial decisions (single legal action) get a compact one-line
    // format instead of being suppressed. User feedback: "the fact that
    // priority was passed but no decision was made is not trivial — it's
    // important for game flow and should be surfaced for V&V." Without
    // this, the replay panel went blank for forced PassPriority /
    // PassFocus / forced rune-pick decisions, leaving readers unable to
    // confirm the engine offered priority to the opponent.
    //
    // Format: "  ==> P2 (forced): Pass Priority"
    // The "(forced)" tag distinguishes "agent chose to pass with options"
    // from "agent had no other choice."
    if (actions.size() <= 1) {
        std::ostringstream ss;
        if (chosen.type == IntentType::EndTurn) {
            ss << "\n  ==> " << toString(chosen.player) << ": End Turn\n";
        } else if (actions.empty()) {
            ss << "\n  --- (no legal actions) ---\n";
        } else {
            ss << "\n  ==> " << toString(chosen.player) << " (forced): "
               << renderChosenAction(state, chosen) << "\n";
        }
        return ss.str();
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

std::string StateRenderer::battlefieldLabel(const GameState& state,
                                              BattlefieldId bf_id) {
    // Look up the target BF's name.
    const BattlefieldState* target = nullptr;
    std::string name;
    for (const auto& bf : state.battlefields) {
        if (bf.id == bf_id) {
            target = &bf;
            if (state.objectExists(bf.card_object_id)) {
                name = state.getObject(bf.card_object_id).name;
            }
            break;
        }
    }
    if (!target) return "BF#" + std::to_string(bf_id);
    if (name.empty()) return "BF#" + std::to_string(bf_id);

    // Disambiguate when another BF on the board shares this name
    // (mirror match where both players brought the same BF). Use
    // `contributed_by` since that's fixed for the life of the game;
    // the live `controller` flips on Score and would mislead.
    int collisions = 0;
    for (const auto& other : state.battlefields) {
        if (other.id == bf_id) continue;
        if (!state.objectExists(other.card_object_id)) continue;
        if (state.getObject(other.card_object_id).name == name) ++collisions;
    }
    if (collisions == 0) return name;

    std::string suffix;
    if (target->contributed_by == PlayerId::Player1) suffix = "P1's";
    else if (target->contributed_by == PlayerId::Player2) suffix = "P2's";
    else suffix = "#" + std::to_string(bf_id);
    return name + " (" + suffix + ")";
}

} // namespace riftbound
