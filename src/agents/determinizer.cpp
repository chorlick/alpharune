#include "agents/determinizer.h"

#include "core/knowledge_view.h"
#include "engine/knowledge_tracker.h"

#include <algorithm>
#include <set>
#include <tuple>

namespace riftbound {

namespace {

using knowledge::Place;

constexpr int kCopiesPerName = 3;    // CR 103.3.b
constexpr int kSignatureCards = 3;   // CR 103.3.d
constexpr int kRejectionTries = 64;

bool isMainDeckType(const CardDef& d) {
    return d.card_type == CardType::Unit || d.card_type == CardType::Spell ||
           d.card_type == CardType::Gear;
}

/// One hidden slot of a player: an object id and the kind of place it is in.
struct Slot {
    GameObjectId id;
    Place place;
};

/// Every hidden slot of `owner`, in a fixed order: hand, Main Deck (bottom to
/// top, as stored), facedown piles (battlefield order), then objects held out
/// of every zone list by a "look at" (ascending id).
std::vector<Slot> hiddenSlots(const GameState& state, PlayerId owner,
                              const std::set<GameObjectId>& listed) {
    std::vector<Slot> out;
    const auto& ps = state.player(owner);
    for (auto id : ps.hand) out.push_back({id, Place::Hand});
    for (auto id : ps.main_deck) out.push_back({id, Place::Deck});
    for (const auto& bf : state.battlefields) {
        for (auto id : bf.facedown) {
            if (state.objectExists(id) && state.getObject(id).owner == owner)
                out.push_back({id, Place::Facedown});
        }
    }
    std::vector<GameObjectId> held;
    for (const auto& [id, obj] : state.objects) {
        if (obj.owner != owner || obj.is_token) continue;
        const ZoneType z = obj.zone;
        if (z != ZoneType::MainDeck && z != ZoneType::Hand) continue;
        if (listed.count(id)) continue;
        held.push_back(id);
    }
    std::sort(held.begin(), held.end());
    for (auto id : held) out.push_back({id, Place::Held});
    return out;
}

/// Ids in a hand, a Main Deck or a facedown pile (either player).
std::set<GameObjectId> listedHidden(const GameState& state) {
    std::set<GameObjectId> out;
    for (const auto& ps : state.players) {
        out.insert(ps.hand.begin(), ps.hand.end());
        out.insert(ps.main_deck.begin(), ps.main_deck.end());
    }
    for (const auto& bf : state.battlefields) out.insert(bf.facedown.begin(), bf.facedown.end());
    return out;
}

/// A known unordered group of deck slots: (end, rank, span).
using GroupKey = std::tuple<int, int, int>;
GroupKey groupKey(const CardKnowledge& k) {
    return {static_cast<int>(k.deck_position), k.deck_rank, k.deck_rank_span};
}

/// Copy-limit bookkeeping for one deck (CR 103.3.b / .d, 825.3.a).
struct CopyCounts {
    std::map<std::string, int> by_name;
    int signatures = 0;

