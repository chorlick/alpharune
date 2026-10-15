#include "engine/play_permissions.h"

#include "cards/card.h"

namespace riftbound {

bool enemyUnitAloneAt(const GameState& state, PlayerId player, BattlefieldId bf) {
    int units = 0, enemies = 0;
    for (const auto& [id, obj] : state.objects.atLocation(LocationId{BattlefieldLocation{bf}})) {
        if (!obj.isUnit() || obj.battlefieldId() != bf) continue;
        ++units;
        if (obj.controller != player) ++enemies;
    }
    return units == 1 && enemies == 1;
}

bool ambushLocation(const GameState& state, const Card* card_obj, PlayerId player,
                    const BattlefieldState& bf) {
    if (bf.hasUnitsFrom(player, state.objects)) return true;
    return card_obj && card_obj->ambushToEnemyBattlefields() &&
           bf.hasUnitsFrom(opponent(player), state.objects);
}

bool unitPlayableAt(const GameState& state, const Card* card_obj, PlayerId player,
                    const BattlefieldState& bf, const GameObject* unit) {
    const auto& ps = state.player(player);
    if (bf.blocks_unit_play) return false;  // Rockfall Path
    // [Ambush] "I may be played to a battlefield where you control Units"
    // (CR 822.1.b): a location permission of its own, not tied to the
    // [Reaction] timing it also grants — so it holds in the Main Phase too
    // (testing phase D37), and for a play by an effect (822.1.c, 420.3.b).
    if (unit && unit->hasKeyword(Keyword::Ambush) && ambushLocation(state, card_obj, player, bf))
        return true;
    // Play-to-location permissions the card prints (CR 355.2), and the ones
    // another card grants every unit this player plays.
    const unsigned extra = card_obj ? card_obj->extraPlayLocations() : 0u;
    const bool controlled = bf.controller.has_value() && *bf.controller == player;
    const bool open = !bf.controller.has_value();
    const bool enemy_occupied = bf.controller.has_value() && *bf.controller != player &&
                                bf.hasUnitsFrom(opponent(player), state.objects);
    if (controlled) return true;  // default: controlled BFs
    if (open && ((extra & Card::kPlayToOpenBattlefield) ||
                 ps.grant_friendly_units_open_bf)) return true;   // Miss Fortune, Buccaneer
    if (enemy_occupied && (extra & Card::kPlayToOccupiedEnemyBattlefield)) return true;
    if (extra & Card::kPlayToAnyBattlefield) return true;
    if (((extra & Card::kPlayToLoneEnemyBattlefield) || ps.grant_units_lone_enemy_bf) &&
        enemyUnitAloneAt(state, player, bf.id)) return true;             // Arachnoid Horror
    return false;
}

}  // namespace riftbound
