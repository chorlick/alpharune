#include "ml/entity_tokens.h"

#include "core/game_object.h"

#include <tuple>
#include <utility>

namespace riftbound::ml {

namespace {

// Helper to map (perspective, owner) to {is_perspective, side-aware zone}.
struct SideTag {
    bool is_perspective;
};
inline SideTag tagFor(PlayerId perspective, PlayerId owner) {
    return {owner == perspective};
}

// Resolve a card's domain to a single int. Cards can have multiple
// domains (e.g., Calm+Fury runes), but the tokenizer carries only the
// first for the categorical embedding. Multi-domain handling is
// model-side (future: emit a multi-hot domain vector instead).
int32_t firstDomainId(const GameObject& obj) {
    if (obj.domains.empty()) return static_cast<int32_t>(Domain::Count);  // "none"
    return static_cast<int32_t>(obj.domains.front());
}

// Per-token stance derivation.
EntityStance stanceFor(const GameObject& obj) {
    if (obj.is_hidden) return EntityStance::Facedown;
    if (obj.is_stunned) return EntityStance::Stunned;
    if (obj.is_exhausted) return EntityStance::Exhausted;
    if (obj.location.has_value()) return EntityStance::Ready;
    return EntityStance::None;
}

// Spatial-node mapping for an on-board GameObject.
int32_t spatialNodeFor(const GameObject& obj, PlayerId perspective) {
    if (!obj.location.has_value()) return kSpatialNodeOffBoard;
    if (auto bf_loc = std::get_if<BattlefieldLocation>(&*obj.location)) {
        int bf = static_cast<int>(bf_loc->id);
        if (bf < 0 || bf >= 6) return kSpatialNodeOffBoard;
        return kSpatialNodeBfBase + bf;
    }
    if (auto base_loc = std::get_if<BaseLocation>(&*obj.location)) {
        return base_loc->player == perspective
            ? kSpatialNodeSelfBase
            : kSpatialNodeOppBase;
    }
    return kSpatialNodeOffBoard;
}

class TokenBuilder {
public:
    explicit TokenBuilder(EntityTokens& out) : out_(out) {
        out_.card_def_id.assign(kMaxEntityTokens, 0);
        out_.zone_id.assign(kMaxEntityTokens,
                             static_cast<int32_t>(EntityZone::Unknown));
        out_.domain_id.assign(kMaxEntityTokens,
                                static_cast<int32_t>(Domain::Count));
        out_.stance_id.assign(kMaxEntityTokens,
                                static_cast<int32_t>(EntityStance::None));
        out_.is_perspective.assign(kMaxEntityTokens, 0);
        out_.chain_index.assign(kMaxEntityTokens, -1);
        out_.spatial_node.assign(kMaxEntityTokens, kSpatialNodeOffBoard);
        out_.valid_mask.assign(kMaxEntityTokens, 0);
        out_.stats.assign(kMaxEntityTokens * kEntityStatsDim, 0.0f);
    }