    bool room(const CardDef& d) const {
        if (d.super_type == SuperType::Signature && signatures >= kSignatureCards) return false;
        if (d.unlimited_copies) return true;
        const int cap = d.keywords.has(Keyword::Unique) ? 1 : kCopiesPerName;
        auto it = by_name.find(d.name);
        return (it == by_name.end() ? 0 : it->second) < cap;
    }
    void add(const CardDef& d) {
        if (d.super_type == SuperType::Signature) ++signatures;
        if (!d.unlimited_copies) ++by_name[d.name];
    }
};

}  // namespace

void becomeCard(GameObject& obj, const CardDef& def) {
    GameObject fresh;
    fresh.id = obj.id;
    fresh.owner = obj.owner;
    fresh.controller = obj.controller;
    fresh.zone = obj.zone.get();
    fresh.location = obj.location;
    fresh.is_hidden = obj.is_hidden;
    fresh.hidden_at = obj.hidden_at;
    fresh.hidden_on_turn = obj.hidden_on_turn;
    // As GameEngine::instantiateCard.
    fresh.card_def_id = def.id;
    fresh.name = def.name;
    fresh.card_type = def.card_type;
    fresh.super_type = def.super_type;
    fresh.tags = def.tags;
    fresh.domains = def.domains;
    fresh.base_might = def.might;
    fresh.current_might = def.might;
    fresh.keywords = def.keywords;
    fresh.assault_value = def.assault_value;
    fresh.shield_value = def.shield_value;
    fresh.deflect_value = def.deflect_value;
    fresh.might_bonus = def.might_bonus;
    obj = fresh;   // keeps reporting to the table it is in (tracked_field.h)
}

WorldSampler::WorldSampler(const CardDB& db) : db_(db), builder_(db) {
    std::vector<const CardDef*> defs;
    for (const auto& [id, d] : db.all()) defs.push_back(&d);
    std::sort(defs.begin(), defs.end(),
              [](const CardDef* a, const CardDef* b) { return a->id < b->id; });

    auto fillHidden = [&](LegendPool& pool) {
        for (CardDefId id : pool.cards)
            if (db_.get(id).keywords.has(Keyword::Hidden)) pool.hidden.push_back(id);
    };
    for (const CardDef* d : defs) {
        if (!d->banned && !d->isTokenCard() && isMainDeckType(*d)) any_.cards.push_back(d->id);
    }
    std::vector<CardDefId> all_runes;
    for (const CardDef* d : defs) {
        if (!d->banned && !d->isTokenCard() && d->card_type == CardType::Rune)
            all_runes.push_back(d->id);
    }
    if (!all_runes.empty()) any_.runes.push_back(all_runes);
    fillHidden(any_);

    for (const CardDef* d : defs) {
        if (d->card_type != CardType::Legend) continue;
        const RandomDeckBuilder::LegendCards* cards = builder_.legendCards(d->id);
        if (!cards) continue;
        LegendPool pool;
        pool.found = true;
        pool.cards = cards->main;
        pool.cards.insert(pool.cards.end(), cards->signatures.begin(), cards->signatures.end());
        std::sort(pool.cards.begin(), pool.cards.end());
        pool.runes = cards->runes;
        fillHidden(pool);
        pools_.emplace(d->id, std::move(pool));
    }
}

const CardDef* WorldSampler::def(CardDefId id) const {
    auto it = db_.all().find(id);
    return it == db_.all().end() ? nullptr : &it->second;
}

const WorldSampler::LegendPool& WorldSampler::poolFor(CardDefId legend) const {
    auto it = pools_.find(legend);
    return it == pools_.end() ? any_ : it->second;
}

WorldDraw WorldSampler::sample(GameState& world, PlayerId observer,
                               std::mt19937_64& rng) const {
    WorldDraw out;
    const PlayerId opp = opponent(observer);

    // Everything is read from the true position before anything is written.
    const knowledge::GameKnowledgeView view(world, observer);
    const std::set<GameObjectId> listed = listedHidden(world);
    const std::vector<Slot> own_slots = hiddenSlots(world, observer, listed);
    const std::vector<Slot> opp_slots = hiddenSlots(world, opp, listed);

    auto pick = [&](size_t n) {
        std::uniform_int_distribution<size_t> d(0, n - 1);
        return d(rng);
    };
    // Refill `ids` (in their order) with `cards`, shuffled. Sorting first makes
    // the result a function of the multiset, not of which id held what.
    auto refill = [&](const std::vector<GameObjectId>& ids, std::vector<CardDefId> cards) {
        std::sort(cards.begin(), cards.end());
        std::shuffle(cards.begin(), cards.end(), rng);
        for (size_t i = 0; i < ids.size(); ++i) {
            const CardDef* d = def(cards[i]);
            if (d) becomeCard(world.getObject(ids[i]), *d);
        }
        out.shuffled += static_cast<int>(ids.size());
    };
    auto refillGroups = [&](const std::map<GroupKey, std::vector<GameObjectId>>& groups) {
        for (const auto& [key, ids] : groups) {
            std::vector<CardDefId> cards;
            for (auto id : ids) cards.push_back(world.getObject(id).card_def_id);
            refill(ids, std::move(cards));
        }
    };

    // ── The observer's own hidden cards ──
    {
        std::vector<GameObjectId> free_ids;
        std::vector<CardDefId> free_cards;
        std::map<GroupKey, std::vector<GameObjectId>> groups;
        for (const Slot& s : own_slots) {
            const CardKnowledge k = view.about(s.id);
            if (s.place == Place::Deck && k.deck_position != DeckPosition::Unknown) {
                if (k.deck_rank_span == 1) ++out.kept;
                else groups[groupKey(k)].push_back(s.id);
                continue;
            }
            // A card of theirs out of every zone list is known to them if it
            // came from their hand (a mulligan's set-aside card) or they are
            // the one looking at it; one of their deck cards that the other
            // player is looking at is not.
            const bool held_unknown = s.place == Place::Held && !k.identity_known &&
                                      world.getObject(s.id).zone.get() != ZoneType::Hand;
            if (s.place == Place::Deck || held_unknown) {
                free_ids.push_back(s.id);
                free_cards.push_back(world.getObject(s.id).card_def_id);
                continue;
            }
            ++out.kept;   // own hand, own facedown, a card held by its own look
        }
        refill(free_ids, std::move(free_cards));
        refillGroups(groups);

        const auto& runes = world.player(observer).rune_deck;
        std::vector<CardDefId> rune_cards;
        for (auto id : runes) rune_cards.push_back(world.getObject(id).card_def_id);
        refill(runes, std::move(rune_cards));
    }

    // ── The opponent's ──
    {
        const auto& opp_state = world.player(opp);
        CardDefId legend = kInvalidId;
        if (opp_state.legend_zone != kInvalidId && world.objectExists(opp_state.legend_zone))
            legend = world.getObject(opp_state.legend_zone).card_def_id;
        const LegendPool& pool = poolFor(legend);

        // Copy limits over every card of theirs the observer can identify.
        CopyCounts counts;
        {
            std::vector<GameObjectId> ids;
            for (const auto& [id, obj] : world.objects)
                if (obj.owner == opp && !obj.is_token) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            for (auto id : ids) {
                const CardKnowledge k = view.about(id);
                if (!k.identity_known) continue;
                const CardDef* d = def(k.card_def_id);
                if (d && isMainDeckType(*d)) counts.add(*d);
            }
        }

        // Classify. `open` are the slots to fill; `floating` the cards known
        // by identity whose slot is not known (deck_only: known to be in the
        // Main Deck).
        struct Floating { bool deck_only; CardDefId card; GameObjectId was; };
        std::vector<Slot> open;
        std::vector<Floating> floating;
        std::map<GroupKey, std::vector<GameObjectId>> groups;
        for (const Slot& s : opp_slots) {
            const CardKnowledge k = view.about(s.id);
            if (!k.identity_known) { open.push_back(s); continue; }
            switch (s.place) {
                case Place::Held:
                    ++out.kept;
                    break;
                case Place::Hand:
                case Place::Facedown:
                    if (k.zone_confirmed) { ++out.kept; break; }
                    floating.push_back({false, k.card_def_id, s.id});
                    open.push_back(s);
                    break;
                case Place::Deck:
                    if (k.deck_position != DeckPosition::Unknown) {
                        if (k.deck_rank_span == 1) ++out.kept;
                        else groups[groupKey(k)].push_back(s.id);
                        break;
                    }
                    floating.push_back({k.zone_confirmed, k.card_def_id, s.id});
                    open.push_back(s);
                    break;
                case Place::Public:
                    break;
            }
        }
        refillGroups(groups);

        std::vector<bool> filled(open.size(), false);
        // The observer's facts about the floating cards follow them to their
        // new slots (they still say what the observer knows of that card).
        auto& facts = world.knowledge.facts[playerIndex(observer)];
        std::vector<std::pair<GameObjectId, HiddenCardFact>> moved_facts;
        // Deck-only cards first (they have fewer places to go), then by card.
        std::stable_sort(floating.begin(), floating.end(),
                         [](const Floating& a, const Floating& b) {
                             if (a.deck_only != b.deck_only) return a.deck_only;
                             return a.card < b.card;
                         });
        std::vector<HiddenCardFact> fact_of(floating.size());
        for (size_t i = 0; i < floating.size(); ++i) {
            auto it = facts.find(floating[i].was);
            if (it != facts.end()) { fact_of[i] = it->second; facts.erase(it); }
        }
        for (size_t i = 0; i < floating.size(); ++i) {
            const Floating& f = floating[i];
            const CardDef* d = def(f.card);
            if (!d) continue;
            auto allowed = [&](size_t j) {
                if (filled[j]) return false;
                const Place p = open[j].place;
                if (f.deck_only) return p == Place::Deck;
                if (p == Place::Held) return false;
                if (p == Place::Facedown) return d->keywords.has(Keyword::Hidden);
                return true;
            };
            std::vector<size_t> choices;
            for (size_t j = 0; j < open.size(); ++j) if (allowed(j)) choices.push_back(j);
            if (choices.empty()) {
                for (size_t j = 0; j < open.size(); ++j) if (!filled[j]) choices.push_back(j);
            }
            if (choices.empty()) break;
            const size_t j = choices[pick(choices.size())];
            filled[j] = true;
            becomeCard(world.getObject(open[j].id), *d);
            HiddenCardFact moved = fact_of[i];
            moved.zone = open[j].place == Place::Hand ? ZoneType::Hand
                       : open[j].place == Place::Facedown ? ZoneType::FacedownZone
                       : ZoneType::MainDeck;
            moved_facts.emplace_back(open[j].id, moved);
            ++out.placed;
        }
        for (const auto& [id, f] : moved_facts) facts[id] = f;

        // Everything else from the deck model: facedown slots first (they need
        // a [Hidden] card), then the rest in slot order.
        auto draw = [&](const std::vector<CardDefId>& from) -> const CardDef* {
            const std::vector<CardDefId>& list = from.empty() ? any_.cards : from;
            if (list.empty()) return nullptr;
            for (int t = 0; t < kRejectionTries; ++t) {
                const CardDef* d = def(list[pick(list.size())]);
                if (d && counts.room(*d)) return d;
            }
            std::vector<CardDefId> roomy;
            for (CardDefId id : list) {
                const CardDef* d = def(id);
                if (d && counts.room(*d)) roomy.push_back(id);
            }
            return def((roomy.empty() ? list : roomy)[pick(roomy.empty() ? list.size() : roomy.size())]);
        };
        for (int pass = 0; pass < 2; ++pass) {
            for (size_t j = 0; j < open.size(); ++j) {
                if (filled[j]) continue;
                const bool facedown = open[j].place == Place::Facedown;
                if ((pass == 0) != facedown) continue;
                const CardDef* d = draw(facedown ? pool.hidden : pool.cards);
                if (!d) continue;
                counts.add(*d);
                becomeCard(world.getObject(open[j].id), *d);
                filled[j] = true;
                ++out.drawn;
            }
        }

        // Their rune deck: a Domain of the legend's, then a printing.
        const auto& rune_lists = pool.runes.empty() ? any_.runes : pool.runes;
        if (!rune_lists.empty()) {
            for (auto id : opp_state.rune_deck) {
                const auto& list = rune_lists[pick(rune_lists.size())];
                const CardDef* d = def(list[pick(list.size())]);
                if (!d) continue;
                becomeCard(world.getObject(id), *d);
                ++out.runes_drawn;
            }
        }

        // Their unchosen battlefields are not known (nothing reads the pool
        // once both are chosen; search does not run before that). What they
        // were shown privately is not known either.
        world.player(opp).battlefield_pool.clear();
        world.player(opp).observed_cards.clear();
    }
    return out;
}

std::vector<std::string> WorldSampler::check(const GameState& truth, const GameState& world,
                                             PlayerId observer) const {
    std::vector<std::string> out;
    auto say = [&](const std::string& s) { out.push_back(s); };
    const PlayerId opp = opponent(observer);
    const knowledge::GameKnowledgeView view(truth, observer);
    auto defName = [&](CardDefId id) {
        const CardDef* d = def(id);
        return d ? d->name : std::string("#") + toString(id);
    };

    // Same objects, same places.
    std::vector<GameObjectId> ids;
    for (const auto& [id, obj] : truth.objects) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    if (world.objects.size() != truth.objects.size()) say("object count differs");
    for (auto id : ids) {
        if (!world.objectExists(id)) { say("object " + toString(id) + " missing"); continue; }
        const GameObject& a = truth.getObject(id);
        const GameObject& b = world.getObject(id);
        if (a.zone.get() != b.zone.get() || a.location.get() != b.location.get() ||
            a.owner != b.owner || a.controller != b.controller || a.is_hidden != b.is_hidden)
            say("object " + toString(id) + " moved or changed hands");
    }
    for (int p = 0; p < 2; ++p) {
        const auto& a = truth.players[p];
        const auto& b = world.players[p];
        if (a.hand != b.hand || a.main_deck != b.main_deck || a.rune_deck != b.rune_deck ||
            a.trash != b.trash || a.banishment != b.banishment)
            say("a zone list of player " + std::to_string(p + 1) + " differs");
    }
    for (size_t i = 0; i < truth.battlefields.size() && i < world.battlefields.size(); ++i)
        if (truth.battlefields[i].facedown != world.battlefields[i].facedown)
            say("a facedown pile differs");

    // What the observer knows is unchanged.
    const std::set<GameObjectId> listed = listedHidden(truth);
    std::set<GameObjectId> hidden_ids;
    for (PlayerId p : {PlayerId::Player1, PlayerId::Player2})
        for (const Slot& s : hiddenSlots(truth, p, listed)) hidden_ids.insert(s.id);
    for (const auto& ps : truth.players)
        hidden_ids.insert(ps.rune_deck.begin(), ps.rune_deck.end());
    for (auto id : ids) {
        if (!world.objectExists(id)) continue;
        const CardDefId was = truth.getObject(id).card_def_id;
        const CardDefId now = world.getObject(id).card_def_id;
        if (!hidden_ids.count(id)) {
            if (was != now) say("public card " + toString(id) + " (" + defName(was) + ") changed");
            continue;
        }
        const CardKnowledge k = view.about(id);
        const GameObject& obj = truth.getObject(id);
        const ZoneType z = obj.zone;
        const bool exact =
            k.identity_known && k.zone_confirmed &&
            (z != ZoneType::MainDeck || !listed.count(id) ||
             (k.deck_position != DeckPosition::Unknown && k.deck_rank_span == 1));
        const bool own_visible = obj.owner == observer &&
                                 (z == ZoneType::Hand || z == ZoneType::FacedownZone);
        if ((exact || own_visible) && was != now)
            say("known card " + toString(id) + " (" + defName(was) + ") changed to " +
                defName(now));
    }

    // Multisets the observer knows.
    auto multiset = [&](const GameState& s, const std::vector<GameObjectId>& v) {
        std::vector<CardDefId> m;
        for (auto id : v) m.push_back(s.getObject(id).card_def_id);
        std::sort(m.begin(), m.end());
        return m;
    };
    {
        const auto& ps = truth.player(observer);
        std::vector<GameObjectId> deck = ps.main_deck;
        for (const Slot& s : hiddenSlots(truth, observer, listed))
            if (s.place == Place::Held && !view.about(s.id).identity_known &&
                truth.getObject(s.id).zone.get() != ZoneType::Hand)
                deck.push_back(s.id);
        if (multiset(truth, deck) != multiset(world, deck))
            say("the observer's Main Deck holds other cards");
        if (multiset(truth, ps.rune_deck) != multiset(world, ps.rune_deck))
            say("the observer's rune deck holds other cards");
    }
    for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
        std::map<GroupKey, std::vector<GameObjectId>> groups;
        std::vector<CardDefId> known, present;
        for (const Slot& s : hiddenSlots(truth, p, listed)) {
            const CardKnowledge k = view.about(s.id);
            present.push_back(world.getObject(s.id).card_def_id);
            if (s.place == Place::Deck && k.deck_position != DeckPosition::Unknown &&
                k.deck_rank_span > 1)
                groups[groupKey(k)].push_back(s.id);
            if (p != observer && k.identity_known) known.push_back(k.card_def_id);
        }
        for (const auto& [key, g] : groups)
            if (multiset(truth, g) != multiset(world, g))
                say(std::string("a known group of player ") + toString(p) + "'s deck holds other cards");
        std::sort(known.begin(), known.end());
        std::sort(present.begin(), present.end());
        if (!std::includes(present.begin(), present.end(), known.begin(), known.end()))
            say(std::string("a card of player ") + toString(p) +
                " the observer knows by identity is missing from their hidden cards");
    }

