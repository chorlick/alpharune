#include "action_vocab.h"

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

int cardKey(const GameState& state, GameObjectId obj) {
    const GameObject* o = lookup(state, obj);
    return o ? cardKey(o->card_def_id) : 0;
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
/// then the pairs (i < j) in order, and a last slot for anything else.
int mulliganKey(const Intent& intent, const GameState& state) {
    constexpr int kOther = 1 + kMulliganHand + kMulliganHand * (kMulliganHand - 1) / 2;
    const auto& cards = intent.cards_to_mulligan;
    if (cards.empty()) return 0;
    if (intent.player != PlayerId::Player1 && intent.player != PlayerId::Player2) return kOther;
    const auto& hand = state.player(intent.player).hand;
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

/// FNV-1a over 64-bit words.
struct Fingerprint {
    uint64_t h = 14695981039346656037ull;
    void add(uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (i * 8)) & 0xFFu;
            h *= 1099511628211ull;
        }
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

} // namespace

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
                ? 0 : cardKey(state, intent.units_to_move.front());
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
                   + (cardKey(state, intent.card) * kPlaceKeys
                      + placeKey(intent.play_location))
                         * static_cast<int>(PlayVariant::Count)
                   + static_cast<int>(variant);
        }

        case T::HideCard:
            return base(V::HideCard)
                   + cardKey(state, intent.card) * kPlaceKeys
                   + placeKey(intent.chosen_battlefield);

        // The ability index is never replaced by anything else: an ability
        // granted by another card and an ability aimed at a target named in
        // the intent (Equip) have ranges of their own.
        case T::ActivateAbility:
        case T::ActivateReactionAbility:
        case T::ActivateActionAbility: {
            if (const int granted = cardKey(intent.granted_ability_def))
                return base(V::ActivateGranted) + granted;
            const int source = cardKey(state, intent.ability_source);
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
                const GameObject* first = lookup(state, intent.chosen_objects.front());
                const bool own = !first || first->controller == intent.player;
                return base(own ? V::ChooseOwnObject : V::ChooseOpposingObject)
                       + (first ? cardKey(first->card_def_id) : 0);
            }
            if (intent.chosen_card_def != kInvalidId)
                return base(V::ChooseNamedCard) + cardKey(intent.chosen_card_def);
            const int v = intent.chosen_value.value_or(-1);
            return base(V::ChooseValue)
                   + (v >= 0 && v < kNumIntChoices ? v : kNumIntChoices);
        }

        case T::PlaceOptionalTrigger:
            return base(V::PlaceOptionalTrigger) + cardKey(state, intent.ability_source);
        case T::DeclineOptionalTrigger:
            return base(V::DeclineOptionalTrigger) + cardKey(state, intent.ability_source);
        case T::PayTriggeredCost:
            return base(V::PayTriggeredCost) + cardKey(state, intent.ability_source);
        case T::DeclineTriggeredCost:
            return base(V::DeclineTriggeredCost) + cardKey(state, intent.ability_source);

        case T::SideboardSwap:
            return base(V::SideboardSwap)
                   + (intent.sideboard_swaps.empty()
                          ? 0 : cardKey(state, intent.sideboard_swaps.front().first));
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

    // Within a slot: canonical order, twin 0, 1, 2, ...; equal intents (the
    // engine may list an option twice) share one.
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        if (ids[a] != ids[b]) return ids[a] < ids[b];
        return compareIntents(legal_actions[a], legal_actions[b]) < 0;
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
    // before it in the canonical order. Only intents of the same verb family
    // can share the slot.
    const int slot = actionSlot(intent, state);
    const int family = verbFamily(intent.type);
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
        if (compareIntents(other, intent) >= 0) continue;
        if (actionSlot(other, state) != slot) continue;
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
