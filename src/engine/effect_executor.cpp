#include "effect_executor.h"

#include <algorithm>
#include <cassert>

namespace riftbound {

EffectExecutor::EffectExecutor(GameState& state, EventBus& events,
                               const CardDB& card_db)
    : state_(state), events_(events), card_db_(card_db) {}

// ═══════════════════════════════════════════════════════════════════════════════
// Atomic game operations
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::dealDamage(GameObjectId target, int amount,
                                 GameObjectId source) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: deal " + std::to_string(amount) + " damage to " +
                     obj.name + " (id=" + std::to_string(target) + ", was " +
                     std::to_string(obj.damage_marked) + "/" +
                     std::to_string(obj.current_might) + "M)");
    obj.damage_marked += amount;
    events_.emit(DamageDealtEvent{target, amount, source, false});
}

void EffectExecutor::killObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    if (obj.zone == ZoneType::Trash) return; // already dead
    events_.logTrace("EFFECT: kill " + obj.name + " (id=" + std::to_string(target) + ")");

    auto was_at = obj.location;
    auto controller = obj.controller;

    if (obj.isUnit()) {
        // Detach gear (CR 719.5)
        for (auto gear_id : obj.attachments) {
            if (state_.objectExists(gear_id)) {
                state_.getObject(gear_id).attached_to = std::nullopt;
            }
        }
        obj.attachments.clear();
        obj.attachment_might_bonus = 0;

        int might = obj.current_might;
        obj.zone = ZoneType::Trash;
        obj.location = std::nullopt;
        obj.damage_marked = 0;
        obj.combat_designation = CombatDesignation::None;
        state_.player(obj.owner).trash.push_back(target);

        events_.emit(UnitDiedEvent{target, controller,
            was_at.value_or(BaseLocation{controller}), might});
        events_.emit(LeftBoardEvent{target, controller, CardType::Unit,
            was_at.value_or(BaseLocation{controller}), ZoneType::Trash, true});
    } else if (obj.isGear()) {
        obj.zone = ZoneType::Trash;
        obj.location = std::nullopt;
        state_.player(obj.owner).trash.push_back(target);

        events_.emit(LeftBoardEvent{target, controller, CardType::Gear,
            was_at.value_or(BaseLocation{controller}), ZoneType::Trash, true});
    }
}

void EffectExecutor::drawCards(PlayerId player, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " draws " +
                     std::to_string(count) + " (deck=" +
                     std::to_string(state_.player(player).main_deck.size()) + ")");
    auto& ps = state_.player(player);
    int drawn = 0;
    for (int i = 0; i < count; ++i) {
        if (ps.main_deck.empty()) {
            // Burn Out (CR 431.2)
            if (ps.trash.empty()) break;
            events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                             " shuffling trash into deck, losing 1 point");
            ps.burned_out = true;
            for (auto cid : ps.trash) {
                state_.getObject(cid).zone = ZoneType::MainDeck;
                ps.main_deck.push_back(cid);
            }
            ps.trash.clear();
            if (rng_) {
                std::shuffle(ps.main_deck.begin(), ps.main_deck.end(), *rng_);
            }
            if (ps.score > 0) ps.score--;
            if (ps.main_deck.empty()) break;
        }
        auto card_id = ps.main_deck.back();
        ps.main_deck.pop_back();
        ps.hand.push_back(card_id);
        state_.getObject(card_id).zone = ZoneType::Hand;
        events_.logTrace("  DREW: " + state_.getObject(card_id).name +
                         " (id=" + std::to_string(card_id) + ")");
        drawn++;
    }
    if (drawn > 0) {
        events_.emit(CardsDrawnEvent{player, drawn});
    }
}

void EffectExecutor::bounceToHand(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: bounce " + obj.name + " (id=" + std::to_string(target) + ") to hand");
    auto was_at = obj.location;
    auto controller = obj.controller;

    obj.zone = ZoneType::Hand;
    obj.location = std::nullopt;
    obj.damage_marked = 0;
    obj.combat_designation = CombatDesignation::None;
    obj.is_exhausted = false;
    state_.player(obj.owner).hand.push_back(target);

    events_.emit(LeftBoardEvent{target, controller, obj.card_type,
        was_at.value_or(BaseLocation{controller}), ZoneType::Hand, false});
}

