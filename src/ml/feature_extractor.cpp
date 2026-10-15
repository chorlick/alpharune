#include "ml/feature_extractor.h"

#include "engine/knowledge_tracker.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace riftbound::ml {

namespace {

// Cards have card_def_id in [1, kCardVocabSize]. Tokens have kInvalidId
// (== UINT32_MAX == 4,294,967,295), which is catastrophic if fed to a Linear
// layer as a raw float — it produces astronomical activations and blows up
// the loss during training. This helper returns 0.0f for invalid/token IDs.
inline float safeCardId(CardDefId id) {
    if (id == kInvalidId) return 0.0f;
    const uint32_t n = id.raw();
    if (n < 1 || n > kCardVocabSize) return 0.0f;
    return static_cast<float>(n);
}

// BattlefieldIds are typically 0..3 but the type is uint32_t. If a bug ever
// sets one to kInvalidId (UINT32_MAX), pushing it as a raw float would poison
// the action features. Clamp to a sane bound — anything above 15 is almost
// certainly a corruption signal.
inline float safeBfId(BattlefieldId id) {
    if (id == kInvalidId) return -1.0f;
    if (id.raw() > 15) return -1.0f;
    return static_cast<float>(id.raw());
}

// Every block starts where `layout` says it does: a block that grows or moves
// without its offset is a layout bug, caught on the first extraction.
inline void blockStartsAt(const std::vector<float>& features, int offset,
                          const char* block) {
    if (static_cast<int>(features.size()) != offset) [[unlikely]] {
        throw std::logic_error(
            std::string("extractStateFeatures: block ") + block + " starts at " +
            std::to_string(features.size()) + ", layout says " +
            std::to_string(offset));
    }
}

// A card id the perspective player may see: 0 for an object whose identity
// it does not know (batch 5b item T2).
inline float visibleCardId(const GameState& state, GameObjectId id, PlayerId perspective) {
    if (!state.objectExists(id) || !knowledge::identityVisible(state, id, perspective))
        return 0.0f;
    return safeCardId(state.getObject(id).card_def_id);
}

// The one order of units (and base gear) everywhere in the vector: Might
// descending, then every other field the vector writes about a unit, so that
// two units that tie on Might come out in the same order whatever their ids
// and whatever order the object table holds them in (batch 5b item T2), and
// one (battlefield, side, position) slot is the same unit in every block.
struct UnitOrderKey {
    std::array<int64_t, 18> k{};
    explicit UnitOrderKey(const GameState& state, const GameObject& u) {
        CardDefId gear;
        for (auto aid : u.attachments) {
            if (state.objectExists(aid)) { gear = state.getObject(aid).card_def_id; break; }
        }
        k = {-static_cast<int64_t>(u.current_might),
             static_cast<int64_t>(safeCardId(u.card_def_id)),
             u.damage_marked,
             static_cast<int64_t>(safeCardId(gear)),
             u.current_might - u.base_might,
             u.temp_might_bonus,
             static_cast<int64_t>(u.combat_designation),
             u.assault_value, u.shield_value, u.deflect_value,
             u.buff_count, u.temp_buff_count,
             u.hasKeyword(Keyword::Tank), u.hasKeyword(Keyword::Backline),
             u.hasKeyword(Keyword::Ganking),
             u.is_exhausted, u.is_stunned,
             static_cast<int64_t>((u.keywords | u.aura_keywords).bits)};
    }
};

void sortUnits(const GameState& state, std::vector<GameObjectId>& ids) {
    if (ids.size() < 2) return;
    std::vector<std::pair<UnitOrderKey, GameObjectId>> keyed;
    keyed.reserve(ids.size());
    for (auto id : ids) keyed.emplace_back(UnitOrderKey(state, state.getObject(id)), id);
    std::stable_sort(keyed.begin(), keyed.end(),
                     [](const auto& a, const auto& b) { return a.first.k < b.first.k; });
    for (size_t i = 0; i < ids.size(); ++i) ids[i] = keyed[i].second;
}

// Where the extractor's board queries land, gathered in ONE pass over
// `state.objects`. The extractor used to ask the state ~24 separate
// questions (`unitsAt` per battlefield per player, twice; runes, base units,
// base gear and ready base units per player), each a full scan of the object
// map returning a fresh vector. Every list keeps the map's iteration order,
// which is the order those queries returned; each list is then put in the
// one unit order (sortUnits).
struct BoardIndex {
    static constexpr int kBfSlots = 4;
    std::vector<GameObjectId> bf_units[kBfSlots][2];  // [battlefield slot][seat]
    std::vector<GameObjectId> runes_in_base[2];       // by whose base
    std::vector<GameObjectId> base_units[2];          // by controller, any base
    std::vector<GameObjectId> base_gear[2];           // by controller, any base

