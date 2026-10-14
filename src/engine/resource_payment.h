#pragma once
/// @file resource_payment.h
/// The one rule for paying Energy and Power outside a card's base cost.
///
/// Accelerate, Repeat, Equip, Hide, Flow, activated abilities, optional
/// additional costs and "pay [N] to ..." inside an effect all pay the same way
/// (CR 164.2): floating resources first, then runes in the payer's base —
/// exhaust a ready rune for Energy, recycle a rune (ready or exhausted) for
/// Power, and one rune may do both. GameEngine::payAdditionalCost,
/// EffectExecutor::payAnyPower and the card helpers (payEnergy, payOnePower)
/// all come here, so the affordability checks and the payments cannot drift.
///
/// State-only: usable with a bare EffectExecutor (no engine).

#include "core/board_state.h"
#include "core/events.h"
#include "core/game_state.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace riftbound {

/// What paying for (or with an ability of) `obj` counts as.
inline SpendFor spendForObject(const GameObject& obj) {
    if (obj.isUnit()) return SpendFor::Unit;
    if (obj.isSpell()) return SpendFor::Spell;
    if (obj.isGear()) return SpendFor::Gear;
    return SpendFor::Other;
}

/// A showdown (or the combat that follows it) is under way somewhere.
inline bool showdownUnderway(const GameState& state) {
    for (const auto& bf : state.battlefields)
        if (bf.showdown_in_progress || bf.combat_in_progress) return true;
    return false;
}

