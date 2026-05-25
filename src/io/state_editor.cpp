#include "state_editor.h"

#include "core/game_state.h"
#include "core/game_object.h"

#include <algorithm>

namespace riftbound {

namespace {

PlayerState* findPlayerState(GameState& state, PlayerId player) {
    if (player != PlayerId::Player1 && player != PlayerId::Player2) return nullptr;
    return &state.player(player);
}

std::vector<GameObjectId>* zoneVectorFor(PlayerState& ps, ZoneType zone) {
    switch (zone) {
        case ZoneType::MainDeck:   return &ps.main_deck;
        case ZoneType::RuneDeck:   return &ps.rune_deck;
        case ZoneType::Hand:       return &ps.hand;
        case ZoneType::Trash:      return &ps.trash;
        case ZoneType::Banishment: return &ps.banishment;
        default: return nullptr;
    }
}

} // namespace

bool StateEditor::removeFromZoneVector(GameState& state, GameObjectId object_id) {
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state.player(pid);
        for (auto* vec : {&ps.main_deck, &ps.rune_deck, &ps.hand,
                          &ps.trash, &ps.banishment}) {
            auto it = std::find(vec->begin(), vec->end(), object_id);
            if (it != vec->end()) {
                vec->erase(it);
                return true;
            }
        }
        if (ps.champion_zone == object_id) {
            ps.champion_zone = kInvalidId;
            return true;
        }
        if (ps.legend_zone == object_id) {
            ps.legend_zone = kInvalidId;
            return true;
        }
    }
    return false;
}

void StateEditor::insertIntoZoneVector(GameState& state,
                                       GameObjectId object_id,
                                       PlayerId target_player,
                                       ZoneType target_zone,
                                       bool to_top) {
    auto* ps = findPlayerState(state, target_player);
    if (!ps) return;
    auto* vec = zoneVectorFor(*ps, target_zone);
    if (vec) {
        // Convention: top-of-deck == back of vector (per PlayerState comment).
        if (to_top) vec->push_back(object_id);
        else        vec->insert(vec->begin(), object_id);
        return;
    }
    if (target_zone == ZoneType::ChampionZone) {
        ps->champion_zone = object_id;
        return;
    }
    if (target_zone == ZoneType::LegendZone) {
        ps->legend_zone = object_id;
        return;
    }
    // Other zones (BattlefieldZone, Chain, FacedownZone) are not backed
    // by player vectors — caller handles destination semantics.
}

EditResult StateEditor::moveObject(GameState& state,
                                   GameObjectId object_id,
                                   PlayerId target_player,
                                   ZoneType target_zone,
                                   bool to_top) {
    if (state.objects.find(object_id) == state.objects.end()) {
        return EditResult::fail("object not found: " + std::to_string(static_cast<int>(object_id)));
    }
    if (target_player != PlayerId::Player1 && target_player != PlayerId::Player2) {
        return EditResult::fail("target_player must be Player1 or Player2");
    }
    if (target_zone == ZoneType::BattlefieldZone) {
        return EditResult::fail("use moveObjectToBattlefield for battlefield moves");
    }

    auto& obj = state.getObject(object_id);
    removeFromZoneVector(state, object_id);
    obj.zone = target_zone;
    obj.combat_designation = CombatDesignation::None;
    obj.controller = target_player;
    if (target_zone == ZoneType::Base) {
        // Base is on-board; units have a LocationId pointing at their
        // controller's base. Clear the existing location first so a
        // unit moving from a battlefield to base loses its BF binding.
        obj.location = BaseLocation{target_player};
    } else {
        obj.location.reset();  // off-board zones have no location
    }
    insertIntoZoneVector(state, object_id, target_player, target_zone, to_top);
    return EditResult::success();
}

