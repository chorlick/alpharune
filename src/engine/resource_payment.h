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

/// Pay it: all or nothing. Restricted resources go first, then floating ones,
/// then runes — Energy from the ready runes that cannot pay the Power (so a
/// rune that can is still there to recycle), Power from exhausted runes before
/// ready ones.
inline bool payIn(GameState& state, EventBus& events, PlayerId player, int energy,
                  int power, DomainMask mask, SpendFor f = SpendFor::Other) {
    if (!canPayIn(state, player, energy, power, mask, f)) return false;
    auto& ps = state.player(player);
    int e_need = std::max(0, energy);
    int p_need = std::max(0, power);

    auto take = [](int& from, int& need) {
        int n = std::min(from, std::max(0, need));
        from -= n;
        need -= n;
    };
    ps.rune_pool.takeRestrictedEnergy(f, showdownUnderway(state), e_need);
    take(ps.rune_pool.energy, e_need);
    ps.rune_pool.takeRestrictedPower(f, p_need);
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
        if (mask & (1u << di)) take(ps.rune_pool.power[di], p_need);
    take(ps.rune_pool.universal_power, p_need);

    auto runes = runesInBase(state, player);
    std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
        return runePays(state.getObject(a), mask) < runePays(state.getObject(b), mask);
    });
    int paid_e = 0;
    for (auto rid : runes) {
        if (e_need <= 0) break;
        auto& rune = state.getObject(rid);
        if (rune.is_exhausted) continue;
        rune.is_exhausted = true;
        events.emit(ObjectStateChangedEvent{rid, "exhausted"});
        --e_need;
        ++paid_e;
    }

    std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
        return state.getObject(a).is_exhausted > state.getObject(b).is_exhausted;
    });
    int paid_p = 0;
    const LocationId base_loc{BaseLocation{player}};
    for (auto rid : runes) {
        if (p_need <= 0) break;
        auto& rune = state.getObject(rid);
        if (!runePays(rune, mask)) continue;
        // The same order as every other way off the board: the rune as it
        // was (last_known), the move, its announcement, and only then the
        // new object (CR 124.1; a rune carries no attachments).
        snapshotAsItLeaves(rune);
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        ps.power_spent_this_turn++;
        events.emit(LeftBoardEvent{rid, player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        forgetBoardState(state.getObject(rid), nullptr);
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
                int power, Domain domain, SpendFor f = SpendFor::Other) {
    return payIn(state, events, player, energy, power, maskOf(domain), f);
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
inline bool payAnyPowerAheadOfCost(GameState& state, EventBus& events, PlayerId player,
                                   int power, DomainMask cost_domains) {
    if (power <= 0) return true;
    if (payablePowerIn(state, player, kAnyDomain) < power) return false;
    auto& ps = state.player(player);
    int need = power;
    auto take = [&](int& from) {
        const int n = std::min(from, need);
        from -= n;
        need -= n;
    };
    // Floating Power the cost cannot use.
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
        if (!(cost_domains & (1u << di))) take(ps.rune_pool.power[di]);

    const LocationId base_loc{BaseLocation{player}};
    int exhausted_first = 0, recycled = 0;
    auto recycle = [&](GameObjectId rid) {
        auto& rune = state.getObject(rid);
        if (!rune.is_exhausted) {
            rune.is_exhausted = true;
            events.emit(ObjectStateChangedEvent{rid, "exhausted"});
            ++ps.rune_pool.energy;
            ++exhausted_first;
        }
        snapshotAsItLeaves(rune);
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        ps.power_spent_this_turn++;
        events.emit(LeftBoardEvent{rid, player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        forgetBoardState(state.getObject(rid), nullptr);
        --need;
        ++recycled;
    };
    // Runes the cost's Power cannot use, exhausted ones first; then floating
    // Power it can use; then the runes it can use.
    const auto runes = runesInBase(state, player);
    for (int pass = 0; pass < 2 && need > 0; ++pass)
        for (auto rid : runes) {
            if (need <= 0) break;
            const auto& rune = state.getObject(rid);
            if (!rune.location.has_value() || runePays(rune, cost_domains)) continue;
            if (rune.is_exhausted == (pass == 0)) recycle(rid);
        }
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
        if (cost_domains & (1u << di)) take(ps.rune_pool.power[di]);
    take(ps.rune_pool.universal_power);
    for (int pass = 0; pass < 2 && need > 0; ++pass)
        for (auto rid : runes) {
            if (need <= 0) break;
            const auto& rune = state.getObject(rid);
            if (!rune.location.has_value() || !runePays(rune, cost_domains)) continue;
            if (rune.is_exhausted == (pass == 0)) recycle(rid);
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
