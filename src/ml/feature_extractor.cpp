#include "ml/feature_extractor.h"

#include <algorithm>

namespace riftbound::ml {

namespace {

// Cards have card_def_id in [1, kCardVocabSize]. Tokens have kInvalidId
// (== UINT32_MAX == 4,294,967,295), which is catastrophic if fed to a Linear
// layer as a raw float — it produces astronomical activations and blows up
// the loss during training. This helper returns 0.0f for invalid/token IDs.
inline float safeCardId(CardDefId id) {
    if (id == kInvalidId) return 0.0f;
    if (id < 1 || id > kCardVocabSize) return 0.0f;
    return static_cast<float>(id);
}

// BattlefieldIds are typically 0..3 but the type is uint32_t. If a bug ever
// sets one to kInvalidId (UINT32_MAX), pushing it as a raw float would poison
// the action features. Clamp to a sane bound — anything above 15 is almost
// certainly a corruption signal.
inline float safeBfId(BattlefieldId id) {
    if (id == kInvalidId) return -1.0f;
    if (id > 15) return -1.0f;
    return static_cast<float>(id);
}

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

        CardDefId gear_def = 0;
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

    std::sort(unit_infos.begin(), unit_infos.end(),
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
                           std::vector<float>& out) {
    auto& ps = state.player(player);

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

    // Hand card IDs (10) — self only, opponent's hidden
    for (int i = 0; i < 10; ++i) {
        if (is_self && i < static_cast<int>(ps.hand.size())) {
            out.push_back(safeCardId(state.getObject(ps.hand[i]).card_def_id));
        } else {
            out.push_back(0.0f);
        }
    }

    // Hand card costs (10) — self only
    for (int i = 0; i < 10; ++i) {
        if (is_self && i < static_cast<int>(ps.hand.size())) {
            auto& obj = state.getObject(ps.hand[i]);
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
    auto runes = state.runesInBase(player);
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
    int base_might = 0, base_damage = 0, base_exhausted = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.controller == player && obj.isAtBase()) {
            base_unit_infos.push_back({obj.card_def_id, obj.current_might});
            base_might += obj.current_might;
            base_damage += obj.damage_marked;
            if (obj.is_exhausted) base_exhausted++;
        }
    }
    out.push_back(static_cast<float>(base_unit_infos.size()));
    out.push_back(static_cast<float>(base_might));
    out.push_back(static_cast<float>(base_damage));
    out.push_back(static_cast<float>(base_exhausted));

    // Base unit IDs top 3 sorted by might desc (3)
    std::sort(base_unit_infos.begin(), base_unit_infos.end(),
              [](auto& a, auto& b) { return a.might > b.might; });
    for (int i = 0; i < 3; ++i)
        out.push_back(i < static_cast<int>(base_unit_infos.size())
                       ? safeCardId(base_unit_infos[i].def_id) : 0.0f);

    // Base gear count (1) + top 2 gear IDs (2)
    std::vector<CardDefId> gear_ids;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == player && obj.isAtBase())
            gear_ids.push_back(obj.card_def_id);
    }
    out.push_back(static_cast<float>(gear_ids.size()));
    for (int i = 0; i < 2; ++i)
        out.push_back(i < static_cast<int>(gear_ids.size())
                       ? safeCardId(gear_ids[i]) : 0.0f);

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
    features.reserve(kStateFeatureDim);

    PlayerId opp = opponent(perspective);

    // Global features (10)
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
    for (int i = 0; i < kMaxChainItems; ++i) {
        if (i < static_cast<int>(state.chain.items.size())) {
            auto& item = state.chain.items[i];
            CardDefId src_def = 0;
            if (state.objectExists(item.source))
                src_def = state.getObject(item.source).card_def_id;
            features.push_back(safeCardId(src_def));
            features.push_back(item.controller == perspective ? 1.0f : 0.0f);
        } else {
            features.push_back(0.0f);
            features.push_back(0.0f);
        }
    }

    // Self player features (70)
    extractPlayerFeatures(state, perspective, true, perspective, card_db, features);

    // Opponent player features (70)
    extractPlayerFeatures(state, opp, false, perspective, card_db, features);