namespace resource {

/// A set of Domains a Power payment accepts, one bit per Domain. A single
/// Domain for "[R]", every bit for [A], and a multi-domain card's own Domains
/// for "its base cost".
using DomainMask = unsigned;
inline constexpr DomainMask kAnyDomain = (1u << static_cast<int>(Domain::Count)) - 1u;
/// `domain` == Domain::Count is [A].
inline DomainMask maskOf(Domain domain) {
    return domain == Domain::Count ? kAnyDomain : (1u << static_cast<int>(domain));
}
inline DomainMask maskOf(const std::vector<Domain>& domains) {
    DomainMask m = 0;
    for (auto d : domains)
        if (d != Domain::Count) m |= 1u << static_cast<int>(d);
    return m == 0 ? kAnyDomain : m;
}

inline bool runePays(const GameObject& rune, DomainMask mask) {
    if (mask == kAnyDomain) return true;
    for (auto d : rune.domains)
        if (d != Domain::Count && (mask & (1u << static_cast<int>(d)))) return true;
    return false;
}

/// `player`'s runes in their base, lowest id first (the object table is
/// unordered; a payment must not depend on its iteration order).
inline std::vector<GameObjectId> runesInBase(const GameState& state, PlayerId player) {
    std::vector<GameObjectId> out;
    const LocationId base{BaseLocation{player}};
    for (const auto& [id, obj] : state.objects.atLocation(base))
        if (obj.isRune() && obj.controller == player) out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

/// Energy `player` could pay for `f` right now: floating plus ready runes.
inline int payableEnergy(const GameState& state, PlayerId player,
                         SpendFor f = SpendFor::Other) {
    const auto& pool = state.player(player).rune_pool;
    int n = pool.energy + pool.restrictedEnergy(f, showdownUnderway(state));
    // (A count: the base is read as it stands, no list of ids is built.)
    for (const auto& [id, obj] : state.objects.atLocation(LocationId{BaseLocation{player}}))
        if (obj.isRune() && obj.controller == player && !obj.is_exhausted) ++n;
    return n;
}

/// Power of the Domains in `mask` `player` could pay for `f`.
inline int payablePowerIn(const GameState& state, PlayerId player, DomainMask mask,
                          SpendFor f = SpendFor::Other) {
    const auto& pool = state.player(player).rune_pool;
    int n = pool.universal_power + pool.restrictedPower(f);
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
        if (mask & (1u << di)) n += pool.power[di];
    for (const auto& [id, obj] : state.objects.atLocation(LocationId{BaseLocation{player}}))
        if (obj.isRune() && obj.controller == player && runePays(obj, mask)) ++n;
    return n;
}
/// Power of `domain` (Domain::Count = any) `player` could pay for `f`.
inline int payablePower(const GameState& state, PlayerId player, Domain domain,
                        SpendFor f = SpendFor::Other) {
    return payablePowerIn(state, player, maskOf(domain), f);
}

/// Whether `player` can pay [energy] plus `power` Power, each of a Domain in
/// `mask` (the Powers need not share one). A rune can be exhausted for Energy
/// and then recycled for Power, so the two requirements are independent.
inline bool canPayIn(const GameState& state, PlayerId player, int energy, int power,
                     DomainMask mask, SpendFor f = SpendFor::Other) {
    return payableEnergy(state, player, f) >= std::max(0, energy) &&
           payablePowerIn(state, player, mask, f) >= std::max(0, power);
}
inline bool canPay(const GameState& state, PlayerId player, int energy, int power,
                   Domain domain, SpendFor f = SpendFor::Other) {
    return canPayIn(state, player, energy, power, maskOf(domain), f);
}

/// Asks the payer which rune pays the next point of a payment (testing phase
/// D9: the payer chooses, as for a card's base cost). `options` holds one rune
/// of each kind on offer (runeKinds), in the order the engine would take them
/// itself; the answer must be one of them. Without a chooser (or with one
/// kind) the first is taken.
struct RuneChooser {
    std::function<GameObjectId(PlayerId player, const std::vector<GameObjectId>& options,
                               bool for_power)> pick;
    explicit operator bool() const { return static_cast<bool>(pick); }
};

/// One rune of each kind among `runes`, the first of each in `runes`' order.
/// Runes of one kind are interchangeable for a payment: the same card, the
/// same Domains, both ready or both exhausted.
inline std::vector<GameObjectId> runeKinds(const GameState& state,
                                           const std::vector<GameObjectId>& runes) {
    std::vector<GameObjectId> out;
    for (auto id : runes) {
        const auto& r = state.getObject(id);
        bool seen = false;
        for (auto o : out) {
            const auto& q = state.getObject(o);
            if (q.card_def_id == r.card_def_id && q.is_exhausted == r.is_exhausted &&
                q.domains.get() == r.domains.get()) { seen = true; break; }
        }
        if (!seen) out.push_back(id);
    }
    return out;
}

/// Power taken from a pool counts as spent like a recycled rune's (Sivir,
/// Mercenary: "If you've spent at least [A][A] this turn"; testing phase I3).
inline void notePowerSpent(PlayerState& ps, int n) {
    if (n > 0) ps.power_spent_this_turn += n;
}

/// The rune `chooser` names among `candidates` (ordered as the engine would
/// take them), or the first.
inline GameObjectId chooseRune(const GameState& state, PlayerId player,
                               const std::vector<GameObjectId>& candidates, bool for_power,
                               const RuneChooser* chooser) {
    const auto kinds = runeKinds(state, candidates);
    if (kinds.empty()) return kInvalidId;
    if (!chooser || !*chooser || kinds.size() < 2) return kinds.front();
    const GameObjectId picked = chooser->pick(player, kinds, for_power);
    return std::find(kinds.begin(), kinds.end(), picked) != kinds.end() ? picked : kinds.front();
}

/// Recycle `rid` from `player`'s base for Power (CR 164.2): the rune as it
/// was (last_known), the move, its announcement, then the new object (CR
/// 124.1; a rune carries no attachments).
inline void recycleRuneForPower(GameState& state, EventBus& events, PlayerId player,
                                GameObjectId rid) {
    auto& ps = state.player(player);
    auto& rune = state.getObject(rid);
    snapshotAsItLeaves(rune);
    rune.location = std::nullopt;
    rune.zone = ZoneType::RuneDeck;
    ps.rune_deck.insert(ps.rune_deck.begin(), rid);
    ps.power_spent_this_turn++;
    events.emit(LeftBoardEvent{rid, player, CardType::Rune,
        LocationId{BaseLocation{player}}, ZoneType::RuneDeck, false});
    forgetBoardState(state.getObject(rid), nullptr);
}

/// Pay it: all or nothing. Restricted resources go first, then floating ones,
/// then runes — Energy from the ready runes that cannot pay the Power (so a
/// rune that can is still there to recycle), Power from exhausted runes before
/// ready ones. With a `chooser` the payer names each rune instead, among the
/// kinds that differ (RuneChooser); the order above is the order of the
/// options and the choice when nobody is asked.
inline bool payIn(GameState& state, EventBus& events, PlayerId player, int energy,
                  int power, DomainMask mask, SpendFor f = SpendFor::Other,
                  const RuneChooser* chooser = nullptr) {
    if (!canPayIn(state, player, energy, power, mask, f)) return false;
    auto& ps = state.player(player);
    int e_need = std::max(0, energy);
    int p_need = std::max(0, power);

    auto take = [](int& from, int& need) {
        int n = std::min(from, std::max(0, need));
        from -= n;
        need -= n;
        return n;
    };
    ps.rune_pool.takeRestrictedEnergy(f, showdownUnderway(state), e_need);
    take(ps.rune_pool.energy, e_need);
    {
        const int before = p_need;
        ps.rune_pool.takeRestrictedPower(f, p_need);
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
            if (mask & (1u << di)) take(ps.rune_pool.power[di], p_need);
        take(ps.rune_pool.universal_power, p_need);
        notePowerSpent(ps, before - p_need);
    }

    int paid_e = 0;
    while (e_need > 0) {
        // Ready runes, those that cannot pay the Power first, lowest id first.
        auto runes = runesInBase(state, player);
        std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
            return runePays(state.getObject(a), mask) < runePays(state.getObject(b), mask);
        });
        runes.erase(std::remove_if(runes.begin(), runes.end(), [&](GameObjectId id) {
            return state.getObject(id).is_exhausted;
        }), runes.end());
        const GameObjectId rid = chooseRune(state, player, runes, false, chooser);
        if (rid == kInvalidId) break;
        state.getObject(rid).is_exhausted = true;
        events.emit(ObjectStateChangedEvent{rid, "exhausted"});
        --e_need;
        ++paid_e;
    }