    // The opponent's drawn cards: legal under the deck model.
    {
        const auto& ops = world.player(opp);
        CardDefId legend = kInvalidId;
        if (ops.legend_zone != kInvalidId && world.objectExists(ops.legend_zone))
            legend = world.getObject(ops.legend_zone).card_def_id;
        const LegendPool& pool = poolFor(legend);
        std::set<CardDefId> legal(pool.cards.begin(), pool.cards.end());
        for (const auto& list : pool.runes) legal.insert(list.begin(), list.end());
        // A drawn card is one whose identity the world changed and the
        // observer did not know: counted after every card that stayed.
        CopyCounts counts;
        std::vector<const CardDef*> drawn;
        for (auto id : ids) {
            const GameObject& b = world.getObject(id);
            if (b.owner != opp || b.is_token) continue;
            const CardDef* d = def(b.card_def_id);
            if (!d) continue;
            const bool changed = truth.getObject(id).card_def_id != b.card_def_id;
            if (changed && !legal.count(d->id))
                say("drawn card " + d->name + " is outside the opponent's legal pool");
            if (changed && b.zone.get() == ZoneType::FacedownZone &&
                !d->keywords.has(Keyword::Hidden))
                say("drawn facedown card " + d->name + " has no [Hidden]");
            if (!isMainDeckType(*d)) continue;
            if (changed) drawn.push_back(d);
            else counts.add(*d);
        }
        for (const CardDef* d : drawn) {
            if (!counts.room(*d)) say("drawn card " + d->name + " breaks a copy limit");
            counts.add(*d);
        }
    }

    for (const auto& line : knowledge::check(world)) say("knowledge: " + line);
    return out;
}

}  // namespace riftbound