    static int seat(PlayerId p) { return p == PlayerId::Player2 ? 1 : 0; }

    void build(const GameState& state) {
        for (auto& slot : bf_units) for (auto& v : slot) v.clear();
        for (int s = 0; s < 2; ++s) {
            runes_in_base[s].clear();
            base_units[s].clear();
            base_gear[s].clear();
        }
        const int bf_count = std::min<int>(
            kBfSlots, static_cast<int>(state.battlefields.size()));
        for (auto& [oid, obj] : state.objects) {
            if (!obj.location.has_value()) continue;
            const bool seated = obj.controller == PlayerId::Player1 ||
                                obj.controller == PlayerId::Player2;
            if (const auto* base = std::get_if<BaseLocation>(&*obj.location)) {
                if (obj.isRune()) {
                    if (base->player == PlayerId::Player1 ||
                        base->player == PlayerId::Player2) {
                        runes_in_base[seat(base->player)].push_back(oid);
                    }
                } else if (obj.isUnit()) {
                    if (seated) base_units[seat(obj.controller)].push_back(oid);
                } else if (obj.isGear()) {
                    if (seated) base_gear[seat(obj.controller)].push_back(oid);
                }
            } else if (obj.isUnit() && seated) {
                const BattlefieldId at =
                    std::get<BattlefieldLocation>(*obj.location).id;
                for (int i = 0; i < bf_count; ++i) {
                    if (state.battlefields[i].id == at) {
                        bf_units[i][seat(obj.controller)].push_back(oid);
                    }
                }
            }
        }
        // The walk above is in the table's order, a function of object ids:
        // put every list in the one unit order (runes are only counted).
        for (auto& slot : bf_units) for (auto& v : slot) sortUnits(state, v);
        for (int s = 0; s < 2; ++s) {
            sortUnits(state, base_units[s]);
            sortUnits(state, base_gear[s]);
        }
    }
};

// 20 features per side (written to `out`) plus 12 extended per-unit features
// (3 units × 4 fields) written to `ext_out` if non-null. Top-3 ordering is by
// current_might descending — same for both buffers so the trainer can line
// them up by position.
void extractSideFeatures(const GameState& state,
                         const std::vector<GameObjectId>& units,
                         std::vector<float>& out,
                         float* ext_out /* [12] or nullptr */) {
    int count = 0, total_might = 0, total_damage = 0;
    int num_exhausted = 0, num_stunned = 0;
    int num_tank = 0, num_backline = 0, num_ganking = 0;
    int num_assault = 0, num_shield = 0, num_deflect = 0;

    struct UnitInfo {
        CardDefId def_id;
        int might;
        int damage;
        CardDefId attachment_def_id;
        int might_delta;
        int temp_might_bonus;
        int combat_designation;
    };
    std::vector<UnitInfo> unit_infos;

    for (auto uid : units) {
        auto& u = state.getObject(uid);
        count++;
        total_might += u.current_might;
        total_damage += u.damage_marked;
        if (u.is_exhausted) num_exhausted++;
        if (u.is_stunned) num_stunned++;
        if (u.hasKeyword(Keyword::Tank)) num_tank++;
        if (u.hasKeyword(Keyword::Backline)) num_backline++;
        if (u.hasKeyword(Keyword::Ganking)) num_ganking++;
        if (u.hasKeyword(Keyword::Assault)) num_assault++;
        if (u.hasKeyword(Keyword::Shield)) num_shield++;
        if (u.hasKeyword(Keyword::Deflect)) num_deflect++;

        CardDefId gear_def;  // none: safeCardId gives 0
        for (auto aid : u.attachments) {
            if (state.objectExists(aid)) {
                gear_def = state.getObject(aid).card_def_id;
                break;
            }
        }
        int cd = 0;
        if (u.combat_designation == CombatDesignation::Attacker) cd = 1;
        else if (u.combat_designation == CombatDesignation::Defender) cd = 2;

        unit_infos.push_back({
            u.card_def_id, u.current_might, u.damage_marked,
            gear_def, u.current_might - u.base_might, u.temp_might_bonus, cd
        });
    }

    out.push_back(static_cast<float>(count));
    out.push_back(static_cast<float>(total_might));
    out.push_back(static_cast<float>(total_damage));
    out.push_back(static_cast<float>(num_exhausted));
    out.push_back(static_cast<float>(num_stunned));
    out.push_back(static_cast<float>(num_tank));
    out.push_back(static_cast<float>(num_backline));
    out.push_back(static_cast<float>(num_ganking));
    out.push_back(static_cast<float>(num_assault));
    out.push_back(static_cast<float>(num_shield));
    out.push_back(static_cast<float>(num_deflect));

    // `units` is in the one unit order (Might first): stable, so ties keep it.
    std::stable_sort(unit_infos.begin(), unit_infos.end(),
                     [](auto& a, auto& b) { return a.might > b.might; });

    for (int i = 0; i < kMaxBfUnitsPerSide; ++i)
        out.push_back(i < static_cast<int>(unit_infos.size())
                       ? safeCardId(unit_infos[i].def_id) : 0.0f);
    for (int i = 0; i < kMaxBfUnitsPerSide; ++i)
        out.push_back(i < static_cast<int>(unit_infos.size())
                       ? static_cast<float>(unit_infos[i].might) : 0.0f);
    for (int i = 0; i < kMaxBfUnitsPerSide; ++i)
        out.push_back(i < static_cast<int>(unit_infos.size())
                       ? static_cast<float>(unit_infos[i].damage) : 0.0f);

    if (ext_out) {
        for (int i = 0; i < kMaxBfUnitsPerSide; ++i) {
            int base = i * kPerUnitExtFields;
            if (i < static_cast<int>(unit_infos.size())) {
                ext_out[base + 0] = static_cast<float>(unit_infos[i].combat_designation);
                ext_out[base + 1] = safeCardId(unit_infos[i].attachment_def_id);
                ext_out[base + 2] = static_cast<float>(unit_infos[i].might_delta);
                ext_out[base + 3] = static_cast<float>(unit_infos[i].temp_might_bonus);
            } else {
                ext_out[base + 0] = 0.0f;
                ext_out[base + 1] = 0.0f;
                ext_out[base + 2] = 0.0f;
                ext_out[base + 3] = 0.0f;
            }
        }
    }
}

// 70 features per player
void extractPlayerFeatures(const GameState& state, PlayerId player,
                           bool is_self, PlayerId /*perspective*/,
                           const CardDB& card_db,
                           const BoardIndex& board,
                           std::vector<float>& out) {
    auto& ps = state.player(player);
    const int seat = BoardIndex::seat(player);

    // Resources (8)
    out.push_back(static_cast<float>(ps.score));
    out.push_back(static_cast<float>(ps.xp));
    out.push_back(static_cast<float>(ps.hand.size()));
    out.push_back(static_cast<float>(ps.main_deck.size()));
    out.push_back(static_cast<float>(ps.rune_deck.size()));
    out.push_back(static_cast<float>(ps.rune_pool.energy));
    out.push_back(ps.burned_out ? 1.0f : 0.0f);
    int cards_played = ps.cards_played_this_turn;
    out.push_back(static_cast<float>(cards_played));

    // Champion and legend (4)
    out.push_back(ps.champion_zone != kInvalidId && state.objectExists(ps.champion_zone)
        ? safeCardId(state.getObject(ps.champion_zone).card_def_id) : 0.0f);
    out.push_back(ps.champion_zone != kInvalidId &&
        state.objectExists(ps.champion_zone) &&
        state.getObject(ps.champion_zone).zone == ZoneType::ChampionZone ? 1.0f : 0.0f);
    out.push_back(ps.legend_zone != kInvalidId && state.objectExists(ps.legend_zone)
        ? safeCardId(state.getObject(ps.legend_zone).card_def_id) : 0.0f);
    out.push_back(ps.legend_zone != kInvalidId &&
        state.objectExists(ps.legend_zone) &&
        state.getObject(ps.legend_zone).is_exhausted ? 1.0f : 0.0f);

    // Power breakdown by domain (7)
    for (int i = 0; i < static_cast<int>(Domain::Count); ++i)
        out.push_back(static_cast<float>(ps.rune_pool.power[i]));
    out.push_back(static_cast<float>(ps.rune_pool.universal_power));

    // Hand card IDs (10) — self only, opponent's hidden. Sorted by card
    // (knowledge::sortedHand; the mulligan slots name these positions), not
    // in the order the cards were dealt (batch 5b item T2).
    const std::vector<GameObjectId> hand =
        is_self ? knowledge::sortedHand(state, player) : std::vector<GameObjectId>{};
    for (int i = 0; i < 10; ++i) {
        if (i < static_cast<int>(hand.size())) {
            out.push_back(safeCardId(state.getObject(hand[i]).card_def_id));
        } else {
            out.push_back(0.0f);
        }
    }

    // Hand card costs (10) — self only
    for (int i = 0; i < 10; ++i) {
        if (i < static_cast<int>(hand.size())) {
            auto& obj = state.getObject(hand[i]);
            if (obj.card_def_id != kInvalidId) {
                out.push_back(static_cast<float>(card_db.get(obj.card_def_id).energy_cost));
            } else {
                out.push_back(0.0f);
            }
        } else {
            out.push_back(0.0f);
        }
    }

    // Runes (2) — ready vs exhausted count
    const auto& runes = board.runes_in_base[seat];
    int ready = 0, exhausted = 0;
    for (auto rid : runes) {
        if (state.getObject(rid).is_exhausted) exhausted++;
        else ready++;
    }
    out.push_back(static_cast<float>(ready));
    out.push_back(static_cast<float>(exhausted));

    // Rune domain breakdown: ready (6)
    for (int d = 0; d < static_cast<int>(Domain::Count); ++d) {
        int count = 0;
        for (auto rid : runes) {
            auto& obj = state.getObject(rid);
            if (!obj.is_exhausted && obj.card_def_id != kInvalidId) {
                auto& def = card_db.get(obj.card_def_id);
                for (auto dom : def.domains) {
                    if (static_cast<int>(dom) == d) { count++; break; }
                }
            }
        }
        out.push_back(static_cast<float>(count));
    }

    // Rune domain breakdown: exhausted (6)
    for (int d = 0; d < static_cast<int>(Domain::Count); ++d) {
        int count = 0;
        for (auto rid : runes) {
            auto& obj = state.getObject(rid);
            if (obj.is_exhausted && obj.card_def_id != kInvalidId) {
                auto& def = card_db.get(obj.card_def_id);
                for (auto dom : def.domains) {
                    if (static_cast<int>(dom) == d) { count++; break; }
                }
            }
        }
        out.push_back(static_cast<float>(count));
    }

    // Base units: count, total might, total damage, num exhausted (4)
    struct BaseUnitInfo { CardDefId def_id; int might; };
    std::vector<BaseUnitInfo> base_unit_infos;
    base_unit_infos.reserve(board.base_units[seat].size());
    int base_might = 0, base_damage = 0, base_exhausted = 0;
    for (auto uid : board.base_units[seat]) {
        auto& obj = state.getObject(uid);
        base_unit_infos.push_back({obj.card_def_id, obj.current_might});
        base_might += obj.current_might;
        base_damage += obj.damage_marked;
        if (obj.is_exhausted) base_exhausted++;
    }
    out.push_back(static_cast<float>(base_unit_infos.size()));
    out.push_back(static_cast<float>(base_might));
    out.push_back(static_cast<float>(base_damage));
    out.push_back(static_cast<float>(base_exhausted));

    // Base unit IDs top 3 sorted by might desc (3)
    std::stable_sort(base_unit_infos.begin(), base_unit_infos.end(),
                     [](auto& a, auto& b) { return a.might > b.might; });
    for (int i = 0; i < 3; ++i)
        out.push_back(i < static_cast<int>(base_unit_infos.size())
                       ? safeCardId(base_unit_infos[i].def_id) : 0.0f);

    // Base gear count (1) + top 2 gear IDs (2)
    const auto& gear_ids = board.base_gear[seat];
    out.push_back(static_cast<float>(gear_ids.size()));
    for (int i = 0; i < 2; ++i)
        out.push_back(i < static_cast<int>(gear_ids.size())
                       ? safeCardId(state.getObject(gear_ids[i]).card_def_id)
                       : 0.0f);

    // Trash and banishment sizes (2)
    out.push_back(static_cast<float>(ps.trash.size()));
    out.push_back(static_cast<float>(ps.banishment.size()));

    // has_discarded_this_turn (1)
    out.push_back(ps.has_discarded_this_turn ? 1.0f : 0.0f);

    // bfs_scored_this_turn (1)
    out.push_back(static_cast<float>(ps.battlefields_scored_this_turn.size()));

    // num_bfs_controlled (1)
    int num_controlled = 0;
    for (auto& bf : state.battlefields) {
        if (bf.controller.has_value() && *bf.controller == player)
            num_controlled++;
    }
    out.push_back(static_cast<float>(num_controlled));

    // legion_active (1) — cards_played_this_turn >= 2
    out.push_back(cards_played >= 2 ? 1.0f : 0.0f);

    // cost_modifier_count (1)
    out.push_back(static_cast<float>(ps.cost_modifiers.size()));
}

} // anonymous namespace

