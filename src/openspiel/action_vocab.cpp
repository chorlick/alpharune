#include "action_vocab.h"
#include <array>

#include "engine/knowledge_tracker.h"
#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"

#include <algorithm>
#include <numeric>
#include <optional>
#include <variant>

namespace riftbound::openspiel {

namespace {

/// Key of an object in a card-keyed range: its card id, or 0 for an object
/// with none (a token without a card class), a missing object, or an id
/// outside the vocabulary.
int cardKey(CardDefId def) {
    if (def == kInvalidId) return 0;
    const uint32_t n = def.raw();
    return n >= 1 && n <= static_cast<uint32_t>(kNumCardDefIds) ? static_cast<int>(n) : 0;
}

/// The object, or nullptr (one lookup: a slot is worked out for every applied
/// decision and for every option of every node a search visits).
const GameObject* lookup(const GameState& state, GameObjectId obj) {
    if (obj == kInvalidId) return nullptr;
    const auto it = state.objects.find(obj);
    return it == state.objects.end() ? nullptr : &it->second;
}

/// The card key of an object as `decider` sees it: 0 unless the decider
/// knows which card it is (batch 5b item L2: a prompt that offered the
/// opponent's facedown or hand card used to put its card in the slot).
int cardKey(const GameState& state, GameObjectId obj, PlayerId decider) {
    const GameObject* o = lookup(state, obj);
    if (!o || !knowledge::identityVisible(state, obj, decider)) return 0;
    return cardKey(o->card_def_id);
}

/// 0 for none, a base or a slot past the range; battlefield slot + 1 otherwise.
int placeKey(BattlefieldId bf) {
    if (bf == kInvalidId) return 0;
    const uint32_t v = bf.raw();
    return v < static_cast<uint32_t>(kPlaceKeys - 1) ? static_cast<int>(v) + 1 : 0;
}

int placeKey(const std::optional<LocationId>& loc) {
    if (!loc.has_value()) return 0;
    if (const auto* bf = std::get_if<BattlefieldLocation>(&*loc)) return placeKey(bf->id);
    return 0;
}

int base(ActionVerb v) { return verbOffset(v); }

/// Slot of a mulligan within its range: 0 keep, 1..8 one hand position,
/// then the pairs (i < j) in order, and a last slot for anything else. A
/// position is in the hand sorted by card (`sortedHand`, the order the
/// observation writes the hand in), not in the order the cards were dealt.
int mulliganKey(const Intent& intent, const GameState& state) {
    constexpr int kOther = 1 + kMulliganHand + kMulliganHand * (kMulliganHand - 1) / 2;
    const auto& cards = intent.cards_to_mulligan;
    if (cards.empty()) return 0;
    if (intent.player != PlayerId::Player1 && intent.player != PlayerId::Player2) return kOther;
    const std::vector<GameObjectId> hand = knowledge::sortedHand(state, intent.player);
    auto position = [&](GameObjectId card) {
        const auto it = std::find(hand.begin(), hand.end(), card);
        const auto at = it - hand.begin();
        return it == hand.end() || at >= kMulliganHand ? -1 : static_cast<int>(at);
    };
    if (cards.size() == 1) {
        const int p = position(cards[0]);
        return p < 0 ? kOther : 1 + p;
    }
    if (cards.size() == 2) {
        int a = position(cards[0]), b = position(cards[1]);
        if (a < 0 || b < 0 || a == b) return kOther;
        if (a > b) std::swap(a, b);
        // Pairs (i, j), i < j, row by row.
        const int before = a * kMulliganHand - a * (a + 1) / 2;
        return 1 + kMulliganHand + before + (b - a - 1);
    }
    return kOther;
}

/// FNV-1a over 64-bit words (each word's eight bytes, low byte first).
struct Fingerprint {
    static constexpr uint64_t kPrime = 1099511628211ull;
    /// kPrime to the power k (mod 2^64), k = 0..8.
    static constexpr std::array<uint64_t, 9> kPrimePow = [] {
        std::array<uint64_t, 9> p{};
        p[0] = 1;
        for (int k = 1; k < 9; ++k) p[k] = p[k - 1] * kPrime;
        return p;
    }();
    uint64_t h = 14695981039346656037ull;
    void add(uint64_t v) {
        // A zero byte only multiplies (h ^= 0), so the zero bytes above a
        // word's highest non-zero byte — six or seven of them for an id or a
        // small number — are one multiply by kPrime to their count. The value
        // is the byte-by-byte one (1.9% of a game was this loop).
        int n = 8;
        while (n > 0 && ((v >> ((n - 1) * 8)) & 0xFFu) == 0) --n;
        for (int i = 0; i < n; ++i) {
            h ^= (v >> (i * 8)) & 0xFFu;
            h *= kPrime;
        }
        h *= kPrimePow[static_cast<std::size_t>(8 - n)];
    }
    void addLocation(const std::optional<LocationId>& loc) {
        if (!loc.has_value()) { add(0); return; }
        if (auto* base = std::get_if<BaseLocation>(&*loc)) {
            add(1); add(static_cast<uint64_t>(base->player));
        } else if (auto* bf = std::get_if<BattlefieldLocation>(&*loc)) {
            add(2); add(static_cast<uint64_t>(bf->id.raw()));
        }
    }
    void addIds(const std::vector<GameObjectId>& ids) {
        add(ids.size());
        for (auto id : ids) add(static_cast<uint64_t>(id.raw()));
    }
};

uint64_t fingerprintOf(const Intent& intent) {
    // Every field Intent::operator== compares, in the same order.
    Fingerprint fp;
    fp.add(static_cast<uint64_t>(intent.type));
    fp.add(static_cast<uint64_t>(intent.player));
    fp.add(static_cast<uint64_t>(intent.card.raw()));
    fp.addLocation(intent.play_location);
    fp.addIds(intent.units_to_move);
    fp.addLocation(intent.move_destination);
    fp.add(static_cast<uint64_t>(intent.ability.raw()));
    fp.add(static_cast<uint64_t>(intent.ability_source.raw()));
    fp.add(static_cast<uint64_t>(intent.ability_index));
    fp.add(static_cast<uint64_t>(intent.play_source));
    fp.addIds(intent.targets);
    fp.add(intent.damage_assignments.size());
    for (const auto& da : intent.damage_assignments) {
        fp.add(static_cast<uint64_t>(da.target_unit.raw()));
        fp.add(static_cast<uint64_t>(da.damage));
    }
    fp.addIds(intent.cards_to_mulligan);
    fp.add(static_cast<uint64_t>(intent.chosen_battlefield.raw()));
    fp.add(static_cast<uint64_t>(intent.battlefield_pool_index.raw()));
    fp.add(intent.choose_first ? 1 : 0);
    fp.add(intent.sideboard_swaps.size());
    for (const auto& [out, in] : intent.sideboard_swaps) {
        fp.add(static_cast<uint64_t>(out.raw()));
        fp.add(static_cast<uint64_t>(in.raw()));
    }
    fp.addIds(intent.chosen_objects);
    fp.add(intent.chosen_value.has_value() ? 1 : 0);
    fp.add(static_cast<uint64_t>(intent.chosen_value.value_or(0)));
    fp.add(static_cast<uint64_t>(intent.chosen_card_def.raw()));
    fp.add(intent.pay_location_cost ? 1 : 0);
    fp.add(intent.use_alt_play_cost ? 1 : 0);
    // "The source's own ability" is fed as 0, the number the field held
    // before ids were types (plans/05 item 5.5).
    fp.add(intent.granted_ability_def == kInvalidId
               ? uint64_t{0}
               : static_cast<uint64_t>(intent.granted_ability_def.raw()));
    return fp.h;
}

// Three-way comparisons for compareIntents.
template <class T>
int cmp(const T& a, const T& b) { return a < b ? -1 : (b < a ? 1 : 0); }

int cmpIds(const std::vector<GameObjectId>& a, const std::vector<GameObjectId>& b) {
    if (int c = cmp(a.size(), b.size())) return c;
    for (size_t i = 0; i < a.size(); ++i)
        if (int c = cmp(a[i].raw(), b[i].raw())) return c;
    return 0;
}

/// (kind, number): none < a base (by player) < a battlefield (by slot).
std::pair<int, uint32_t> locationKey(const std::optional<LocationId>& loc) {
    if (!loc.has_value()) return {0, 0};
    if (const auto* b = std::get_if<BaseLocation>(&*loc))
        return {1, static_cast<uint32_t>(b->player)};
    return {2, std::get<BattlefieldLocation>(*loc).id.raw()};
}

// ─── What the decider sees of an intent (twin order) ──────────────────────

/// 1 the viewer, 2 another player, 0 nobody.
int64_t relation(PlayerId p, PlayerId viewer) {
    if (p == PlayerId::None) return 0;
    return p == viewer ? 1 : 2;
}

int64_t placeCode(const std::optional<LocationId>& loc, PlayerId viewer) {
    if (!loc.has_value()) return 0;
    if (const auto* b = std::get_if<BaseLocation>(&*loc)) return 1 + relation(b->player, viewer);
    return 4 + static_cast<int64_t>(std::get<BattlefieldLocation>(*loc).id.raw());
}

/// Index of `id` in `v`, or -1.
int64_t indexIn(const std::vector<GameObjectId>& v, GameObjectId id) {
    const auto it = std::find(v.begin(), v.end(), id);
    return it == v.end() ? -1 : static_cast<int64_t>(it - v.begin());
}

/// What `viewer` sees of one object, appended to `what`; the public order it
/// has in its zone (the order cards entered a hand, a trash, a banishment)
/// appended to `order`. Never the object's id, and its card only if the
/// viewer knows it.
void seeObject(std::vector<int64_t>& what, std::vector<int64_t>& order,
               const GameState& state, GameObjectId id, PlayerId viewer) {
    if (id == kInvalidId) { what.push_back(-1); return; }
    const GameObject* o = lookup(state, id);
    if (!o) { what.push_back(-2); return; }
    const bool visible = knowledge::identityVisible(state, id, viewer);
    what.push_back(relation(o->controller, viewer));
    what.push_back(relation(o->owner, viewer));
    what.push_back(static_cast<int64_t>(o->zone.get()));
    what.push_back(placeCode(o->location.get(), viewer));
    int64_t facedown_at = -1;
    if (o->zone.get() == ZoneType::FacedownZone) {
        for (const auto& bf : state.battlefields)
            if (indexIn(bf.facedown, id) >= 0) facedown_at = static_cast<int64_t>(bf.id.raw());
    }
    what.push_back(facedown_at);
    what.push_back(visible ? static_cast<int64_t>(cardKey(o->card_def_id)) : -1);
    // A card in a Main Deck the viewer was shown: the slot it knows it at (two
    // copies among the top cards looked at are not the same choice; batch 5d
    // N1). Built only for such a card, which an option rarely names.
    if (visible && o->zone.get() == ZoneType::MainDeck &&
        (viewer == PlayerId::Player1 || viewer == PlayerId::Player2)) {
        const CardKnowledge k = knowledge::GameKnowledgeView(state, viewer).about(id);
        what.push_back(static_cast<int64_t>(k.deck_position));
        what.push_back(k.deck_rank);
        what.push_back(k.deck_rank_span);
    }
    if (visible && (o->location.has_value() || o->zone.get() == ZoneType::LegendZone ||
                    o->zone.get() == ZoneType::ChampionZone)) {
        what.push_back(o->current_might);
        what.push_back(o->damage_marked);
        what.push_back(o->is_exhausted);
        what.push_back(o->is_stunned);
        what.push_back(o->buff_count);
        what.push_back(o->temp_buff_count);
        what.push_back(o->temp_might_bonus);
        what.push_back(static_cast<int64_t>(o->combat_designation));
        what.push_back(o->empowered);
        what.push_back(static_cast<int64_t>((o->keywords | o->aura_keywords).bits));
        std::vector<int64_t> gear;
        for (auto g : o->attachments)
            if (const GameObject* go = lookup(state, g)) gear.push_back(cardKey(go->card_def_id));
        std::sort(gear.begin(), gear.end());
        what.push_back(static_cast<int64_t>(gear.size()));
        what.insert(what.end(), gear.begin(), gear.end());
        const GameObject* bearer = o->attached_to ? lookup(state, *o->attached_to) : nullptr;
        what.push_back(bearer ? cardKey(bearer->card_def_id) : -1);
    }
    // What the chain says about it: two copies of one card are told apart by
    // which is the source, the subject or a target of which pending item
    // (both players see the chain).
    for (size_t k = 0; k < state.chain.items.size(); ++k) {
        const auto& ci = state.chain.items[k];
        if (ci.source == id) { what.push_back(1000 + static_cast<int64_t>(k)); }
        if (ci.triggering_subject == id) { what.push_back(2000 + static_cast<int64_t>(k)); }
        if (std::find(ci.targets.begin(), ci.targets.end(), id) != ci.targets.end())
            what.push_back(3000 + static_cast<int64_t>(k));
    }
    what.push_back(-3);   // end of the chain part
    // Public order within the zone.
    int64_t at = -1;
    if (o->owner == PlayerId::Player1 || o->owner == PlayerId::Player2) {
        const auto& ps = state.player(o->owner);
        switch (o->zone.get()) {
            case ZoneType::Hand:       at = indexIn(ps.hand, id); break;
            case ZoneType::Trash:      at = indexIn(ps.trash, id); break;
            case ZoneType::Banishment: at = indexIn(ps.banishment, id); break;
            default: break;
        }
    }
    order.push_back(at);
}

void seeIds(std::vector<int64_t>& what, std::vector<int64_t>& order,
            const GameState& state, const std::vector<GameObjectId>& ids, PlayerId viewer) {
    what.push_back(static_cast<int64_t>(ids.size()));
    for (auto id : ids) seeObject(what, order, state, id, viewer);
}

void seeLocation(std::vector<int64_t>& what, const std::optional<LocationId>& loc,
                 PlayerId viewer) {
    what.push_back(placeCode(loc, viewer));
}

struct SeenIntent {
    std::vector<int64_t> what;    // what the decider sees
    std::vector<int64_t> order;   // public order of the objects named
};

SeenIntent seeIntent(const Intent& i, const GameState& state) {
    SeenIntent out;
    auto& w = out.what;
    auto& o = out.order;
    const PlayerId viewer = i.player;
    w.reserve(32);
    w.push_back(static_cast<int64_t>(i.type));
    seeObject(w, o, state, i.card, viewer);
    seeObject(w, o, state, i.ability_source, viewer);
    // A prompt's answer of several objects is a set (see actionSlot).
    if (i.type == IntentType::MakeChoice && i.chosen_objects.size() > 1) {
        std::vector<std::pair<std::vector<int64_t>, std::vector<int64_t>>> objs;
        for (auto id : i.chosen_objects) {
            std::vector<int64_t> cw, co;
            seeObject(cw, co, state, id, viewer);
            objs.emplace_back(std::move(cw), std::move(co));
        }
        std::sort(objs.begin(), objs.end());
        w.push_back(static_cast<int64_t>(objs.size()));
        for (auto& [cw, co] : objs) {
            w.insert(w.end(), cw.begin(), cw.end());
            o.insert(o.end(), co.begin(), co.end());
        }
    } else {
        seeIds(w, o, state, i.chosen_objects, viewer);
    }
    w.push_back(i.chosen_value.has_value());
    w.push_back(i.chosen_value.value_or(0));
    seeIds(w, o, state, i.targets, viewer);
    seeIds(w, o, state, i.units_to_move, viewer);
    // The cards a mulligan sets aside are a set: in what the decider sees,
    // in an order of what it sees (the intent lists them in hand order).
    {
        std::vector<std::pair<std::vector<int64_t>, std::vector<int64_t>>> cards;
        for (auto id : i.cards_to_mulligan) {
            std::vector<int64_t> cw, co;
            seeObject(cw, co, state, id, viewer);
            cards.emplace_back(std::move(cw), std::move(co));
        }
        std::sort(cards.begin(), cards.end());
        w.push_back(static_cast<int64_t>(cards.size()));
        for (auto& [cw, co] : cards) {
            w.insert(w.end(), cw.begin(), cw.end());
            o.insert(o.end(), co.begin(), co.end());
        }
    }
    // Combat damage is dealt all at once (CR 467.2.c.1.a): an assignment is
    // a set of (unit, damage), whatever order the generator listed it in.
    {
        std::vector<std::pair<std::vector<int64_t>, std::vector<int64_t>>> hits;
        for (const auto& da : i.damage_assignments) {
            std::vector<int64_t> hw, ho;
            seeObject(hw, ho, state, da.target_unit, viewer);
            hw.push_back(da.damage);
            hits.emplace_back(std::move(hw), std::move(ho));
        }
        std::sort(hits.begin(), hits.end());
        w.push_back(static_cast<int64_t>(hits.size()));
        for (auto& [hw, ho] : hits) {
            w.insert(w.end(), hw.begin(), hw.end());
            o.insert(o.end(), ho.begin(), ho.end());
        }
    }
    seeLocation(w, i.play_location, viewer);
    seeLocation(w, i.move_destination, viewer);
    w.push_back(static_cast<int64_t>(i.play_source));
    w.push_back(static_cast<int64_t>(i.ability.raw()));
    w.push_back(i.ability_index);
    w.push_back(static_cast<int64_t>(i.chosen_battlefield.raw()));
    w.push_back(static_cast<int64_t>(i.battlefield_pool_index.raw()));
    w.push_back(i.choose_first);
    w.push_back(static_cast<int64_t>(i.sideboard_swaps.size()));
    for (const auto& [out_card, in_card] : i.sideboard_swaps) {
        seeObject(w, o, state, out_card, viewer);
        seeObject(w, o, state, in_card, viewer);
    }
    w.push_back(static_cast<int64_t>(i.chosen_card_def.raw()));
    w.push_back(i.use_alt_play_cost);
    w.push_back(i.pay_location_cost);
    w.push_back(static_cast<int64_t>(i.granted_ability_def.raw()));
    return out;
}

/// The twin order: what the decider sees, then the public order of the
/// objects named, then (only between intents the decider sees as the same,
/// whose ids are random) the intents themselves.
int compareSeen(const SeenIntent& sa, const SeenIntent& sb, const Intent& a, const Intent& b) {
    if (sa.what != sb.what) return sa.what < sb.what ? -1 : 1;
    if (sa.order != sb.order) return sa.order < sb.order ? -1 : 1;
    return compareIntents(a, b);
}

} // namespace

std::vector<int64_t> visibleObjectDescription(const GameState& state, GameObjectId object,
                                              PlayerId viewer) {
    std::vector<int64_t> what, order;
    seeObject(what, order, state, object, viewer);
    return what;
}

void sortObjectsAsSeen(const GameState& state, std::vector<GameObjectId>& objects,
                       PlayerId viewer) {
    if (objects.size() < 2) return;
    std::vector<std::pair<std::vector<int64_t>, GameObjectId>> seen;
    seen.reserve(objects.size());
    for (auto id : objects) seen.push_back({visibleObjectDescription(state, id, viewer), id});
    std::sort(seen.begin(), seen.end());
    for (size_t i = 0; i < objects.size(); ++i) objects[i] = seen[i].second;
}

std::vector<int64_t> visibleDescription(const Intent& intent, const GameState& state) {
    return seeIntent(intent, state).what;
}

int compareIntentsAsSeen(const Intent& a, const Intent& b, const GameState& state) {
    return compareSeen(seeIntent(a, state), seeIntent(b, state), a, b);
}

int actionSlot(const Intent& intent, const GameState& state) {
    using T = IntentType;
    using V = ActionVerb;
    switch (intent.type) {
        case T::EndTurn:       return base(V::EndTurn);
        case T::PassPriority:  return base(V::PassPriority);
        case T::PassFocus:     return base(V::PassFocus);
        case T::Concede:       return base(V::Concede);

        case T::PlayFirstDecision:
            return base(V::PlayFirstDecision) + (intent.choose_first ? 1 : 0);

        // Which cards are set aside, by their place in the hand (the order
        // the player's own hand has in the state features).
        case T::MulliganDecision:
            return base(V::MulliganDecision) + mulliganKey(intent, state);

        // The setup choice: the position in the player's battlefield list.
        case T::ChooseBattlefield: {
            const uint32_t pick = intent.battlefield_pool_index.raw();
            return base(V::ChooseBattlefield)
                   + (intent.battlefield_pool_index.valid() && pick < 8
                          ? static_cast<int>(pick) : 0);
        }

        // How many units are given damage. Which units and how much is in the
        // twin number.
        case T::AssignCombatDamage: {
            int hit = 0;
            for (const auto& da : intent.damage_assignments)
                if (da.damage > 0) ++hit;
            return base(V::AssignCombatDamage) + std::min(hit, 15);
        }

        // Which unit, and where to. (Units that join the move are asked for
        // one by one afterwards.)
        case T::StandardMove: {
            const int unit = intent.units_to_move.empty()
                ? 0 : cardKey(state, intent.units_to_move.front(), intent.player);
            return base(V::StandardMove)
                   + unit * kPlaceKeys + placeKey(intent.move_destination);
        }

        // Which card, where it lands, and how it is played: the Chosen
        // Champion's alternative-cost play is not its normal play, and a play
        // to a battlefield is not the play to the base.
        case T::PlayCard:
        case T::PlayActionCard:
        case T::PlayReaction: {
            PlayVariant variant = PlayVariant::Normal;
            if (intent.use_alt_play_cost) {
                variant = PlayVariant::AlternativeCost;
            } else if (intent.pay_location_cost) {
                variant = PlayVariant::LocationCost;
            } else if (intent.play_source != Intent::PlaySource::Hand &&
                       intent.play_source != Intent::PlaySource::ChampionZone) {
                variant = PlayVariant::FromElsewhere;
            }
            return base(V::Play)
                   + (cardKey(state, intent.card, intent.player) * kPlaceKeys
                      + placeKey(intent.play_location))
                         * static_cast<int>(PlayVariant::Count)
                   + static_cast<int>(variant);
        }

        case T::HideCard:
            return base(V::HideCard)
                   + cardKey(state, intent.card, intent.player) * kPlaceKeys
                   + placeKey(intent.chosen_battlefield);

        // The ability index is never replaced by anything else: an ability
        // granted by another card and an ability aimed at a target named in
        // the intent (Equip) have ranges of their own.
        case T::ActivateAbility:
        case T::ActivateReactionAbility:
        case T::ActivateActionAbility: {
            if (const int granted = cardKey(intent.granted_ability_def))
                return base(V::ActivateGranted) + granted;
            const int source = cardKey(state, intent.ability_source, intent.player);
            if (!intent.targets.empty())
                return base(V::ActivateAtTarget) + source;
            const int idx = std::clamp(intent.ability_index, 0, kMaxAbilitiesPerCard - 1);
            return base(V::ActivateAbility) + source * kMaxAbilitiesPerCard + idx;
        }

        // A prompt's answer. GameEngine::askAgent refuses a prompt whose
        // options do not differ by object, value or named card, so one of
        // the three is always there. An object first (a prompt that offers
        // objects next to "none of them" gives the latter a value), by whose
        // it is: in a mirror match "my X" and "their X" are different answers.
        case T::MakeChoice: {
            if (!intent.chosen_objects.empty()) {
                // Several objects in one answer are a set ("choose 2 runes to
                // keep"), listed in whatever order the prompt built them (it
                // may follow ids): keyed by the one the decider sees first.
                GameObjectId lead = intent.chosen_objects.front();
                if (intent.chosen_objects.size() > 1) {
                    auto best = visibleObjectDescription(state, lead, intent.player);
                    for (auto id : intent.chosen_objects) {
                        auto d = visibleObjectDescription(state, id, intent.player);
                        if (d < best) { best = std::move(d); lead = id; }
                    }
                }
                const GameObject* first = lookup(state, lead);
                const bool own = !first || first->controller == intent.player;
                return base(own ? V::ChooseOwnObject : V::ChooseOpposingObject)
                       + cardKey(state, lead, intent.player);
            }
            if (intent.chosen_card_def != kInvalidId)
                return base(V::ChooseNamedCard) + cardKey(intent.chosen_card_def);
            const int v = intent.chosen_value.value_or(-1);
            return base(V::ChooseValue)
                   + (v >= 0 && v < kNumIntChoices ? v : kNumIntChoices);
        }

        case T::PlaceOptionalTrigger:
            return base(V::PlaceOptionalTrigger) + cardKey(state, intent.ability_source, intent.player);
        case T::DeclineOptionalTrigger:
            return base(V::DeclineOptionalTrigger) + cardKey(state, intent.ability_source, intent.player);
        case T::PayTriggeredCost:
            return base(V::PayTriggeredCost) + cardKey(state, intent.ability_source, intent.player);
        case T::DeclineTriggeredCost:
            return base(V::DeclineTriggeredCost) + cardKey(state, intent.ability_source, intent.player);

        case T::SideboardSwap:
            return base(V::SideboardSwap)
                   + (intent.sideboard_swaps.empty()
                          ? 0 : cardKey(state, intent.sideboard_swaps.front().first, intent.player));
    }
    return base(V::Reserved);
}

int compareIntents(const Intent& a, const Intent& b) {
    // Every field Intent::operator== compares. Zero exactly when a == b. The
    // fields that most often tell two options of one decision apart come
    // first; any fixed order would do.
    if (int c = cmp(static_cast<int>(a.type), static_cast<int>(b.type))) return c;
    if (int c = cmp(a.card.raw(), b.card.raw())) return c;
    if (int c = cmp(a.ability_source.raw(), b.ability_source.raw())) return c;
    if (int c = cmpIds(a.chosen_objects, b.chosen_objects)) return c;
    if (int c = cmp(a.chosen_value.has_value(), b.chosen_value.has_value())) return c;
    if (int c = cmp(a.chosen_value.value_or(0), b.chosen_value.value_or(0))) return c;
    if (int c = cmpIds(a.targets, b.targets)) return c;
    if (int c = cmpIds(a.units_to_move, b.units_to_move)) return c;
    if (int c = cmpIds(a.cards_to_mulligan, b.cards_to_mulligan)) return c;
    if (int c = cmp(a.damage_assignments.size(), b.damage_assignments.size())) return c;
    for (size_t i = 0; i < a.damage_assignments.size(); ++i) {
        if (int c = cmp(a.damage_assignments[i].target_unit.raw(),
                        b.damage_assignments[i].target_unit.raw())) return c;
        if (int c = cmp(a.damage_assignments[i].damage, b.damage_assignments[i].damage)) return c;
    }
    if (int c = cmp(locationKey(a.play_location), locationKey(b.play_location))) return c;
    if (int c = cmp(locationKey(a.move_destination), locationKey(b.move_destination))) return c;
    if (int c = cmp(static_cast<int>(a.player), static_cast<int>(b.player))) return c;
    if (int c = cmp(static_cast<int>(a.play_source), static_cast<int>(b.play_source))) return c;
    if (int c = cmp(a.ability.raw(), b.ability.raw())) return c;
    if (int c = cmp(a.ability_index, b.ability_index)) return c;
    if (int c = cmp(a.chosen_battlefield.raw(), b.chosen_battlefield.raw())) return c;
    if (int c = cmp(a.battlefield_pool_index.raw(), b.battlefield_pool_index.raw())) return c;
    if (int c = cmp(a.choose_first, b.choose_first)) return c;
    if (int c = cmp(a.sideboard_swaps.size(), b.sideboard_swaps.size())) return c;
    for (size_t i = 0; i < a.sideboard_swaps.size(); ++i) {
        if (int c = cmp(a.sideboard_swaps[i].first.raw(), b.sideboard_swaps[i].first.raw())) return c;
        if (int c = cmp(a.sideboard_swaps[i].second.raw(), b.sideboard_swaps[i].second.raw())) return c;
    }
    if (int c = cmp(a.chosen_card_def.raw(), b.chosen_card_def.raw())) return c;
    if (int c = cmp(a.use_alt_play_cost, b.use_alt_play_cost)) return c;
    if (int c = cmp(a.pay_location_cost, b.pay_location_cost)) return c;
    return cmp(a.granted_ability_def.raw(), b.granted_ability_def.raw());
}

std::vector<int> actionIds(const std::vector<Intent>& legal_actions,
                           const GameState& state) {
    const size_t n = legal_actions.size();
    std::vector<int> ids(n);
    for (size_t i = 0; i < n; ++i) ids[i] = actionSlot(legal_actions[i], state);
    if (n < 2) return ids;

    // Most decisions have no two intents on one slot: look before sorting.
    std::vector<uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(),
              [&](uint32_t a, uint32_t b) { return ids[a] < ids[b]; });
    bool shared = false;
    for (size_t k = 1; k < n && !shared; ++k) shared = ids[order[k]] == ids[order[k - 1]];
    if (!shared) return ids;