    /// Emit a token. Returns false if we're at capacity (token dropped).
    bool emit(int32_t card_def_id, EntityZone zone, int32_t domain_id,
               EntityStance stance, bool is_perspective, int32_t chain_idx,
               int32_t spatial_node, float might, float damage,
               float buff_count, float energy_cost, float power_cost) {
        if (out_.n_entities >= kMaxEntityTokens) return false;
        int i = out_.n_entities++;
        out_.card_def_id[i]    = card_def_id;
        out_.zone_id[i]        = static_cast<int32_t>(zone);
        out_.domain_id[i]      = domain_id;
        out_.stance_id[i]      = static_cast<int32_t>(stance);
        out_.is_perspective[i] = is_perspective ? 1 : 0;
        out_.chain_index[i]    = chain_idx;
        out_.spatial_node[i]   = spatial_node;
        out_.valid_mask[i]     = 1;
        float* s = out_.stats.data() + i * kEntityStatsDim;
        s[0] = might;
        s[1] = damage;
        s[2] = buff_count;
        s[3] = energy_cost;
        s[4] = power_cost;
        return true;
    }

private:
    EntityTokens& out_;
};

}  // namespace

EntityTokens extractEntityTokens(const GameState& state,
                                  PlayerId perspective,
                                  const CardDB& card_db) {
    EntityTokens out;
    TokenBuilder tb(out);

    // ── Pass 1: per-zone tokens for both players ──────────────────────
    //
    // Hidden-info masking: opp hand and main_deck card identities are
    // emitted as card_def_id=0 (unknown) but the tokens themselves are
    // emitted so the model sees the COUNT and zone. Public zones
    // (trash, banishment, on-board) keep identities. Facedown at BFs is
    // emitted as identity=0 unless perspective owns it.
    // Bounds-safe energy/power cost lookup. CardDB.get() throws on
    // unknown ids; we defensively check id is in 1..size() and return
    // (0, 0) otherwise. Lets test states use synthetic ids without
    // triggering a registry lookup failure.
    auto safe_cost = [&](CardDefId id) -> std::pair<float, float> {
        if (id == kInvalidId) return {0.0f, 0.0f};
        if (static_cast<size_t>(id) == 0 ||
            static_cast<size_t>(id) > card_db.size()) {
            return {0.0f, 0.0f};
        }
        const auto& def = card_db.get(id);
        return {static_cast<float>(def.energy_cost),
                static_cast<float>(def.power_cost > 0 ? def.power_cost : 0)};
    };

    // Domain lookup mirror of safe_cost — needed for chain items where
    // we look up def.domains directly.
    auto safe_first_domain = [&](CardDefId id) -> int32_t {
        if (id == kInvalidId) return static_cast<int32_t>(Domain::Count);
        if (static_cast<size_t>(id) == 0 ||
            static_cast<size_t>(id) > card_db.size()) {
            return static_cast<int32_t>(Domain::Count);
        }
        const auto& def = card_db.get(id);
        if (def.domains.empty()) return static_cast<int32_t>(Domain::Count);
        return static_cast<int32_t>(def.domains.front());
    };

    for (PlayerId pid : {PlayerId::Player1, PlayerId::Player2}) {
        bool is_self = (pid == perspective);
        const auto& ps = state.player(pid);

        auto emit_zoneset = [&](const std::vector<GameObjectId>& ids,
                                  EntityZone zone, bool reveal_identity) {
            for (auto id : ids) {
                if (!state.objectExists(id)) continue;
                const auto& obj = state.getObject(id);
                int32_t cdef = reveal_identity
                    ? static_cast<int32_t>(obj.card_def_id) : 0;
                int32_t domain = reveal_identity ? firstDomainId(obj)
                                                 : static_cast<int32_t>(Domain::Count);
                float energy = 0.0f, power = 0.0f;
                if (reveal_identity) {
                    std::tie(energy, power) = safe_cost(obj.card_def_id);
                }
                tb.emit(cdef, zone, domain, EntityStance::None,
                         is_self, /*chain_idx=*/-1,
                         /*spatial_node=*/kSpatialNodeOffBoard,
                         /*might=*/0.0f, /*damage=*/0.0f,
                         /*buff_count=*/0.0f,
                         energy, power);
            }
        };

        // Self gets full visibility; opponent's hand + main_deck are
        // identity-masked (cards are emitted but card_def_id=0).
        if (is_self) {
            emit_zoneset(ps.hand,      EntityZone::SelfHand,     true);
            emit_zoneset(ps.main_deck, EntityZone::SelfMainDeck, true);
            emit_zoneset(ps.rune_deck, EntityZone::SelfRuneDeck, true);
            emit_zoneset(ps.trash,     EntityZone::SelfTrash,    true);
            emit_zoneset(ps.banishment,EntityZone::SelfBanish,   true);
        } else {
            emit_zoneset(ps.hand,      EntityZone::OppHand,      false);
            emit_zoneset(ps.main_deck, EntityZone::OppMainDeck,  false);
            emit_zoneset(ps.rune_deck, EntityZone::OppRuneDeck,  false);
            emit_zoneset(ps.trash,     EntityZone::OppTrash,     true);
            emit_zoneset(ps.banishment,EntityZone::OppBanish,    true);
        }

        // Champion zone — public (the chosen champion is openly declared).
        auto emit_singleton = [&](GameObjectId id, EntityZone zone) {
            if (id == kInvalidId || !state.objectExists(id)) return;
            const auto& obj = state.getObject(id);
            int32_t cdef = static_cast<int32_t>(obj.card_def_id);
            auto [energy, power] = safe_cost(obj.card_def_id);
            tb.emit(cdef, zone, firstDomainId(obj), stanceFor(obj),
                     is_self, /*chain_idx=*/-1,
                     /*spatial_node=*/(is_self ? kSpatialNodeSelfBase
                                                : kSpatialNodeOppBase),
                     static_cast<float>(obj.current_might),
                     static_cast<float>(obj.damage_marked),
                     static_cast<float>(obj.buff_count),
                     energy, power);
        };
        emit_singleton(ps.champion_zone, is_self ? EntityZone::SelfChampion
                                                  : EntityZone::OppChampion);
        emit_singleton(ps.legend_zone,   is_self ? EntityZone::SelfLegend
                                                  : EntityZone::OppLegend);
    }

    // ── Pass 2: on-board game objects (units, gear, runes at BFs/bases) ──
    //
    // Iterating state.objects gets EVERY object. We've already emitted
    // hand/deck/trash/banishment/champion/legend above (those have no
    // `location`). Here we pick up everything WITH a location — units,
    // gear, runes on board.
    for (const auto& [id, obj] : state.objects) {
        if (!obj.location.has_value()) continue;
        // Hidden-on-board cards (facedown at BF) — emit as Facedown with
        // identity masked unless perspective owns it.
        bool perspective_owns = (obj.controller == perspective);
        bool reveal_identity = !obj.is_hidden || perspective_owns;

        EntityZone zone;
        if (std::holds_alternative<BaseLocation>(*obj.location)) {
            zone = perspective_owns ? EntityZone::SelfBase
                                     : EntityZone::OppBase;
        } else {
            zone = obj.is_hidden ? EntityZone::Facedown
                                  : EntityZone::Battlefield;
        }

        int32_t cdef = reveal_identity
            ? static_cast<int32_t>(obj.card_def_id) : 0;
        int32_t domain = reveal_identity ? firstDomainId(obj)
                                          : static_cast<int32_t>(Domain::Count);
        float energy = 0.0f, power = 0.0f;
        if (reveal_identity) {
            std::tie(energy, power) = safe_cost(obj.card_def_id);
        }
        tb.emit(cdef, zone, domain, stanceFor(obj),
                 perspective_owns, /*chain_idx=*/-1,
                 spatialNodeFor(obj, perspective),
                 static_cast<float>(obj.current_might),
                 static_cast<float>(obj.damage_marked),
                 static_cast<float>(obj.buff_count),
                 energy, power);
    }

    // ── Pass 3: chain items ──────────────────────────────────────────
    //
    // Chain items are public (their identity is known to both players).
    // chain_index encodes the position in the stack (0 = oldest, top
    // resolves first per LIFO + sinusoidal pos enc on chain tokens).
    for (size_t i = 0; i < state.chain.items.size(); ++i) {
        const auto& item = state.chain.items[i];
        bool ctrl_is_perspective = (item.controller == perspective);
        int32_t cdef = static_cast<int32_t>(item.card_def_id);
        int32_t domain = safe_first_domain(item.card_def_id);
        auto [energy, power] = safe_cost(item.card_def_id);
        tb.emit(cdef, EntityZone::ChainItem, domain, EntityStance::None,
                 ctrl_is_perspective, /*chain_idx=*/static_cast<int32_t>(i),
                 /*spatial_node=*/kSpatialNodeOffBoard,
                 /*might=*/0.0f, /*damage=*/0.0f,
                 /*buff_count=*/0.0f,
                 energy, power);
    }

    return out;
}

}  // namespace riftbound::ml
