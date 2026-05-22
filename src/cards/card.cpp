#include "cards/card.h"
#include "core/game_state.h"

namespace riftbound {

std::vector<GameObjectId> Card::enumerateLegalTargets(
    const GameState& state, PlayerId controller) const {

    auto reqs = getTargetRequirements();
    if (reqs.count == 0) return {};

    std::vector<GameObjectId> targets;

    for (auto& [id, obj] : state.objects) {
        // Must be on the board (has a location)
        if (!obj.location.has_value()) continue;

        // Type filter
        if (reqs.must_be_unit && !obj.isUnit()) continue;
        if (reqs.must_be_gear && !obj.isGear()) continue;

        // Ownership filter
        if (reqs.must_be_enemy && obj.controller == controller) continue;
        if (reqs.must_be_friendly && obj.controller != controller) continue;

        // Location filter
        if (reqs.must_be_at_battlefield && !obj.isAtBattlefield()) continue;

        // Stat filter
        if (reqs.max_might > 0 && obj.current_might > reqs.max_might) continue;

        targets.push_back(id);
    }

    return targets;
}

} // namespace riftbound