void EffectExecutor::giveTemporaryMight(GameObjectId target, int amount, int minimum) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);

    // Apply the buff
    obj.buff_count += amount;
    obj.temp_might_bonus += amount;
    obj.recomputeMight();

    // Enforce minimum (e.g., "to a minimum of 1 [M]")
    if (minimum > 0 && obj.current_might < minimum) {
        int correction = minimum - obj.current_might;
        obj.buff_count += correction;
        obj.temp_might_bonus += correction;
        obj.recomputeMight();
    }

    events_.logTrace("EFFECT: give " + obj.name + " " + std::to_string(amount) +
                     " [M] this turn -> " + std::to_string(obj.current_might) + "M" +
                     (minimum > 0 ? " (min " + std::to_string(minimum) + ")" : ""));
    events_.emit(ObjectStateChangedEvent{target, "buffed"});
}

void EffectExecutor::giveTemporaryKeyword(GameObjectId target,
                                            Keyword kw, int value) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: give " + obj.name + " keyword " + std::to_string(static_cast<int>(kw)) +
                     " (value=" + std::to_string(value) + ") this turn");
    obj.keywords.set(kw);
    if (kw == Keyword::Assault) { obj.assault_value += value; obj.temp_assault_value += value; }
    else if (kw == Keyword::Shield) { obj.shield_value += value; obj.temp_shield_value += value; }
    else if (kw == Keyword::Deflect) obj.deflect_value += value;
    obj.recomputeMight();
    events_.emit(ObjectStateChangedEvent{target, "buffed"});
}

void EffectExecutor::buffUnit(GameObjectId target) {
    giveTemporaryMight(target, 1);
}

void EffectExecutor::readyObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.is_exhausted = false;
    events_.emit(ObjectStateChangedEvent{target, "readied"});
}

void EffectExecutor::moveToBase(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    auto old_loc = obj.location;

    LocationId dest = BaseLocation{obj.controller};
    obj.location = dest;
    obj.zone = ZoneType::Base;

    events_.emit(UnitMovedEvent{target, obj.controller,
        old_loc.value_or(BaseLocation{obj.controller}), dest, false});
}

void EffectExecutor::stunUnit(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);

    // CR 423: already stunned units can't be stunned again
    if (obj.is_stunned) return;
    events_.logTrace("EFFECT: stun " + obj.name + " (id=" + std::to_string(target) + ")");

    obj.is_stunned = true;
    events_.emit(ObjectStateChangedEvent{target, "stunned"});
}

void EffectExecutor::discardCards(PlayerId player, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " discards " +
                     std::to_string(count) + " (hand=" +
                     std::to_string(state_.player(player).hand.size()) + ")");
    auto& ps = state_.player(player);
    std::vector<GameObjectId> discarded;

    for (int i = 0; i < count && !ps.hand.empty(); ++i) {
        GameObjectId to_discard = kInvalidId;

        if (agent_query_ && ps.hand.size() > 1) {
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent choice;
                choice.type = IntentType::MakeChoice;
                choice.player = player;
                choice.chosen_objects = {card_id};
                choices.push_back(choice);
            }
            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                to_discard = chosen.chosen_objects[0];
            }
        }

        // Fallback: discard last card in hand
        if (to_discard == kInvalidId) {
            to_discard = ps.hand.back();
        }

        auto it = std::find(ps.hand.begin(), ps.hand.end(), to_discard);
        if (it != ps.hand.end()) {
            events_.logTrace("  DISCARDED: " + state_.getObject(to_discard).name +
                             " (id=" + std::to_string(to_discard) + ")");
            ps.hand.erase(it);
            state_.getObject(to_discard).zone = ZoneType::Trash;
            ps.trash.push_back(to_discard);
            discarded.push_back(to_discard);
        }
    }

    if (!discarded.empty()) {
        ps.has_discarded_this_turn = true;
        events_.emit(CardsDiscardedEvent{player, discarded});
    }
}

void EffectExecutor::opponentDiscards(PlayerId opp, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(opp) + " forced to discard " +
                     std::to_string(count));
    discardCards(opp, count);
}

void EffectExecutor::recycleCards(PlayerId player,
                                   const std::vector<GameObjectId>& cards) {
    auto& ps = state_.player(player);
    auto to_recycle = cards;
    if (rng_) {
        std::shuffle(to_recycle.begin(), to_recycle.end(), *rng_);
    }
    for (auto card_id : to_recycle) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        obj.zone = ZoneType::MainDeck;
        obj.location = std::nullopt;
        ps.main_deck.insert(ps.main_deck.begin(), card_id); // bottom = front
    }
}