    // Within a slot: the order of what the decider sees (compareIntentsAsSeen),
    // twin 0, 1, 2, ...; equal intents (the engine may list an option twice)
    // share one.
    std::vector<SeenIntent> seen(n);
    for (size_t k = 0; k < n; ++k) {
        const bool alone = (k == 0 || ids[order[k]] != ids[order[k - 1]]) &&
                           (k + 1 == n || ids[order[k]] != ids[order[k + 1]]);
        if (!alone) seen[order[k]] = seeIntent(legal_actions[order[k]], state);
    }
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        if (ids[a] != ids[b]) return ids[a] < ids[b];
        return compareSeen(seen[a], seen[b], legal_actions[a], legal_actions[b]) < 0;
    });
    int twin = 0;
    std::vector<int> out(n);
    out[order[0]] = ids[order[0]];
    for (size_t k = 1; k < n; ++k) {
        const uint32_t prev = order[k - 1], cur = order[k];
        if (ids[cur] != ids[prev]) {
            twin = 0;
        } else if (compareIntents(legal_actions[prev], legal_actions[cur]) != 0) {
            ++twin;
        }
        out[cur] = twin < kTwinPlanes ? makeActionId(ids[cur], twin) : kNoActionId;
    }
    return out;
}

namespace {

/// Intent types that can share a slot: the timing variants of a play and of
/// an activation are one verb, every other type has verbs of its own.
int verbFamily(IntentType t) {
    switch (t) {
        case IntentType::PlayCard:
        case IntentType::PlayActionCard:
        case IntentType::PlayReaction:
            return -1;
        case IntentType::ActivateAbility:
        case IntentType::ActivateReactionAbility:
        case IntentType::ActivateActionAbility:
            return -2;
        default:
            return static_cast<int>(t);
    }
}

} // namespace