    // Battlefield features (49 each × 4 = 196). Also collects per-side
    // extended features (12 floats per side) into ext_per_side for later
    // appending — kept here so the top-3 sort order matches extractSideFeatures.
    constexpr int kExtPerSide = kMaxBfUnitsPerSide * kPerUnitExtFields; // 12
    float ext_per_side[8 /* 4 BFs × 2 sides */][kExtPerSide] = {};
    int ext_slot = 0;
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(state.battlefields.size())) {
            auto& bf = state.battlefields[i];

            features.push_back(state.objectExists(bf.card_object_id)
                ? safeCardId(state.getObject(bf.card_object_id).card_def_id) : 0.0f);

            float ctrl = 0;
            if (bf.controller.has_value())
                ctrl = (*bf.controller == PlayerId::Player1) ? 1.0f : 2.0f;
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

            // Per-side features (20 each × 2 = 40) + extended per-side (12 each).
            auto bf_loc = BattlefieldLocation{bf.id};
            auto p1u = state.unitsAt(bf_loc, PlayerId::Player1);
            auto p2u = state.unitsAt(bf_loc, PlayerId::Player2);
            extractSideFeatures(state, p1u, features, ext_per_side[ext_slot++]);
            extractSideFeatures(state, p2u, features, ext_per_side[ext_slot++]);
        } else {
            for (int j = 0; j < 49; ++j) features.push_back(0.0f);
            // Leave both ext_per_side slots at zero for this BF.
            ext_slot += 2;
        }
    }

    // ── Tier 1 expansion (positions 354..469) ───────────────────────────────

    // Chain item targets: top-2 per slot, 4 slots = 8 floats.
    for (int slot = 0; slot < kMaxChainItems; ++slot) {
        if (slot < static_cast<int>(state.chain.items.size())) {
            auto& item = state.chain.items[slot];
            for (int t = 0; t < 2; ++t) {
                CardDefId td = 0;
                if (t < static_cast<int>(item.targets.size())
                    && state.objectExists(item.targets[t]))
                    td = state.getObject(item.targets[t]).card_def_id;
                features.push_back(safeCardId(td));
            }
        } else {
            features.push_back(0.0f);
            features.push_back(0.0f);
        }
    }

    // Cost modifier types: 2 per player × 2 players = 4 floats.
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
    // Slot order is identical to the order they were collected during the BF
    // loop: bf0/p1, bf0/p2, bf1/p1, ..., bf3/p2.
    for (int s = 0; s < 8; ++s)
        for (int j = 0; j < kExtPerSide; ++j)
            features.push_back(ext_per_side[s][j]);

    // ── Zone multi-hot encoding (positions 470..4404) ────────────────────────
    // 5 zones × kCardVocabSize (787) = 3935 floats. Each vector contains the
    // count of each card_def_id present in that zone. card_def_id values are
    // 1..787, encoded at position (id - 1) in the vector. Perfect-information
    // encoding: a player knows their own deck/trash/banishment and the
    // opponent's public zones (trash, banishment). Opponent's deck stays
    // hidden — never serialized.
    auto emit_zone_multihot = [&](const std::vector<GameObjectId>& zone_ids) {
        const size_t start = features.size();
        features.resize(start + kCardVocabSize, 0.0f);
        for (auto oid : zone_ids) {
            if (!state.objectExists(oid)) continue;
            CardDefId id = state.getObject(oid).card_def_id;
            if (id < 1 || id > kCardVocabSize) continue;
            features[start + (id - 1)] += 1.0f;
        }
    };

    const auto& self_ps = state.player(perspective);
    const auto& opp_ps  = state.player(opp);

    emit_zone_multihot(self_ps.main_deck);      // self deck (787)
    emit_zone_multihot(self_ps.trash);          // self trash (787)
    emit_zone_multihot(self_ps.banishment);     // self banishment (787)
    emit_zone_multihot(opp_ps.trash);           // opponent trash (787, public)
    emit_zone_multihot(opp_ps.banishment);      // opponent banishment (787, public)

    // Per-player turn flags affecting playable-actions (positions 4405..4406).
    features.push_back(self_ps.cant_play_cards_this_turn ? 1.0f : 0.0f);
    features.push_back(opp_ps.cant_play_cards_this_turn  ? 1.0f : 0.0f);

    return features;
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
    float card_def = 0;
    if (intent.card != kInvalidId && state.objectExists(intent.card))
        card_def = safeCardId(state.getObject(intent.card).card_def_id);
    features.push_back(card_def);

    // Targets (pad to 2)
    for (int i = 0; i < 2; ++i) {
        if (i < static_cast<int>(intent.targets.size()) &&
            state.objectExists(intent.targets[i])) {
            features.push_back(safeCardId(state.getObject(intent.targets[i]).card_def_id));
        } else {
            features.push_back(0.0f);
        }
    }

    // Ability source
    float src_def = 0;
    if (intent.ability_source != kInvalidId && state.objectExists(intent.ability_source))
        src_def = safeCardId(state.getObject(intent.ability_source).card_def_id);
    features.push_back(src_def);

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
        if (i < static_cast<int>(intent.units_to_move.size()) &&
            state.objectExists(intent.units_to_move[i])) {
            features.push_back(safeCardId(state.getObject(intent.units_to_move[i]).card_def_id));
        } else {
            features.push_back(0.0f);
        }
    }

    // Chosen object def ID
    float chosen_obj = 0.0f;
    if (!intent.chosen_objects.empty() &&
        state.objectExists(intent.chosen_objects[0])) {
        chosen_obj = safeCardId(state.getObject(intent.chosen_objects[0]).card_def_id);
    }
    features.push_back(chosen_obj);

    // Chosen battlefield ID
    features.push_back(safeBfId(intent.chosen_battlefield));

    while (static_cast<int>(features.size()) < kActionFeatureDim)
        features.push_back(0.0f);

    return features;
}

} // namespace riftbound::ml