std::vector<float> extractStateFeatures(const GameState& state,
                                        PlayerId perspective,
                                        const CardDB& card_db) {
    std::vector<float> features;
    extractStateFeatures(state, perspective, card_db, features);
    return features;
}

void extractStateFeatures(const GameState& state,
                          PlayerId perspective,
                          const CardDB& card_db,
                          std::vector<float>& features) {
    features.clear();
    features.reserve(kStateFeatureDim);

    PlayerId opp = opponent(perspective);

    // One scan of the object map for everything below (see BoardIndex). The
    // index is per thread so its lists keep their capacity between calls.
    thread_local BoardIndex board;
    board.build(state);
    const int self_seat = BoardIndex::seat(perspective);
    const int opp_seat  = BoardIndex::seat(opp);

    // Global features (10)
    blockStartsAt(features, layout::kGlobals, "kGlobals");
    features.push_back(static_cast<float>(state.turn.turn_number));
    int phase_val = 0;
    switch (state.turn.phase) {
        case TurnPhase::Mulligan: phase_val = 0; break;
        case TurnPhase::AwakenPhase: phase_val = 1; break;
        case TurnPhase::BeginningStep: phase_val = 2; break;
        case TurnPhase::ScoringStep: phase_val = 3; break;
        case TurnPhase::ChannelPhase: phase_val = 4; break;
        case TurnPhase::DrawPhase: phase_val = 5; break;
        case TurnPhase::MainPhase: phase_val = 6; break;
        case TurnPhase::EndingStep: phase_val = 7; break;
        case TurnPhase::ExpirationStep: phase_val = 8; break;
        default: break;
    }
    features.push_back(static_cast<float>(phase_val));
    features.push_back(state.turn.turn_player == perspective ? 1.0f : 0.0f);
    features.push_back(state.turn.starting_player == perspective ? 1.0f : 0.0f);
    features.push_back(static_cast<float>(
        state.player(perspective).score - state.player(opp).score));
    features.push_back(static_cast<float>(state.chain.items.size()));
    features.push_back(state.turn.ns_state == NeutralShowdownState::Showdown ? 1.0f : 0.0f);
    features.push_back(state.turn.oc_state == OpenClosedState::Closed ? 1.0f : 0.0f);
    features.push_back(state.turn.is_additional_turn ? 1.0f : 0.0f);
    features.push_back(static_cast<float>(state.delayed_abilities.size()));

    // Chain items (kMaxChainItems x 2 = 8)
    blockStartsAt(features, layout::kChainItems, "kChainItems");
    for (int i = 0; i < kMaxChainItems; ++i) {
        if (i < static_cast<int>(state.chain.items.size())) {
            auto& item = state.chain.items[i];
            features.push_back(visibleCardId(state, item.source, perspective));
            features.push_back(item.controller == perspective ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
            features.push_back(0.0f);
        }
    }

    // Self player features (70)
    blockStartsAt(features, layout::kSelfPlayer, "kSelfPlayer");
    extractPlayerFeatures(state, perspective, true, perspective, card_db,
                          board, features);

    // Opponent player features (70)
    blockStartsAt(features, layout::kOppPlayer, "kOppPlayer");
    extractPlayerFeatures(state, opp, false, perspective, card_db,
                          board, features);

    // Battlefield features (49 each × 4 = 196), slot by slot. Also collects per-side
    // extended features (12 floats per side) into ext_per_side for later
    // appending — kept here so the top-3 sort order matches extractSideFeatures.
    blockStartsAt(features, layout::kBattlefields, "kBattlefields");
    constexpr int kExtPerSide = kMaxBfUnitsPerSide * kPerUnitExtFields; // 12
    float ext_per_side[8 /* 4 BFs × 2 sides */][kExtPerSide] = {};
    int ext_slot = 0;
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            auto& bf = state.battlefields[i];

            features.push_back(state.objectExists(bf.card_object_id)
                ? safeCardId(state.getObject(bf.card_object_id).card_def_id) : 0.0f);

            // 0 nobody, 1 the perspective player, 2 the opponent (it was
            // 1 = Player1, 2 = Player2 before the 2026-10 layout).
            float ctrl = 0;
            if (bf.controller.has_value())
                ctrl = (*bf.controller == perspective) ? 1.0f : 2.0f;
            features.push_back(ctrl);

            features.push_back(bf.is_contested ? 1.0f : 0.0f);
            bool in_combat = bf.combat_in_progress;
            features.push_back(in_combat ? 1.0f : 0.0f);
            features.push_back(bf.showdown_in_progress ? 1.0f : 0.0f);

            features.push_back(static_cast<float>(bf.facedown.size()));

            if (in_combat) {
                if (bf.attacker.has_value() && *bf.attacker == perspective)
                    features.push_back(1.0f);
                else
                    features.push_back(0.0f);
            } else {
                features.push_back(-1.0f);
            }

            int cp = 0;
            switch (bf.combat_phase) {
                case CombatPhase::ShowdownStep: cp = 1; break;
                case CombatPhase::DamageStep: cp = 2; break;
                case CombatPhase::ResolutionStep: cp = 3; break;
                default: break;
            }
            features.push_back(static_cast<float>(cp));

            features.push_back(bf.is_token ? 1.0f : 0.0f);

            // Per-side features (20 each × 2 = 40) + extended per-side (12
            // each): the perspective player's side, then the opponent's — the
            // order of every per-side block of the vector.
            extractSideFeatures(state, board.bf_units[i][self_seat], features,
                                ext_per_side[ext_slot++]);
            extractSideFeatures(state, board.bf_units[i][opp_seat], features,
                                ext_per_side[ext_slot++]);
        } else {
            for (int j = 0; j < 49; ++j) features.push_back(0.0f);
            // Leave both ext_per_side slots at zero for this BF.
            ext_slot += 2;
        }
    }

    // ── Tier 1 expansion ────────────────────────────────────────────────────
    blockStartsAt(features, layout::kChainTargets, "kChainTargets");

    // Chain item targets: top-2 per slot, 4 slots = 8 floats.
    for (int slot = 0; slot < kMaxChainItems; ++slot) {
        if (slot < static_cast<int>(state.chain.items.size())) {
            auto& item = state.chain.items[slot];
            for (int t = 0; t < 2; ++t) {
                features.push_back(t < static_cast<int>(item.targets.size())
                    ? visibleCardId(state, item.targets[t], perspective) : 0.0f);
            }
        } else {
            features.push_back(0.0f);
            features.push_back(0.0f);
        }
    }

    // Cost modifier types: 2 per player × 2 players = 4 floats.
    blockStartsAt(features, layout::kCostModTypes, "kCostModTypes");
    auto emit_cost_mod_types = [&](PlayerId p) {
        bool next_spell = false, next_unit = false;
        for (auto& mod : state.player(p).cost_modifiers) {
            if (mod.next_spell_only) next_spell = true;
            if (mod.next_unit_only)  next_unit = true;
        }
        features.push_back(next_spell ? 1.0f : 0.0f);
        features.push_back(next_unit  ? 1.0f : 0.0f);
    };
    emit_cost_mod_types(perspective);
    emit_cost_mod_types(opp);

    // Per-BF scored flags: 2 per BF × 4 BFs = 8 floats.
    blockStartsAt(features, layout::kScoredFlags, "kScoredFlags");
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            BattlefieldId bf_id = state.battlefields[i].id;
            features.push_back(state.player(perspective)
                .battlefields_scored_this_turn.count(bf_id) ? 1.0f : 0.0f);
            features.push_back(state.player(opp)
                .battlefields_scored_this_turn.count(bf_id) ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
            features.push_back(0.0f);
        }
    }

    // Per-unit extended features: 12 per side × 2 sides × 4 BFs = 96 floats.
    // Slot order is the order they were collected in during the BF loop:
    // bf0/self, bf0/opp, bf1/self, ..., bf3/opp.
    blockStartsAt(features, layout::kUnitExt, "kUnitExt");
    for (int s = 0; s < 8; ++s)
        for (int j = 0; j < kExtPerSide; ++j)
            features.push_back(ext_per_side[s][j]);

    // ── Zone multi-hot encoding ──────────────────────────────────────────────
    // 5 zones × kCardVocabSize floats. Each vector contains the count of each
    // card_def_id present in that zone, card id i at position (i - 1). A
    // player knows the composition of their own deck and both players' public
    // trash and banishment. The opponent's deck is never written.
    blockStartsAt(features, layout::kZones, "kZones");
    auto emit_zone_multihot = [&](const std::vector<GameObjectId>& zone_ids) {
        const size_t start = features.size();
        features.resize(start + kCardVocabSize, 0.0f);
        for (auto oid : zone_ids) {
            if (!state.objectExists(oid)) continue;
            const uint32_t id = state.getObject(oid).card_def_id.raw();
            if (id < 1 || id > kCardVocabSize) continue;
            features[start + (id - 1)] += 1.0f;
        }
    };

    const auto& self_ps = state.player(perspective);
    const auto& opp_ps  = state.player(opp);

    emit_zone_multihot(self_ps.main_deck);      // self deck
    emit_zone_multihot(self_ps.trash);          // self trash
    emit_zone_multihot(self_ps.banishment);     // self banishment
    emit_zone_multihot(opp_ps.trash);           // opponent trash (public)
    emit_zone_multihot(opp_ps.banishment);      // opponent banishment (public)

    // Per-player turn flags affecting playable-actions: self, opp.
    blockStartsAt(features, layout::kPlayLockout, "kPlayLockout");
    features.push_back(self_ps.cant_play_cards_this_turn ? 1.0f : 0.0f);
    features.push_back(opp_ps.cant_play_cards_this_turn  ? 1.0f : 0.0f);

    // ── Tier 2/3 expansion ──────────────────────────────────────────────────
    //
    // For each BF (4) × side (self, opp) × top-3 unit (by current_might desc)
    // emit 8 fields:
    //   0: assault_value (base, not temp)
    //   1: shield_value
    //   2: deflect_value
    //   3: buff_count (permanent)
    //   4: temp_buff_count (expires at expiration step)
    //   5: has Tank keyword (0/1)
    //   6: has Backline keyword (0/1)
    //   7: has Ganking keyword (0/1)
    // Missing units zero-padded. Same sides in the same order (self, opp) and
    // the same sort as the base side block and the extended block, so one
    // (battlefield, side, unit) slot is the same unit in all three.
    auto emit_unit_t2 = [&](const std::vector<GameObjectId>& units) {
        struct U {
            int assault, shield, deflect;
            int buff_count, temp_buff_count;
            int tank, backline, ganking;
            int might;  // sort key
        };
        std::vector<U> infos;
        infos.reserve(units.size());
        for (auto uid : units) {
            auto& u = state.getObject(uid);
            infos.push_back({
                u.assault_value, u.shield_value, u.deflect_value,
                u.buff_count, u.temp_buff_count,
                u.hasKeyword(Keyword::Tank) ? 1 : 0,
                u.hasKeyword(Keyword::Backline) ? 1 : 0,
                u.hasKeyword(Keyword::Ganking) ? 1 : 0,
                u.current_might
            });
        }
        std::stable_sort(infos.begin(), infos.end(),
                         [](const U& a, const U& b) { return a.might > b.might; });
        for (int i = 0; i < kMaxBfUnitsPerSide; ++i) {
            if (i < static_cast<int>(infos.size())) {
                const U& u = infos[i];
                features.push_back(static_cast<float>(u.assault));
                features.push_back(static_cast<float>(u.shield));
                features.push_back(static_cast<float>(u.deflect));
                features.push_back(static_cast<float>(u.buff_count));
                features.push_back(static_cast<float>(u.temp_buff_count));
                features.push_back(static_cast<float>(u.tank));
                features.push_back(static_cast<float>(u.backline));
                features.push_back(static_cast<float>(u.ganking));
            } else {
                for (int f = 0; f < kPerUnitExtFieldsT2; ++f)
                    features.push_back(0.0f);
            }
        }
    };
    blockStartsAt(features, layout::kUnitT2, "kUnitT2");
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            // perspective ordering: self side first, then opp side
            emit_unit_t2(board.bf_units[i][self_seat]);
            emit_unit_t2(board.bf_units[i][opp_seat]);
        } else {
            for (int slot = 0; slot < 2 * kMaxBfUnitsPerSide * kPerUnitExtFieldsT2; ++slot)
                features.push_back(0.0f);
        }
    }

    // Extended globals (24 floats).
    // Ready base unit count (self, opp).
    blockStartsAt(features, layout::kExtGlobals, "kExtGlobals");
    auto count_ready_base = [&](int seat) {
        int n = 0;
        for (auto uid : board.base_units[seat]) {
            if (!state.getObject(uid).is_exhausted) n++;
        }
        return n;
    };
    features.push_back(static_cast<float>(count_ready_base(self_seat)));
    features.push_back(static_cast<float>(count_ready_base(opp_seat)));

    // Cost modifier magnitude sum (self, opp).
    auto cost_mod_sum = [](const auto& mods) {
        int s = 0;
        for (auto& m : mods) s += m.energy_reduction;
        return s;
    };
    features.push_back(static_cast<float>(cost_mod_sum(self_ps.cost_modifiers)));
    features.push_back(static_cast<float>(cost_mod_sum(opp_ps.cost_modifiers)));

    // Additional turn queue depth (self, opp).
    features.push_back(static_cast<float>(state.additionalTurnsOf(perspective)));
    features.push_back(static_cast<float>(state.additionalTurnsOf(opp)));

    // Contested-by-self per BF (×4).
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            const auto& bf = state.battlefields[i];
            features.push_back(bf.is_contested && bf.contested_by == perspective ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
        }
    }

    // combat_staged per BF (×4).
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            features.push_back(state.battlefields[i].combat_staged ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
        }
    }

    // showdown_staged per BF (×4).
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            features.push_back(state.battlefields[i].showdown_staged ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
        }
    }

    // Facedown age (max turns_since_hidden across facedown cards) per BF (×4).
    // 0 = no facedown or only hidden this turn (no Reaction yet);
    // 1+ = at least one facedown card is reaction-ready.
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            const auto& bf = state.battlefields[i];
            int max_age = 0;
            for (auto fid : bf.facedown) {
                if (!state.objectExists(fid)) continue;
                int hidden_on = state.getObject(fid).hidden_on_turn;
                if (hidden_on >= 0) {
                    int age = state.turn.turn_number - hidden_on;
                    if (age > max_age) max_age = age;
                }
            }
            features.push_back(static_cast<float>(max_age));
        } else {
            features.push_back(0.0f);
        }
    }

    // focus_holder (1=self, 0=opp, -1=no showdown / no focus assigned).
    if (state.turn.focus_holder.has_value()) {
        features.push_back(*state.turn.focus_holder == perspective ? 1.0f : 0.0f);
    } else {
        features.push_back(-1.0f);
    }

    // Total battlefield count.
    features.push_back(static_cast<float>(state.battlefields.size()));

    // Who brought each battlefield slot to the game: 0 nobody (a token
    // battlefield, or no battlefield in the slot), 1 self, 2 opponent. Slot
    // order is setup order (Player1's battlefield, then Player2's), the one
    // thing in this vector that would otherwise depend on the seat.
    blockStartsAt(features, layout::kBfContributedBy, "kBfContributedBy");
    for (int i = 0; i < layout::kBfSlots; ++i) {
        float who = 0.0f;
        if (i < static_cast<int>(state.battlefields.size())) {
            const PlayerId by = state.battlefields[i].contributed_by;
            if (by == perspective) who = 1.0f;
            else if (by == opp) who = 2.0f;
        }
        features.push_back(who);
    }

    // Padding to a multiple of 16 floats: always zero, carries nothing.
    blockStartsAt(features, layout::kPadding, "kPadding");
    features.resize(features.size() + layout::kPaddingDim, 0.0f);
    blockStartsAt(features, layout::kEnd, "the end");
}

