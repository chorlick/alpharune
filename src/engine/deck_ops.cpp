#include "engine/deck_ops.h"

namespace riftbound::deck {

namespace {

bool contains(const std::vector<GameObjectId>& v, GameObjectId id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

void enterDeck(GameState& state, GameObjectId object) {
    auto& obj = state.getObject(object);
    obj.zone = ZoneType::MainDeck;
    obj.location = std::nullopt;
}

} // namespace

Place whereIs(const GameState& state, GameObjectId object) {
    if (!state.objectExists(object)) return Place::Public;
    const auto& obj = state.getObject(object);
    if (obj.owner != PlayerId::None) {
        const auto& ps = state.player(obj.owner);
        if (contains(ps.hand, object)) return Place::Hand;
        if (contains(ps.main_deck, object)) return Place::Deck;
    }
    for (const auto& bf : state.battlefields) {
        if (contains(bf.facedown, object)) return Place::Facedown;
    }
    // In no list: a caller took it out of its zone and has not placed it yet.
    if (obj.zone == ZoneType::Hand) return Place::Hand;
    if (obj.zone == ZoneType::MainDeck) return Place::Held;
    if (obj.is_hidden) return Place::Facedown;
    return Place::Public;
}

GameObjectId takeTop(GameState& state, PlayerId owner, Place to, Seers seers) {
    auto& d = state.player(owner).main_deck;
    if (d.empty()) return kInvalidId;
    const GameObjectId id = d.back();
    d.pop_back();
    knowledge::moved(state, id, Place::Deck, to, seers,
                     knowledge::DeckSlot{DeckEnd::Top, 0});
    return id;
}

bool take(GameState& state, GameObjectId object, Place to, Seers seers,
          bool slot_public) {
    for (auto& ps : state.players) {
        auto& d = ps.main_deck;
        auto it = std::find(d.begin(), d.end(), object);
        if (it == d.end()) continue;
        const int from_top = static_cast<int>(d.end() - it) - 1;
        d.erase(it);
        knowledge::moved(state, object, Place::Deck, to, seers,
                         knowledge::DeckSlot{DeckEnd::Top, from_top}, DeckEnd::None,
                         slot_public);
        return true;
    }
    return false;
}

GameObjectId drawTop(GameState& state, PlayerId owner, Seers seers) {
    const GameObjectId id = takeTop(state, owner, Place::Hand, seers);
    if (id == kInvalidId || !state.objectExists(id)) return id;
    state.player(owner).hand.push_back(id);
    auto& obj = state.getObject(id);
    obj.zone = ZoneType::Hand;
    obj.location = std::nullopt;
    return id;
}

void putInHand(GameState& state, GameObjectId object, Seers seers,
               std::optional<Place> from, PlayerId no_owner_hand) {
    if (!state.objectExists(object)) return;
    // A card that left as it was looked at is not the held card any more
    // (GameState::left_while_seen): the looking effect does not move it.
    if (from == Place::Held && state.leftWhileSeen(object)) return;
    const Place was = from.value_or(whereIs(state, object));
    auto& obj = state.getObject(object);
    obj.zone = ZoneType::Hand;
    obj.location = std::nullopt;
    state.player(obj.owner != PlayerId::None ? obj.owner : no_owner_hand)
        .hand.push_back(object);
    // Taken off a deck by the caller without `take`: nothing to tell (the
    // deck hooks own every move out of a deck).
    if (was == Place::Deck || was == Place::Hand) return;
    knowledge::moved(state, object, was, Place::Hand, seers);
}

void putFacedown(GameState& state, GameObjectId object, BattlefieldId at) {
    if (!state.objectExists(object)) return;
    const Place was = whereIs(state, object);
    state.battlefields[at].facedown.push_back(object);
    auto& obj = state.getObject(object);
    obj.zone = ZoneType::FacedownZone;
    obj.location = std::nullopt;
    knowledge::moved(state, object, was, Place::Facedown, knowledge::nobody());
}

void putOnTop(GameState& state, PlayerId owner, GameObjectId object, Place from,
              Seers seers) {
    if (from == Place::Held && state.leftWhileSeen(object)) return;   // (see putInHand)
    enterDeck(state, object);
    state.player(owner).main_deck.push_back(object);
    knowledge::moved(state, object, from, Place::Deck, seers, {}, DeckEnd::Top);
}

void putOnBottom(GameState& state, PlayerId owner, GameObjectId object, Place from,
                 Seers seers) {
    if (from == Place::Held && state.leftWhileSeen(object)) return;   // (see putInHand)
    enterDeck(state, object);
    auto& d = state.player(owner).main_deck;
    d.insert(d.begin(), object);
    knowledge::moved(state, object, from, Place::Deck, seers, {}, DeckEnd::Bottom);
}

void putOnBottomTogether(GameState& state, PlayerId owner,
                         const std::vector<Placed>& cards) {
    std::vector<knowledge::GroupCard> group;
    auto& d = state.player(owner).main_deck;
    for (const auto& c : cards) {
        if (c.from == Place::Held && state.leftWhileSeen(c.object)) continue;   // (see putInHand)
        enterDeck(state, c.object);
        d.insert(d.begin(), c.object);
        group.push_back({c.object, c.from, c.seers});
    }
    knowledge::movedTogether(state, group, DeckEnd::Bottom);
}

void putToShuffle(GameState& state, PlayerId owner, GameObjectId object, Place from,
                  Seers seers) {
    enterDeck(state, object);
    state.player(owner).main_deck.push_back(object);
    knowledge::moved(state, object, from, Place::Deck, seers);
}

} // namespace riftbound::deck
