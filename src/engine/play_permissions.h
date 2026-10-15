#pragma once
/// @file play_permissions.h
/// Where a unit may be played: one rule for every play. A unit played from a
/// hand (GameEngine's generators) and a unit an effect plays (card_helpers.h
/// playLocationsFor) are played by the same steps (CR 420.3.b: "Treat all
/// steps of Play as normal"), so the same permissions apply to both: the
/// default (CR 355.2.a), what the card prints and what another card grants
/// the player (355.2.b), and [Ambush] (822.1.c).

#include "core/game_state.h"

namespace riftbound {

class Card;

/// Exactly one unit stands at `bf` and it is an enemy of `player`.
bool enemyUnitAloneAt(const GameState& state, PlayerId player, BattlefieldId bf);

/// [Ambush]'s location (CR 822.1.b, 822.1.d): `bf` has units of `player`, or
/// — for a card that widens it (Card::ambushToEnemyBattlefields) — enemy units.
bool ambushLocation(const GameState& state, const Card* card_obj, PlayerId player,
                    const BattlefieldState& bf);

/// Whether `player` may play the unit `card_obj` to `bf`: never where units
/// can't be played (Rockfall Path); a battlefield they control (CR 355.2.a);
/// a battlefield the card's own permission (Card::extraPlayLocations) or a
/// grant to the player's units (PlayerState::grant_friendly_units_open_bf,
/// grant_units_lone_enemy_bf) allows (355.2.b); with `unit` given, where its
/// [Ambush] allows (822.1.c). `card_obj` is null for a token with no card of
/// its own. "Can only play units to base" (PlayerState::units_play_base_only)
/// is the caller's: the engine's play gates and the effect-play lists ask it
/// themselves.
bool unitPlayableAt(const GameState& state, const Card* card_obj, PlayerId player,
                    const BattlefieldState& bf, const GameObject* unit = nullptr);

}  // namespace riftbound