    int paid_p = 0;
    while (p_need > 0) {
        // Runes that pay the Power, exhausted ones first, lowest id first.
        auto runes = runesInBase(state, player);
        runes.erase(std::remove_if(runes.begin(), runes.end(), [&](GameObjectId id) {
            return !runePays(state.getObject(id), mask);
        }), runes.end());
        std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
            return state.getObject(a).is_exhausted > state.getObject(b).is_exhausted;
        });
        const GameObjectId rid = chooseRune(state, player, runes, true, chooser);
        if (rid == kInvalidId) break;
        recycleRuneForPower(state, events, player, rid);
        --p_need;
        ++paid_p;
    }
    RIFT_TRACE(events, "PAY_ADD: " + std::string(toString(player)) + " pays [" +
                    std::to_string(energy) + "] + " + std::to_string(power) + " power" +
                    (mask == kAnyDomain ? " (any)" : "") +
                    ": exhausted " + std::to_string(paid_e) + ", recycled " +
                    std::to_string(paid_p));
    return e_need <= 0 && p_need <= 0;
}
inline bool pay(GameState& state, EventBus& events, PlayerId player, int energy,
                int power, Domain domain, SpendFor f = SpendFor::Other,
                const RuneChooser* chooser = nullptr) {
    return payIn(state, events, player, energy, power, maskOf(domain), f, chooser);
}

