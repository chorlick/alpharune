#include "intent.h"
#include <sstream>

namespace riftbound {

std::string Intent::describe() const {
    std::ostringstream ss;
    ss << toString(player) << ": " << toString(type);

    switch (type) {
        case IntentType::StandardMove:
            ss << " [" << units_to_move.size() << " units]";
            break;
        case IntentType::MulliganDecision:
            ss << " [" << cards_to_mulligan.size() << " cards]";
            break;
        case IntentType::AssignCombatDamage:
            ss << " [" << damage_assignments.size() << " assignments]";
            break;
        case IntentType::PlayFirstDecision:
            ss << " [" << (choose_first ? "first" : "second") << "]";
            break;
        default:
            if (card != kInvalidId) ss << " card=" << card;
            if (!targets.empty()) ss << " targets=" << targets.size();
            break;
    }

    return ss.str();
}

} // namespace riftbound
