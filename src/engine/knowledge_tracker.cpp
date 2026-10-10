#include "engine/knowledge_tracker.h"

#include <algorithm>
#include <array>
#include <tuple>

namespace riftbound::knowledge {

namespace {

constexpr std::array<PlayerId, 2> kPlayers = {PlayerId::Player1, PlayerId::Player2};

ZoneType zoneOf(Place place) {
    switch (place) {
        case Place::Hand:     return ZoneType::Hand;
        case Place::Facedown: return ZoneType::FacedownZone;
        default:              return ZoneType::MainDeck;
    }
}

int& seqOf(KnowledgeState& k, PlayerId owner, DeckEnd end) {
    auto& c = k.deck[playerIndex(owner)];
    return end == DeckEnd::Top ? c.top_seq : c.bottom_seq;
}

int seqOf(const KnowledgeState& k, PlayerId owner, DeckEnd end) {
    const auto& c = k.deck[playerIndex(owner)];
    return end == DeckEnd::Top ? c.top_seq : c.bottom_seq;
}

PlayerId ownerOf(const GameState& state, GameObjectId object) {
    auto it = state.objects.find(object);
    return it == state.objects.end() ? PlayerId::None : it->second.owner;
}

uint32_t groupOf(const KnowledgeState& k, GameObjectId object) {
    for (const auto& [id, g] : k.groups) {
        if (std::find(g.members.begin(), g.members.end(), object) != g.members.end())
            return id;
    }
    return 0;
}

void forgetPosition(HiddenCardFact& f) {
    f.end = DeckEnd::None;
    f.anchor = 0;
    f.group = 0;
}

/// A fact that says nothing: the owner's own card, somewhere in their deck.
bool isEmpty(const HiddenCardFact& f, bool own) {
    return own && !f.held && f.zone == ZoneType::MainDeck && f.end == DeckEnd::None;
}

void dropEmptyOwnFacts(GameState& state, PlayerId owner) {
    auto& facts = state.knowledge.facts[playerIndex(owner)];
    for (auto it = facts.begin(); it != facts.end();) {
        if (ownerOf(state, it->first) == owner && isEmpty(it->second, true))
            it = facts.erase(it);
        else
            ++it;
    }
}

/// Nobody knows a position in `owner`'s deck any more.
void clearDeckPositions(GameState& state, PlayerId owner) {
    auto& k = state.knowledge;
    for (PlayerId o : kPlayers) {
        for (auto& [id, f] : k.facts[playerIndex(o)]) {
            if (ownerOf(state, id) != owner) continue;
            if (f.zone == ZoneType::MainDeck && !f.held) forgetPosition(f);
        }
    }
    for (auto it = k.groups.begin(); it != k.groups.end();) {
        it = it->second.owner == owner ? k.groups.erase(it) : std::next(it);
    }
    k.deck[playerIndex(owner)] = DeckCounters{};
    dropEmptyOwnFacts(state, owner);
}

struct Leaving {
    GameObjectId object;
    PlayerId owner;
    Place from;
    Place to;
    DeckEnd from_end;
    int from_rank;
    bool slot_public;
    uint32_t group;
};

/// `observer` cannot tell which of `owner`'s cards just left `from`: every
/// card they knew there that it could have been is now less certain.
void obscure(GameState& state, PlayerId observer, const Leaving& m) {
    const bool left_zone = m.to != m.from;
    for (auto& [id, f] : state.knowledge.facts[playerIndex(observer)]) {
        if (ownerOf(state, id) != m.owner) continue;
        switch (m.from) {
            case Place::Hand:
                if (f.zone == ZoneType::Hand && !f.held) f.zone_confirmed = false;
                break;
            case Place::Deck:
                if (f.zone != ZoneType::MainDeck || f.held) break;
                if (f.end == DeckEnd::None) {
                    if (left_zone) f.zone_confirmed = false;
                } else if ((m.group != 0 && f.group == m.group) ||
                           (!m.slot_public && f.end == m.from_end)) {
                    forgetPosition(f);
                    if (left_zone) f.zone_confirmed = false;
                }
                break;
            case Place::Held:
                if (f.held) {
                    f.held = false;
                    f.zone = zoneOf(m.to);
                    f.zone_confirmed = false;
                    forgetPosition(f);
                }
                break;
            default:
                break;
        }
    }
}

/// Source side of a move. Returns who can identify the card.
std::array<bool, 2> leave(GameState& state, const Leaving& in, Seers seers) {
    auto& k = state.knowledge;
    Leaving m = in;
    m.group = m.from == Place::Deck ? groupOf(k, m.object) : 0;

    std::array<bool, 2> sees{};
    for (PlayerId o : kPlayers) {
        bool s = seers.includes(o) || m.from == Place::Public || m.to == Place::Public;
        if (o == m.owner && (m.from == Place::Hand || m.from == Place::Facedown ||
                             m.to == Place::Hand || m.to == Place::Facedown)) {
            s = true;
        }
        const auto& facts = k.facts[playerIndex(o)];
        if (auto it = facts.find(m.object);
            !s && it != facts.end() && it->second.zone_confirmed && !it->second.held) {
            const auto& f = it->second;
            // A facedown card is one physical card on the table.
            if (m.from == Place::Facedown && f.zone == ZoneType::FacedownZone) s = true;
            // They knew which card was in the slot it was taken from.
            if (m.from == Place::Deck && f.zone == ZoneType::MainDeck &&
                f.end != DeckEnd::None && f.group == 0 && m.slot_public &&
                m.from_end != DeckEnd::None) {
                s = true;
            }
        }
        sees[playerIndex(o)] = s;
    }
    for (PlayerId o : kPlayers) {
        if (!sees[playerIndex(o)]) obscure(state, o, m);
    }

    if (m.from == Place::Deck) {
        if (m.from_end == DeckEnd::None) {
            clearDeckPositions(state, m.owner);
        } else {
            // Slots nearer the end than the one removed keep their rank: move
            // their anchors with the counter.
            const int seq = seqOf(k, m.owner, m.from_end);
            for (PlayerId o : kPlayers) {
                for (auto& [id, f] : k.facts[playerIndex(o)]) {
                    if (id == m.object || ownerOf(state, id) != m.owner) continue;
                    if (f.held || f.zone != ZoneType::MainDeck) continue;
                    if (f.end == m.from_end && f.group == 0 &&
                        seq - f.anchor < m.from_rank) {
                        --f.anchor;
                    }
                }
            }
            for (auto& [id, g] : k.groups) {
                if (g.owner != m.owner || g.end != m.from_end) continue;
                if (id == m.group || seq - g.anchor < m.from_rank) --g.anchor;
            }
            --seqOf(k, m.owner, m.from_end);

            if (m.group != 0) {
                auto git = k.groups.find(m.group);
                if (git != k.groups.end()) {
                    auto& mem = git->second.members;
                    mem.erase(std::remove(mem.begin(), mem.end(), m.object), mem.end());
                    if (mem.empty()) k.groups.erase(git);
                }
            }
        }
        dropEmptyOwnFacts(state, m.owner);
    }
    return sees;
}

/// Destination side of a move, for the players who identified the card.
void arrive(GameState& state, GameObjectId object, PlayerId owner, Place to,
            DeckEnd to_end, int anchor, uint32_t group,
            const std::array<bool, 2>& sees) {
    auto& k = state.knowledge;
    for (PlayerId o : kPlayers) {
        if (!sees[playerIndex(o)]) continue;
        auto& facts = k.facts[playerIndex(o)];
        HiddenCardFact f;
        switch (to) {
            case Place::Public:
                facts.erase(object);
                continue;
            case Place::Hand:
            case Place::Facedown:
                if (o == owner) { facts.erase(object); continue; }
                f.zone = zoneOf(to);
                break;
            case Place::Held:
                f.zone = ZoneType::MainDeck;
                f.held = true;
                break;
            case Place::Deck:
                f.zone = ZoneType::MainDeck;
                if (to_end == DeckEnd::None) {
                    if (o == owner) { facts.erase(object); continue; }
                } else {
                    f.end = to_end;
                    f.anchor = anchor;
                    f.group = group;
                }
                break;
        }
        facts[object] = f;
    }
}

} // namespace

Place placeOf(ZoneType zone) {
    switch (zone) {
        case ZoneType::Hand:         return Place::Hand;
        case ZoneType::MainDeck:     return Place::Deck;
        case ZoneType::FacedownZone: return Place::Facedown;
        default:                     return Place::Public;
    }
}

void seen(GameState& state, GameObjectId object, Place where, Seers seers) {
    const PlayerId owner = ownerOf(state, object);
    if (owner == PlayerId::None || where == Place::Public) return;
    for (PlayerId o : kPlayers) {
        if (!seers.includes(o)) continue;
        auto& facts = state.knowledge.facts[playerIndex(o)];
        if (where == Place::Held) {
            HiddenCardFact f;
            f.held = true;
            facts[object] = f;
            continue;
        }
        if (o == owner && where != Place::Deck) continue;  // own hand, own facedown
        auto it = facts.find(object);
        if (where == Place::Deck) {
            // Identity only. Keep a slot they already knew.
            if (it != facts.end() && it->second.zone == ZoneType::MainDeck &&
                !it->second.held) {
                it->second.zone_confirmed = true;
            } else if (o != owner) {
                facts[object] = HiddenCardFact{};
            }
            continue;
        }
        HiddenCardFact f;
        f.zone = zoneOf(where);
        facts[object] = f;
    }
}

void seenInDeck(GameState& state, GameObjectId object, Seers seers,
                DeckEnd end, int rank) {
    const PlayerId owner = ownerOf(state, object);
    if (owner == PlayerId::None || end == DeckEnd::None) return;
    auto& k = state.knowledge;
    // A member of an unordered group seen in its slot is no longer "one of k"
    // for that player, but the others still are: leave the group alone and
    // give this player an exact fact.
    for (PlayerId o : kPlayers) {
        if (!seers.includes(o)) continue;
        HiddenCardFact f;
        f.end = end;
        f.anchor = seqOf(k, owner, end) - rank;
        k.facts[playerIndex(o)][object] = f;
    }
}

void moved(GameState& state, GameObjectId object, Place from, Place to,
           Seers seers, DeckSlot from_slot, DeckEnd to_end, bool slot_public) {
    const PlayerId owner = ownerOf(state, object);
    if (owner == PlayerId::None) return;
    auto& k = state.knowledge;
    const auto sees = leave(state,
        Leaving{object, owner, from, to, from_slot.end, from_slot.rank, slot_public, 0},
        seers);
    int anchor = 0;
    if (to == Place::Deck && to_end != DeckEnd::None) {
        anchor = ++seqOf(k, owner, to_end);
    }
    arrive(state, object, owner, to, to_end, anchor, 0, sees);
}

void movedTogether(GameState& state, const std::vector<GroupCard>& cards,
                   DeckEnd to_end) {
    if (cards.empty() || to_end == DeckEnd::None) return;
    if (cards.size() == 1) {
        moved(state, cards[0].object, cards[0].from, Place::Deck, cards[0].seers,
              {}, to_end);
        return;
    }
    auto& k = state.knowledge;
    const PlayerId owner = ownerOf(state, cards[0].object);
    if (owner == PlayerId::None) return;

    std::vector<std::array<bool, 2>> sees;
    sees.reserve(cards.size());
    for (const auto& c : cards) {
        sees.push_back(leave(state,
            Leaving{c.object, owner, c.from, Place::Deck, DeckEnd::None, 0, true, 0},
            c.seers));
    }
    const uint32_t gid = k.next_group++;
    DeckGroup g;
    g.owner = owner;
    g.end = to_end;
    seqOf(k, owner, to_end) += static_cast<int>(cards.size());
    g.anchor = seqOf(k, owner, to_end);
    for (const auto& c : cards) g.members.push_back(c.object);
    k.groups[gid] = std::move(g);
    for (size_t i = 0; i < cards.size(); ++i) {
        arrive(state, cards[i].object, owner, Place::Deck, to_end, 0, gid, sees[i]);
    }
}

void shuffled(GameState& state, PlayerId owner) {
    clearDeckPositions(state, owner);
}

void prune(GameState& state) {
    std::map<GameObjectId, Place> hidden;
    for (const auto& p : state.players) {
        for (auto id : p.hand) hidden[id] = Place::Hand;
        for (auto id : p.main_deck) hidden[id] = Place::Deck;
    }
    for (const auto& bf : state.battlefields) {
        for (auto id : bf.facedown) hidden[id] = Place::Facedown;
    }
    auto inPublicPile = [&](GameObjectId id, PlayerId owner) {
        if (owner == PlayerId::None) return true;
        const auto& p = state.player(owner);
        return std::find(p.trash.begin(), p.trash.end(), id) != p.trash.end() ||
               std::find(p.banishment.begin(), p.banishment.end(), id) != p.banishment.end();
    };
    for (PlayerId o : kPlayers) {
        auto& facts = state.knowledge.facts[playerIndex(o)];
        for (auto it = facts.begin(); it != facts.end();) {
            const PlayerId owner = ownerOf(state, it->first);
            auto h = hidden.find(it->first);
            bool drop;
            if (it->second.held) {
                drop = inPublicPile(it->first, owner);
            } else if (h == hidden.end()) {
                drop = true;
            } else {
                drop = o == owner && h->second != Place::Deck;
            }
            it = drop ? facts.erase(it) : std::next(it);
        }
    }
    for (auto git = state.knowledge.groups.begin(); git != state.knowledge.groups.end();) {
        auto& mem = git->second.members;
        mem.erase(std::remove_if(mem.begin(), mem.end(), [&](GameObjectId id) {
            auto h = hidden.find(id);
            return h == hidden.end() || h->second != Place::Deck;
        }), mem.end());
        git = mem.empty() ? state.knowledge.groups.erase(git) : std::next(git);
    }
}

std::vector<std::string> check(const GameState& state) {
    std::vector<std::string> out;
    auto say = [&](PlayerId o, GameObjectId id, const std::string& what) {
        out.push_back("P" + std::to_string(static_cast<int>(o)) + " object " +
                      std::to_string(id) + ": " + what);
    };
    for (PlayerId o : kPlayers) {
        GameKnowledgeView view(state, o);
        for (const auto& [id, fact] : state.knowledge.facts[playerIndex(o)]) {
            if (fact.held) continue;
            const PlayerId owner = ownerOf(state, id);
            if (owner == PlayerId::None) { say(o, id, "object does not exist"); continue; }
            if (!fact.zone_confirmed) continue;
            const auto& ps = state.player(owner);
            bool in_zone = false;
            if (fact.zone == ZoneType::Hand) {
                in_zone = std::find(ps.hand.begin(), ps.hand.end(), id) != ps.hand.end();
            } else if (fact.zone == ZoneType::MainDeck) {
                in_zone = std::find(ps.main_deck.begin(), ps.main_deck.end(), id) !=
                          ps.main_deck.end();
            } else {
                for (const auto& bf : state.battlefields) {
                    in_zone = in_zone || std::find(bf.facedown.begin(), bf.facedown.end(),
                                                   id) != bf.facedown.end();
                }
            }
            if (!in_zone) { say(o, id, "confirmed in a zone it is not in"); continue; }
            if (fact.end == DeckEnd::None) continue;
            const CardKnowledge k = view.about(id);
            if (k.deck_position == DeckPosition::Unknown) {
                say(o, id, "known slot is outside the deck");
                continue;
            }
            // main_deck: top = back.
            const int size = static_cast<int>(ps.main_deck.size());
            const int index = static_cast<int>(
                std::find(ps.main_deck.begin(), ps.main_deck.end(), id) - ps.main_deck.begin());
            const int from_end =
                k.deck_position == DeckPosition::Top ? size - 1 - index : index;
            if (from_end < k.deck_rank || from_end >= k.deck_rank + k.deck_rank_span) {
                say(o, id, "claimed slot " + std::to_string(k.deck_rank) + "+" +
                           std::to_string(k.deck_rank_span) + ", actual " +
                           std::to_string(from_end));
            }
        }
    }
    return out;
}

// ─── View ────────────────────────────────────────────────────────────────────

GameKnowledgeView::GameKnowledgeView(const GameState& state, PlayerId observer)
    : state_(state), observer_(observer) {
    for (PlayerId p : kPlayers) {
        const auto& ps = state_.player(p);
        for (auto id : ps.hand) hidden_[id] = {Place::Hand, p};
        for (auto id : ps.main_deck) hidden_[id] = {Place::Deck, p};
    }
    for (const auto& bf : state_.battlefields) {
        for (auto id : bf.facedown) hidden_[id] = {Place::Facedown, ownerOf(state_, id)};
    }
}

const HiddenCardFact* GameKnowledgeView::factFor(GameObjectId object) const {
    const auto& facts = state_.knowledge.facts[playerIndex(observer_)];
    auto it = facts.find(object);
    return it == facts.end() ? nullptr : &it->second;
}

bool GameKnowledgeView::position(const HiddenCardFact& fact, PlayerId owner,
                                 CardKnowledge& k) const {
    if (fact.end == DeckEnd::None || fact.held || !fact.zone_confirmed ||
        fact.zone != ZoneType::MainDeck) {
        return false;
    }
    const auto& ks = state_.knowledge;
    int rank = 0;
    int span = 1;
    DeckEnd end = fact.end;
    if (fact.group != 0) {
        auto git = ks.groups.find(fact.group);
        if (git == ks.groups.end()) return false;
        end = git->second.end;
        rank = seqOf(ks, owner, end) - git->second.anchor;
        span = static_cast<int>(git->second.members.size());
    } else {
        rank = seqOf(ks, owner, end) - fact.anchor;
    }
    const int size = static_cast<int>(state_.player(owner).main_deck.size());
    if (rank < 0 || span < 1 || rank + span > size) return false;
    k.deck_position = end == DeckEnd::Top ? DeckPosition::Top : DeckPosition::Bottom;
    k.deck_rank = rank;
    k.deck_rank_span = span;
    return true;
}

CardKnowledge GameKnowledgeView::about(GameObjectId object) const {
    CardKnowledge k;
    k.object = object;
    auto oit = state_.objects.find(object);
    if (oit == state_.objects.end()) return k;
    const GameObject& obj = oit->second;
    k.owner = obj.owner;

    const HiddenCardFact* fact = factFor(object);
    auto hit = hidden_.find(object);
    if (hit == hidden_.end()) {
        if (fact && fact->held) {
            k.identity_known = true;
            k.card_def_id = obj.card_def_id;
            k.last_known_zone = ZoneType::MainDeck;
            return k;
        }
        k.last_known_zone = obj.zone;
        if (obj.zone == ZoneType::RuneDeck) return k;
        // A card held by another player's "look at" is in no zone list either.
        if (obj.zone == ZoneType::MainDeck || obj.zone == ZoneType::Hand) return k;
        k.identity_known = true;
        k.card_def_id = obj.card_def_id;
        return k;
    }

    const Place place = hit->second.place;
    if (obj.owner == observer_ && place != Place::Deck) {
        k.identity_known = true;
        k.card_def_id = obj.card_def_id;
        k.last_known_zone = zoneOf(place);
        return k;
    }
    if (!fact) {
        k.last_known_zone = zoneOf(place);
        return k;
    }
    k.identity_known = true;
    k.card_def_id = obj.card_def_id;
    k.last_known_zone = fact->zone;
    k.zone_confirmed = fact->zone_confirmed;
    position(*fact, obj.owner, k);
    return k;
}

std::vector<CardKnowledge> GameKnowledgeView::knownHiddenCards(PlayerId owner) const {
    std::vector<CardKnowledge> out;
    for (const auto& [id, fact] : state_.knowledge.facts[playerIndex(observer_)]) {
        if (ownerOf(state_, id) != owner) continue;
        if (!fact.held && hidden_.find(id) == hidden_.end()) continue;
        out.push_back(about(id));
    }
    if (owner == observer_) {
        for (const auto& [id, w] : hidden_) {
            if (w.place == Place::Facedown && w.owner == owner) out.push_back(about(id));
        }
    }
    std::sort(out.begin(), out.end(), [](const CardKnowledge& a, const CardKnowledge& b) {
        return std::tie(a.card_def_id, a.object) < std::tie(b.card_def_id, b.object);
    });
    return out;
}

int GameKnowledgeView::unknownCount(PlayerId owner, ZoneType zone) const {
    const Place place = placeOf(zone);
    if (place == Place::Public) return 0;
    int total = 0;
    for (const auto& [id, w] : hidden_) {
        if (w.owner == owner && w.place == place) ++total;
    }
    if (owner == observer_ && place != Place::Deck) return 0;
    int known = 0;
    for (const auto& [id, fact] : state_.knowledge.facts[playerIndex(observer_)]) {
        if (ownerOf(state_, id) != owner || fact.held) continue;
        if (fact.zone != zone || !fact.zone_confirmed) continue;
        if (owner == observer_) {
            CardKnowledge k;
            if (!position(fact, owner, k)) continue;
        }
        ++known;
    }
    return std::max(0, total - known);
}

std::vector<std::pair<CardDefId, int>> GameKnowledgeView::ownDeckComposition() const {
    std::map<CardDefId, int> counts;
    for (auto id : state_.player(observer_).main_deck) {
        auto oit = state_.objects.find(id);
        if (oit == state_.objects.end()) continue;
        if (const HiddenCardFact* fact = factFor(id)) {
            CardKnowledge k;
            if (position(*fact, observer_, k)) continue;
        }
        ++counts[oit->second.card_def_id];
    }
    return {counts.begin(), counts.end()};
}

std::vector<CardKnowledge> GameKnowledgeView::knownDeckEnd(PlayerId owner,
                                                           DeckEnd end) const {
    const int size = static_cast<int>(state_.player(owner).main_deck.size());
    // First slot (counted from `end`) -> the cards known to start there.
    std::map<int, std::vector<CardKnowledge>> by_rank;
    for (const auto& [id, fact] : state_.knowledge.facts[playerIndex(observer_)]) {
        if (ownerOf(state_, id) != owner) continue;
        auto hit = hidden_.find(id);
        if (hit == hidden_.end() || hit->second.place != Place::Deck) continue;
        CardKnowledge k = about(id);
        if (k.deck_position == DeckPosition::Unknown) continue;
        const DeckEnd fact_end =
            k.deck_position == DeckPosition::Top ? DeckEnd::Top : DeckEnd::Bottom;
        if (fact_end != end) {
            k.deck_rank = size - (k.deck_rank + k.deck_rank_span);
            k.deck_position = end == DeckEnd::Top ? DeckPosition::Top : DeckPosition::Bottom;
        }
        by_rank[k.deck_rank].push_back(k);
    }
    std::vector<CardKnowledge> out;
    int next = 0;
    for (auto& [rank, cards] : by_rank) {
        if (rank != next) break;
        const int span = cards.front().deck_rank_span;
        if (static_cast<int>(cards.size()) != span) break;
        std::sort(cards.begin(), cards.end(), [](const CardKnowledge& a, const CardKnowledge& b) {
            return std::tie(a.card_def_id, a.object) < std::tie(b.card_def_id, b.object);
        });
        out.insert(out.end(), cards.begin(), cards.end());
        next += span;
    }
    return out;
}

std::vector<CardKnowledge> GameKnowledgeView::knownDeckTop(PlayerId owner) const {
    return knownDeckEnd(owner, DeckEnd::Top);
}

std::vector<CardKnowledge> GameKnowledgeView::knownDeckBottom(PlayerId owner) const {
    return knownDeckEnd(owner, DeckEnd::Bottom);
}

} // namespace riftbound::knowledge
