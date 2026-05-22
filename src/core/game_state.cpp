#include "game_state.h"

namespace riftbound {

// ─── BattlefieldState ────────────────────────────────────────────────────────

std::vector<GameObjectId> BattlefieldState::unitsControlledBy(
    PlayerId player,
    const std::unordered_map<GameObjectId, GameObject>& objects) const {
    std::vector<GameObjectId> result;
    for (auto& [oid, obj] : objects) {
        if (obj.isUnit() && obj.controller == player &&
            obj.zone == ZoneType::Base && obj.isAtBattlefield()) {
            auto bf_id = obj.battlefieldId();
            if (bf_id && *bf_id == id) {
                result.push_back(oid);
            }
        }
    }
    return result;
}

bool BattlefieldState::hasUnitsFrom(
    PlayerId player,
    const std::unordered_map<GameObjectId, GameObject>& objects) const {
    for (auto& [oid, obj] : objects) {
        if (obj.isUnit() && obj.controller == player && obj.isAtBattlefield()) {
            auto bf_id = obj.battlefieldId();
            if (bf_id && *bf_id == id) {
                return true;
            }
        }
    }
    return false;
}

// ─── GameState queries ───────────────────────────────────────────────────────

std::vector<GameObjectId> GameState::objectsAt(LocationId loc) const {
    std::vector<GameObjectId> result;
    for (auto& [oid, obj] : objects) {
        if (obj.location.has_value() && *obj.location == loc) {
            result.push_back(oid);
        }
    }
    return result;
}

std::vector<GameObjectId> GameState::unitsAt(LocationId loc,
                                              PlayerId controller) const {
    std::vector<GameObjectId> result;
    for (auto& [oid, obj] : objects) {
        if (obj.isUnit() && obj.controller == controller &&
            obj.location.has_value() && *obj.location == loc) {
            result.push_back(oid);
        }
    }
    return result;
}

std::vector<GameObjectId> GameState::allUnitsControlledBy(
    PlayerId controller) const {
    std::vector<GameObjectId> result;
    for (auto& [oid, obj] : objects) {
        if (obj.isUnit() && obj.controller == controller &&
            obj.location.has_value()) {
            result.push_back(oid);
        }
    }
    return result;
}

std::vector<GameObjectId> GameState::objectsInBase(PlayerId p) const {
    LocationId base_loc = BaseLocation{p};
    return objectsAt(base_loc);
}

std::vector<GameObjectId> GameState::runesInBase(PlayerId p) const {
    std::vector<GameObjectId> result;
    LocationId base_loc = BaseLocation{p};
    for (auto& [oid, obj] : objects) {
        if (obj.isRune() && obj.location.has_value() &&
            *obj.location == base_loc) {
            result.push_back(oid);
        }
    }
    return result;
}

} // namespace riftbound