void EffectExecutor::banishObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: banish " + obj.name + " (id=" + std::to_string(target) + ")");
    auto was_at = obj.location;
    auto controller = obj.controller;

    obj.zone = ZoneType::Banishment;
    obj.location = std::nullopt;
    state_.player(obj.owner).banishment.push_back(target);

    events_.emit(LeftBoardEvent{target, controller, obj.card_type,
        was_at.value_or(BaseLocation{controller}), ZoneType::Banishment, false});
}

void EffectExecutor::healObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.damage_marked = 0;
    events_.emit(ObjectStateChangedEvent{target, "healed"});
}

void EffectExecutor::exhaustObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.is_exhausted = true;
    events_.emit(ObjectStateChangedEvent{target, "exhausted"});
}

void EffectExecutor::channelRunes(PlayerId player, int count, bool enter_exhausted) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " channels " +
                     std::to_string(count) + " runes" +
                     (enter_exhausted ? " exhausted" : ""));
    auto& ps = state_.player(player);
    for (int i = 0; i < count; ++i) {
        if (ps.rune_deck.empty()) break;
        auto rune_id = ps.rune_deck.back();
        ps.rune_deck.pop_back();

        auto& rune = state_.getObject(rune_id);
        rune.zone = ZoneType::Base;
        rune.location = BaseLocation{player};
        rune.is_exhausted = enter_exhausted;

        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Copy effects
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::copyUnit(GameObjectId token_id, GameObjectId source_id) {
    if (!state_.objectExists(token_id) || !state_.objectExists(source_id)) return;

    auto& token = state_.getObject(token_id);
    auto& src = state_.getObject(source_id);

    events_.logTrace("COPY: " + token.name + " (id=" + std::to_string(token_id) +
                     ") becomes copy of " + src.name + " (id=" + std::to_string(source_id) + ")");

    // Copy base traits only (CR 472.1.b.3: copy "printed" traits)
    token.name = src.name;
    token.card_def_id = src.card_def_id;
    token.card_type = src.card_type;
    token.tags = src.tags;
    token.domains = src.domains;
    token.base_might = src.base_might;
    token.keywords = src.keywords;
    token.assault_value = src.assault_value;
    token.shield_value = src.shield_value;
    token.deflect_value = src.deflect_value;

    // Keep token's own: owner, controller, location, zone, super_type (stays Token)
    // Reset computed values — aura recalculation will rebuild them
    token.current_might = token.base_might;
    token.aura_effects.clear();
    token.aura_might_bonus = 0;
    token.aura_keywords.reset();
    token.recomputeMight();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Token creation
// ═══════════════════════════════════════════════════════════════════════════════

GameObjectId EffectExecutor::createToken(PlayerId controller, CardType type,
                                          const std::string& name, int might,
                                          const std::vector<std::string>& tags,
                                          KeywordSet keywords, LocationId location,
                                          bool enter_ready) {
    auto id = state_.createObject();
    auto& obj = state_.getObject(id);
    obj.owner = controller;
    obj.controller = controller;
    obj.card_type = type;
    obj.super_type = SuperType::Token;
    obj.name = name;
    obj.base_might = might;
    obj.current_might = might;
    obj.tags = tags;
    obj.keywords = keywords;
    obj.location = location;
    obj.is_exhausted = !enter_ready && type == CardType::Unit; // units enter exhausted unless ready

    if (std::holds_alternative<BaseLocation>(location)) {
        obj.zone = ZoneType::Base;
    } else {
        obj.zone = ZoneType::BattlefieldZone;
    }

    obj.recomputeMight();

    events_.logTrace("TOKEN: created " + name + " (" + std::to_string(might) +
                     "M, id=" + std::to_string(id) + ") for " + toString(controller));

    events_.emit(TokenCreatedEvent{id, controller, type, name, location});
    events_.emit(EnteredBoardEvent{id, controller, type, location, false});

    return id;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Composite operations
// ═══════════════════════════════════════════════════════════════════════════════

std::pair<GameObjectId, std::vector<GameObjectId>>
EffectExecutor::revealUntil(PlayerId player, CardType condition) {
    auto& ps = state_.player(player);
    std::vector<GameObjectId> revealed;
    GameObjectId matched = kInvalidId;

    while (!ps.main_deck.empty()) {
        auto card_id = ps.main_deck.back();
        ps.main_deck.pop_back();
        revealed.push_back(card_id);

        auto& obj = state_.getObject(card_id);
        if (obj.card_type == condition) {
            matched = card_id;
            break;
        }
    }

    // Split into matched and rest
    std::vector<GameObjectId> rest;
    for (auto id : revealed) {
        if (id != matched) rest.push_back(id);
    }

    return {matched, rest};
}

void EffectExecutor::predict(PlayerId player, int count) {
    auto& ps = state_.player(player);
    int actual = std::min(count, static_cast<int>(ps.main_deck.size()));
    if (actual <= 0) return;

    events_.logTrace(std::string("PREDICT: ") + toString(player) +
                     " looks at top " + std::to_string(actual) + " cards");

    // Peek at top N cards (remove from deck temporarily)
    std::vector<GameObjectId> peeked;
    for (int i = 0; i < actual; ++i) {
        peeked.push_back(ps.main_deck.back());
        ps.main_deck.pop_back();
    }

    // Agent chooses which to recycle via MakeChoice
    // For now: random agent recycles all (keeps top of deck fresh)
    // Future: proper MakeChoice with multiple selections
    if (agent_query_) {
        // Offer choice: recycle each card or keep it
        for (auto card_id : peeked) {
            std::vector<Intent> choices;
            // Choice 1: recycle (put on bottom)
            Intent recycle_choice;
            recycle_choice.type = IntentType::MakeChoice;
            recycle_choice.player = player;
            recycle_choice.chosen_objects = {card_id};
            choices.push_back(recycle_choice);
            // Choice 2: keep on top
            Intent keep_choice;
            keep_choice.type = IntentType::MakeChoice;
            keep_choice.player = player;
            choices.push_back(keep_choice);

            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                // Recycle: put on bottom
                state_.getObject(card_id).zone = ZoneType::MainDeck;
                ps.main_deck.insert(ps.main_deck.begin(), card_id);
                events_.logTrace("PREDICT: recycled " + state_.getObject(card_id).name);
            } else {
                // Keep: put back on top
                ps.main_deck.push_back(card_id);
            }
        }
    } else {
        // No agent query — put all back on top (no recycling)
        for (int i = static_cast<int>(peeked.size()) - 1; i >= 0; --i) {
            ps.main_deck.push_back(peeked[i]);
        }
    }
}

std::vector<GameObjectId> EffectExecutor::revealAndChoose(PlayerId player, int count) {
    auto& ps = state_.player(player);
    int actual = std::min(count, static_cast<int>(ps.main_deck.size()));
    std::vector<GameObjectId> chosen_cards;
    if (actual <= 0) return chosen_cards;

    events_.logTrace(std::string("REVEAL: ") + toString(player) +
                     " reveals top " + std::to_string(actual));

    // Peek at top N
    std::vector<GameObjectId> revealed;
    for (int i = 0; i < actual; ++i) {
        revealed.push_back(ps.main_deck.back());
        ps.main_deck.pop_back();
    }

    // Agent chooses which to draw, rest get recycled
    for (auto card_id : revealed) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        events_.logTrace("  REVEALED: " + obj.name + " (id=" + std::to_string(card_id) + ")");

        if (agent_query_) {
            std::vector<Intent> choices;
            Intent draw_it;
            draw_it.type = IntentType::MakeChoice;
            draw_it.player = player;
            draw_it.chosen_objects = {card_id};
            choices.push_back(draw_it);
            Intent skip_it;
            skip_it.type = IntentType::MakeChoice;
            skip_it.player = player;
            choices.push_back(skip_it);

            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                ps.hand.push_back(card_id);
                obj.zone = ZoneType::Hand;
                chosen_cards.push_back(card_id);
                events_.logTrace("  CHOSE: draw " + obj.name);
            } else {
                // Recycle to bottom
                ps.main_deck.insert(ps.main_deck.begin(), card_id);
                events_.logTrace("  CHOSE: recycle " + obj.name);
            }
        } else {
            // No agent: put back on top
            ps.main_deck.push_back(card_id);
        }
    }

    if (!chosen_cards.empty()) {
        events_.emit(CardsDrawnEvent{player, static_cast<int>(chosen_cards.size())});
    }
    return chosen_cards;
}

void EffectExecutor::playIgnoringCost(PlayerId player, GameObjectId card) {
    if (!state_.objectExists(card)) return;
    auto& obj = state_.getObject(card);

    obj.zone = ZoneType::Base;
    obj.location = BaseLocation{player};
    obj.controller = player;

    if (obj.isUnit()) {
        obj.is_exhausted = true;
    }
    obj.recomputeMight();

    auto& ps = state_.player(player);
    ps.cards_played_this_turn++;

    events_.emit(CardPlayedEvent{card, player, obj.card_type,
        ps.cards_played_this_turn});
    events_.emit(EnteredBoardEvent{card, player, obj.card_type,
        BaseLocation{player}, true});
}

} // namespace riftbound
