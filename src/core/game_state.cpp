#include "game_state.h"

#include <stdexcept>
#include <string>

namespace riftbound {

// ─── BattlefieldState ────────────────────────────────────────────────────────
// (These and the GameState queries below read the board index of the object
// table — the objects at the location, in the table's order — where they used
// to walk every object of the game: plans/06 item 6.10. Same ids, same order.)

std::vector<GameObjectId> BattlefieldState::unitsControlledBy(
    PlayerId player, const ObjectTable& objects) const {
    std::vector<GameObjectId> result;
    for (const auto& [oid, obj] : objects.atLocation(LocationId{BattlefieldLocation{id}})) {
        if (obj.isUnit() && obj.controller == player) result.push_back(oid);
    }
    return result;
}

bool BattlefieldState::hasUnitsFrom(
    PlayerId player, const ObjectTable& objects) const {
    for (const auto& [oid, obj] : objects.atLocation(LocationId{BattlefieldLocation{id}})) {
        if (obj.isUnit() && obj.controller == player) return true;
    }
    return false;
}

// ─── GameState queries ───────────────────────────────────────────────────────

void GameState::throwInvalidObjectId(GameObjectId id) {
    throw std::logic_error("Invalid game object ID: " + std::to_string(id));
}

std::vector<GameObjectId> GameState::objectsAt(LocationId loc) const {
    std::vector<GameObjectId> result;
    for (const auto& [oid, obj] : objects.atLocation(loc)) result.push_back(oid);
    return result;
}

std::vector<GameObjectId> GameState::unitsAt(LocationId loc,
                                              PlayerId controller) const {
    std::vector<GameObjectId> result;
    for (const auto& [oid, obj] : objects.atLocation(loc)) {
        if (obj.isUnit() && obj.controller == controller) result.push_back(oid);
    }
    return result;
}

std::vector<GameObjectId> GameState::allUnitsControlledBy(
    PlayerId controller) const {
    std::vector<GameObjectId> result;
    for (const auto& [oid, obj] : objects.onBoard()) {
        if (obj.isUnit() && obj.controller == controller) result.push_back(oid);
    }
    return result;
}

std::vector<GameObjectId> GameState::objectsInBase(PlayerId p) const {
    LocationId base_loc = BaseLocation{p};
    return objectsAt(base_loc);
}

std::vector<GameObjectId> GameState::runesInBase(PlayerId p) const {
    std::vector<GameObjectId> result;
    for (const auto& [oid, obj] : objects.atLocation(LocationId{BaseLocation{p}})) {
        if (obj.isRune()) result.push_back(oid);
    }
    return result;
}

} // namespace riftbound
