#pragma once
/// @file keyword_abilities.h
/// Triggered abilities that belong to a KEYWORD rather than to a card.
///
/// [Vision] (CR 817) and [Weaponmaster] (CR 821) are "When this is played /
/// when you play me" triggered abilities. A permanent can have the keyword
/// from a continuous effect ("Your Mechs have [Vision]", "Your Sand Soldiers
/// have [Weaponmaster]") and may be a token with no card class, so the
/// ability cannot live on the permanent's Card. Each one is a stateless Card
/// object here instead: TriggerManager queues a chain item with
/// ChainItem::keyword_ability set, and resolution dispatches to the object
/// returned by keywordAbilityCard — the choice helpers (confirmOptional,
/// pickTarget) work as they do for any card.

#include "cards/card.h"
#include "core/types.h"

namespace riftbound {

/// The ability object for `keyword`, or null if the keyword has no triggered
/// ability of its own. Never registered in the CardRegistry / CardDB.
Card* keywordAbilityCard(Keyword keyword);

/// How many separate instances of `keyword`'s ability trigger for `object`
/// as it is played (CR 817.2 / 821.1.c.7: multiple instances trigger
/// separately): one per continuous effect that grants it, plus one for the
/// keyword on the object itself unless its own Card class already implements
/// the ability (`own_card_fires`).
int keywordAbilityInstances(const GameObject& object, Keyword keyword, bool own_card_fires);

} // namespace riftbound