int actionTypeIndex(IntentType type) {
    switch (type) {
        case IntentType::EndTurn: return 0;
        case IntentType::PassPriority: return 1;
        case IntentType::PassFocus: return 2;
        case IntentType::PlayCard: return 3;
        case IntentType::PlayActionCard: return 4;
        case IntentType::PlayReaction: return 5;
        case IntentType::StandardMove: return 6;
        case IntentType::ActivateAbility: return 7;
        case IntentType::ActivateActionAbility: return 8;
        case IntentType::MulliganDecision: return 9;
        case IntentType::AssignCombatDamage: return 10;
        case IntentType::HideCard: return 11;
        case IntentType::MakeChoice: return 12;
        case IntentType::ChooseBattlefield: return 13;
        default: return 0;
    }
}

std::vector<float> featurizeAction(const Intent& intent,
                                   const GameState& state) {
    std::vector<float> features;
    features.reserve(kActionFeatureDim);

    // Action type one-hot (14)
    int action_type = actionTypeIndex(intent.type);
    for (int i = 0; i < 14; ++i)
        features.push_back(i == action_type ? 1.0f : 0.0f);

    // Card involved
    // Objects by their card only where the decider sees it (batch 5b T2).
    const PlayerId who = intent.player;
    features.push_back(visibleCardId(state, intent.card, who));

    // Targets (pad to 2)
    for (int i = 0; i < 2; ++i) {
        if (i < static_cast<int>(intent.targets.size())) {
            features.push_back(visibleCardId(state, intent.targets[i], who));
        } else {
            features.push_back(0.0f);
        }
    }

    // Ability source
    features.push_back(visibleCardId(state, intent.ability_source, who));

    // Destination BF
    float dest_bf = -1.0f;
    if (intent.move_destination.has_value() &&
        std::holds_alternative<BattlefieldLocation>(*intent.move_destination))
        dest_bf = safeBfId(std::get<BattlefieldLocation>(*intent.move_destination).id);
    features.push_back(dest_bf);

    // Play location BF
    float play_bf = -1.0f;
    if (intent.play_location.has_value() &&
        std::holds_alternative<BattlefieldLocation>(*intent.play_location))
        play_bf = safeBfId(std::get<BattlefieldLocation>(*intent.play_location).id);
    features.push_back(play_bf);

    // Mulligan count
    features.push_back(static_cast<float>(intent.cards_to_mulligan.size()));

    // Unit def IDs (pad to 2)
    for (int i = 0; i < 2; ++i) {
        if (i < static_cast<int>(intent.units_to_move.size())) {
            features.push_back(visibleCardId(state, intent.units_to_move[i], who));
        } else {
            features.push_back(0.0f);
        }
    }

    // Chosen object def ID
    features.push_back(intent.chosen_objects.empty()
        ? 0.0f : visibleCardId(state, intent.chosen_objects[0], who));

    // Chosen battlefield: the slot a card is hidden at, or for the setup
    // choice the position in the player's battlefield list (-1 = neither).
    if (intent.battlefield_pool_index.valid() &&
        intent.battlefield_pool_index.raw() <= 15) {
        features.push_back(static_cast<float>(intent.battlefield_pool_index.raw()));
    } else {
        features.push_back(safeBfId(intent.chosen_battlefield));
    }

    while (static_cast<int>(features.size()) < kActionFeatureDim)
        features.push_back(0.0f);

    return features;
}

} // namespace riftbound::ml