/// Pay `power` Power of any Domain AHEAD of a cost that is still to be paid
/// and whose own Power must be of a Domain in `cost_domains`: the Deflect of
/// a unit a spell chooses before its cost (CR 809; the spell's choices come
/// before its payment, CR 355 -> 356, and this engine pays a choice's Deflect
/// as the choice is made). The cost's affordability was judged with the two
/// paid together (GameEngine::canAffordWith: a ready rune gives its Energy and
/// is then recycled; a rune of another Domain pays the "[A] more"). So that
/// what is left can still pay the cost, this payment
///   * takes what the cost cannot use first — floating Power and runes of
///     other Domains — and only then what it can; and
///   * exhausts a ready rune for its Energy before recycling it: the Energy
///     stays in the pool for the cost that follows (CR 164.2.b: a rune may be
///     exhausted for Energy and recycled for Power).
/// `payIn` does neither (it recycles the first rune, ready or not), which is
/// right when nothing is paid after it and left a spell unpayable here
/// (plans/09 item 9.VV1). All or nothing.
/// With a `chooser` the payer names each point (D9): a rune of any kind, or —
/// as the option kInvalidId — the floating Power the cost could use, each
/// offered only if `still_payable` holds after it (tried on a copy of the
/// game, nothing announced) with the rest of this payment made the engine's
/// way. `spend_for` lets Power restricted to that purpose pay too (D50: a
/// spell's Deflect is a cost of playing that spell, CR 356.2.a.2).
inline bool payAnyPowerAheadOfCost(GameState& state, EventBus& events, PlayerId player,
                                   int power, DomainMask cost_domains,
                                   SpendFor spend_for = SpendFor::Other,
                                   const RuneChooser* chooser = nullptr,
                                   const std::function<bool()>* still_payable = nullptr) {
    if (power <= 0) return true;
    if (payablePowerIn(state, player, kAnyDomain, spend_for) < power) return false;
    int need = power;
    int exhausted_first = 0, recycled = 0;
    auto floatingOutside = [&](PlayerState& ps) {
        int n = 0;
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
            if (!(cost_domains & (1u << di))) n += ps.rune_pool.power[di];
        return n;
    };
    auto takeFloating = [&](PlayerState& ps, bool usable_by_cost) {
        // One point: restricted Power for this purpose, then a Domain's, then [A].
        int one = 1;
        if (usable_by_cost) {
            ps.rune_pool.takeRestrictedPower(spend_for, one);
            for (int di = 0; one > 0 && di < static_cast<int>(Domain::Count); ++di)
                if ((cost_domains & (1u << di)) && ps.rune_pool.power[di] > 0) {
                    --ps.rune_pool.power[di];
                    one = 0;
                }
            if (one > 0 && ps.rune_pool.universal_power > 0) {
                --ps.rune_pool.universal_power;
                one = 0;
            }
        } else {
            for (int di = 0; one > 0 && di < static_cast<int>(Domain::Count); ++di)
                if (!(cost_domains & (1u << di)) && ps.rune_pool.power[di] > 0) {
                    --ps.rune_pool.power[di];
                    one = 0;
                }
        }
        if (one == 0) notePowerSpent(ps, 1);
        return one == 0;
    };
    auto floatingUsable = [&](PlayerState& ps) {
        int n = ps.rune_pool.universal_power + ps.rune_pool.restrictedPower(spend_for);
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
            if (cost_domains & (1u << di)) n += ps.rune_pool.power[di];
        return n;
    };
    auto recycle = [&](GameObjectId rid, int* exhausted_counter) {
        auto& ps = state.player(player);
        auto& rune = state.getObject(rid);
        if (!rune.is_exhausted) {
            rune.is_exhausted = true;
            events.emit(ObjectStateChangedEvent{rid, "exhausted"});
            ++ps.rune_pool.energy;
            if (exhausted_counter) ++*exhausted_counter;
        }
        recycleRuneForPower(state, events, player, rid);
    };
    // The engine's order of the rune options: runes the cost's Power cannot
    // use (exhausted first), then those it can (exhausted first).
    auto runeOptions = [&](bool usable_by_cost) {
        std::vector<GameObjectId> out;
        const auto runes = runesInBase(state, player);
        for (int pass = 0; pass < 2; ++pass)
            for (auto rid : runes) {
                const auto& rune = state.getObject(rid);
                if (runePays(rune, cost_domains) != usable_by_cost) continue;
                if (rune.is_exhausted == (pass == 0)) out.push_back(rid);
            }
        return out;
    };
    // One point, the engine's way: what the cost cannot use first.
    auto payOneDefault = [&](int* exhausted_counter, int* recycled_counter) {
        auto& ps = state.player(player);
        if (floatingOutside(ps) > 0) return takeFloating(ps, false);
        if (auto outside = runeOptions(false); !outside.empty()) {
            recycle(outside.front(), exhausted_counter);
            if (recycled_counter) ++*recycled_counter;
            return true;
        }
        if (floatingUsable(ps) > 0) return takeFloating(ps, true);
        if (auto inside = runeOptions(true); !inside.empty()) {
            recycle(inside.front(), exhausted_counter);
            if (recycled_counter) ++*recycled_counter;
            return true;
        }
        return false;
    };
    const bool asking = chooser && *chooser && still_payable && *still_payable;
    while (need > 0) {
        auto& ps = state.player(player);
        // Floating Power the cost cannot use is always the first (it is lost
        // at the end of the phase and leaves the cost as it was).
        if (floatingOutside(ps) > 0) {
            takeFloating(ps, false);
            --need;
            continue;
        }
        if (!asking) {
            if (!payOneDefault(&exhausted_first, &recycled)) break;
            --need;
            continue;
        }
        // The options, in the engine's order: runes of other Domains, the
        // floating Power the cost could use (kInvalidId), runes of the cost's
        // Domains — one of each kind, each kept only if the cost is still
        // payable after it and the rest of this payment.
        std::vector<GameObjectId> candidates = runeKinds(state, runeOptions(false));
        if (floatingUsable(ps) > 0) candidates.push_back(kInvalidId);
        for (auto id : runeKinds(state, runeOptions(true))) candidates.push_back(id);
        std::vector<GameObjectId> viable;
        for (auto cand : candidates) {
            // Tried on a copy: the game is moved aside (references into it
            // stay good) and moved back after.
            GameState original = std::move(state);
            state = original;
            const auto quiet = events.beginQuiet();
            bool ok = true;
            if (cand == kInvalidId) ok = takeFloating(state.player(player), true);
            else recycle(cand, nullptr);
            for (int k = 1; ok && k < need; ++k) ok = payOneDefault(nullptr, nullptr);
            ok = ok && (*still_payable)();
            events.endQuiet(quiet);
            state = std::move(original);
            if (ok) viable.push_back(cand);
        }
        if (viable.empty()) {
            // (Judged payable as a whole before this payment: the engine's
            // order is then the one that keeps it so.)
            if (!payOneDefault(&exhausted_first, &recycled)) break;
            --need;
            continue;
        }
        GameObjectId pick = viable.front();
        if (viable.size() > 1) {
            const GameObjectId named = chooser->pick(player, viable, true);
            if (std::find(viable.begin(), viable.end(), named) != viable.end()) pick = named;
        }
        if (pick == kInvalidId) {
            takeFloating(state.player(player), true);
        } else {
            recycle(pick, &exhausted_first);
            ++recycled;
        }
        --need;
    }
    // The line payIn writes, so a payment that came out the same reads the
    // same: "exhausted" counts the runes exhausted for their Energy first.
    RIFT_TRACE(events, "PAY_ADD: " + std::string(toString(player)) + " pays [0] + " +
                    std::to_string(power) + " power (any): exhausted " +
                    std::to_string(exhausted_first) + ", recycled " +
                    std::to_string(recycled));
    return need <= 0;
}

}  // namespace resource
}  // namespace riftbound