EditResult StateEditor::moveObjectToBattlefield(GameState& state,
                                                GameObjectId object_id,
                                                BattlefieldId battlefield_id) {
    if (state.objects.find(object_id) == state.objects.end()) {
        return EditResult::fail("object not found");
    }
    bool bf_exists = false;
    for (auto& bf : state.battlefields) {
        if (bf.id == battlefield_id) { bf_exists = true; break; }
    }
    if (!bf_exists) {
        return EditResult::fail("battlefield not found: " + std::to_string(static_cast<int>(battlefield_id)));
    }

    auto& obj = state.getObject(object_id);
    removeFromZoneVector(state, object_id);
    obj.zone = ZoneType::BattlefieldZone;
    obj.location = BattlefieldLocation{battlefield_id};
    return EditResult::success();
}

EditResult StateEditor::setObjectExhausted(GameState& state,
                                           GameObjectId object_id,
                                           bool exhausted) {
    if (state.objects.find(object_id) == state.objects.end()) {
        return EditResult::fail("object not found");
    }
    state.getObject(object_id).is_exhausted = exhausted;
    return EditResult::success();
}

EditResult StateEditor::setObjectDamage(GameState& state,
                                        GameObjectId object_id,
                                        int damage) {
    if (state.objects.find(object_id) == state.objects.end()) {
        return EditResult::fail("object not found");
    }
    state.getObject(object_id).damage_marked = std::max(0, damage);
    return EditResult::success();
}

EditResult StateEditor::setObjectMight(GameState& state,
                                       GameObjectId object_id,
                                       int might) {
    if (state.objects.find(object_id) == state.objects.end()) {
        return EditResult::fail("object not found");
    }
    state.getObject(object_id).current_might = std::max(0, might);
    return EditResult::success();
}

EditResult StateEditor::setPlayerScore(GameState& state, PlayerId player, int score) {
    auto* ps = findPlayerState(state, player);
    if (!ps) return EditResult::fail("invalid player");
    ps->score = std::max(0, score);
    return EditResult::success();
}

EditResult StateEditor::setPlayerEnergy(GameState& state, PlayerId player, int amount) {
    auto* ps = findPlayerState(state, player);
    if (!ps) return EditResult::fail("invalid player");
    ps->rune_pool.energy = std::max(0, amount);
    return EditResult::success();
}

EditResult StateEditor::setPlayerPower(GameState& state,
                                       PlayerId player,
                                       int domain_idx,
                                       int amount) {
    auto* ps = findPlayerState(state, player);
    if (!ps) return EditResult::fail("invalid player");
    amount = std::max(0, amount);
    if (domain_idx == -1) {
        ps->rune_pool.universal_power = amount;
        return EditResult::success();
    }
    const int domain_count = static_cast<int>(Domain::Count);
    if (domain_idx < 0 || domain_idx >= domain_count) {
        return EditResult::fail("domain_idx out of range [0, " +
                                std::to_string(domain_count - 1) + "] or -1");
    }
    ps->rune_pool.power[domain_idx] = amount;
    return EditResult::success();
}

EditResult StateEditor::setPhase(GameState& state, TurnPhase phase) {
    state.turn.phase = phase;
    return EditResult::success();
}

EditResult StateEditor::setTurnPlayer(GameState& state, PlayerId player) {
    if (player != PlayerId::Player1 && player != PlayerId::Player2) {
        return EditResult::fail("invalid player");
    }
    state.turn.turn_player = player;
    return EditResult::success();
}

EditResult StateEditor::reorderMainDeck(GameState& state,
                                        PlayerId player,
                                        const std::vector<GameObjectId>& new_order) {
    auto* ps = findPlayerState(state, player);
    if (!ps) return EditResult::fail("invalid player");

    if (new_order.size() != ps->main_deck.size()) {
        return EditResult::fail("reorder size mismatch: deck has " +
                                std::to_string(ps->main_deck.size()) +
                                ", got " + std::to_string(new_order.size()));
    }
    // Multiset equality — new_order must contain exactly the current ids.
    auto sorted_old = ps->main_deck;
    auto sorted_new = new_order;
    std::sort(sorted_old.begin(), sorted_old.end());
    std::sort(sorted_new.begin(), sorted_new.end());
    if (sorted_old != sorted_new) {
        return EditResult::fail("reorder must contain the same object ids as current deck");
    }
    ps->main_deck = new_order;
    return EditResult::success();
}

} // namespace riftbound