int actionIdOf(const Intent& intent,
               const std::vector<Intent>& legal_actions,
               const GameState& state) {
    // The twin number: how many different legal intents of this slot come
    // before it in the twin order (compareIntentsAsSeen). Only intents of the
    // same verb family can share the slot.
    const int slot = actionSlot(intent, state);
    const int family = verbFamily(intent.type);
    std::optional<SeenIntent> mine;
    // The earlier intents counted, to count one the engine listed twice
    // once. A few fields tell most intents apart; the full comparison runs
    // only when those agree.
    auto quick = [](const Intent& i) {
        uint64_t h = i.card.raw();
        h = h * 1000003u + i.ability_source.raw();
        h = h * 1000003u + (i.chosen_objects.empty() ? 0u : i.chosen_objects.back().raw());
        h = h * 1000003u + (i.targets.empty() ? 0u : i.targets.back().raw());
        h = h * 1000003u + (i.units_to_move.empty() ? 0u : i.units_to_move.front().raw());
        h = h * 1000003u + static_cast<uint32_t>(i.chosen_value.value_or(-7));
        return h;
    };
    constexpr int kInline = 32;
    const Intent* counted[kInline];
    uint64_t counted_key[kInline];
    int twin = 0;
    for (const Intent& other : legal_actions) {
        if (verbFamily(other.type) != family) continue;
        if (other == intent) continue;
        if (actionSlot(other, state) != slot) continue;
        if (!mine) mine = seeIntent(intent, state);
        if (compareSeen(seeIntent(other, state), *mine, other, intent) >= 0) continue;
        const uint64_t key = quick(other);
        bool listed_before = false;
        for (int k = 0; k < std::min(twin, kInline) && !listed_before; ++k)
            listed_before = counted_key[k] == key && *counted[k] == other;
        if (!listed_before && twin >= kInline) {
            // Past the inline list (rare): look the slow way.
            for (const Intent* p = legal_actions.data(); p != &other && !listed_before; ++p)
                listed_before = *p == other;
        }
        if (listed_before) continue;
        if (twin < kInline) {
            counted[twin] = &other;
            counted_key[twin] = key;
        }
        ++twin;
    }
    return twin < kTwinPlanes ? makeActionId(slot, twin) : kNoActionId;
}

const Intent* decodeAction(int action_id,
                            const std::vector<Intent>& legal_actions,
                            const GameState& state) {
    if (action_id < 0) return nullptr;
    const std::vector<int> ids = actionIds(legal_actions, state);
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] == action_id) return &legal_actions[i];
    }
    return nullptr;
}

uint64_t infoSetIdFor(const GameState& state, const std::vector<Intent>& legal,
                      PlayerId decider, PlayerId player, const CardDB& card_db) {
    auto seat = [](PlayerId p) {
        // OpenSpiel's numbering; kTerminalPlayerId (-4) for nobody.
        return p == PlayerId::Player1 ? 0 : p == PlayerId::Player2 ? 1 : -4;
    };
    // Search asks for this key on every node visit: reuse one buffer per
    // thread instead of allocating the kStateFeatureDim-float vector each time.
    thread_local std::vector<float> feats;
    ml::extractStateFeatures(state, player, card_db, feats);
    // The features describe the board, not the question being asked. Two
    // decisions in a row can leave the board untouched (a card's mode prompt
    // followed by its target prompt, two "you may" prompts) yet offer
    // different actions, and a search tree or regret table keyed on the
    // board alone would file both under one node. So the key also carries
    // who is deciding and, for the decider, the actions on offer — which
    // the decider sees, and which makes "same key => same action set" hold
    // by construction. Other players get only the public "who decides".
    std::vector<int64_t> offered;
    if (player == decider && decider != PlayerId::None) {
        for (int id : actionIds(legal, state)) offered.push_back(id);
        std::sort(offered.begin(), offered.end());
        offered.erase(std::unique(offered.begin(), offered.end()), offered.end());
    }
    return ml::mixDecisionIntoInfoSetId(ml::computeInfoSetId(feats, seat(player)),
                                        seat(decider), offered);
}

int64_t historyToken(const Intent& intent, const GameState& state) {
    const int64_t fp_bits = static_cast<int64_t>(
        fingerprintOf(intent) >> (kHistorySlotBits + 1));  // 43 bits, keeps the token positive
    return (fp_bits << kHistorySlotBits) | static_cast<int64_t>(actionSlot(intent, state));
}

int findHistoryToken(int64_t token,
                     const std::vector<Intent>& legal_actions,
                     const GameState& state) {
    for (size_t i = 0; i < legal_actions.size(); ++i) {
        if (historyToken(legal_actions[i], state) == token) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace riftbound::openspiel
