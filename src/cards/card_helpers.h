#pragma once
/// @file card_helpers.h
/// Cross-cutting card helpers used across multiple card types. Type-specific
/// bases live in their type dirs (gear/equip_base.h, units/weaponmaster_base.h).
#include "engine/deck_ops.h"
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

// The engine-known Gold gear token's card_def_id.
constexpr CardDefId kGoldGearCardDefId = 326;

// ── Equip: pay one power (of `domain`, or any rune if nullopt), then attach ──
// Complements the energy+Domain `standardEquip` overload in gear/equip_base.h.
// One Power of `domain` (any domain if nullopt), the way CR 164.2 lets it be
// paid: floating Power of that domain, then universal Power, then recycling a
// matching rune — ready or exhausted (an exhausted one first, since a ready
// rune can still be exhausted for Energy).
inline bool runeMatchesDomain(const GameObject& rune, std::optional<Domain> domain) {
    if (!domain.has_value()) return true;
    for (auto d : rune.domains) if (d == *domain) return true;
    return false;
}
inline bool payableOnePower(const GameState& state, PlayerId player,
                            std::optional<Domain> domain) {
    const auto& pool = state.player(player).rune_pool;
    if (pool.universal_power > 0) return true;
    if (domain.has_value()) { if (pool.power[static_cast<int>(*domain)] > 0) return true; }
    else if (pool.totalPower() > 0) return true;
    const LocationId base{BaseLocation{player}};
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && obj.location.has_value() &&
            *obj.location == base && runeMatchesDomain(obj, domain)) return true;
    return false;
}
/// Recycle `rune` (in `player`'s base) to pay one Power, with the engine's
/// bookkeeping for a rune spent that way (EffectExecutor::payAnyPower): it
/// goes to the bottom of the Rune Deck ready, counts toward
/// `power_spent_this_turn` (Sivir, Mercenary) and leaves the board with an
/// event. Every card effect that recycles a rune for Power goes through here.
inline void recycleRuneForPower(CardContext& ctx, PlayerId player, GameObjectId rune) {
    auto& ps = ctx.state.player(player);
    auto& dr = ctx.state.getObject(rune);
    const LocationId was_at = dr.location.value_or(LocationId{BaseLocation{player}});
    dr.location = std::nullopt;
    dr.zone = ZoneType::RuneDeck;
    dr.is_exhausted = false;
    ps.rune_deck.insert(ps.rune_deck.begin(), rune);
    ps.power_spent_this_turn++;
    ctx.events.logTrace("  ADDL_COST: recycled " + dr.name + " for power");
    ctx.events.emit(LeftBoardEvent{rune, player, CardType::Rune, was_at,
                                   ZoneType::RuneDeck, false});
}
inline bool payOnePower(CardContext& ctx, PlayerId player,
                        std::optional<Domain> domain) {
    auto& state = ctx.state;
    // [A]: the engine's own payment (floating power, then a recycled rune).
    if (!domain.has_value()) return ctx.executor.payAnyPower(player, 1);
    auto& pool = state.player(player).rune_pool;
    int& own = pool.power[static_cast<int>(*domain)];
    if (own > 0) { --own; return true; }
    if (pool.universal_power > 0) { --pool.universal_power; return true; }
    const LocationId base_loc{BaseLocation{player}};
    GameObjectId rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != base_loc) continue;
        if (!runeMatchesDomain(obj, domain)) continue;
        // An exhausted rune first (a ready one can still give Energy), then
        // the lowest id so the pick does not depend on map order.
        if (rune == kInvalidId) { rune = id; continue; }
        const bool cur_ex = state.getObject(rune).is_exhausted;
        if (obj.is_exhausted != cur_ex ? obj.is_exhausted : id < rune) rune = id;
    }
    if (rune == kInvalidId) return false;
    recycleRuneForPower(ctx, player, rune);
    return true;
}

// ── Paying Energy inside an effect ("you may pay [N] to ...") ───────────────
/// Energy `player` could pay right now: floating pool + ready runes in base.
inline int payableEnergy(const GameState& state, PlayerId player) {
    int n = state.player(player).rune_pool.energy;
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
            obj.location.has_value()) ++n;
    return n;
}

/// Pay `amount` Energy: floating pool first, then exhaust ready runes. Returns
/// false (paying nothing) if it can't be covered.
inline bool payEnergy(CardContext& ctx, PlayerId player, int amount) {
    if (amount <= 0) return true;
    if (payableEnergy(ctx.state, player) < amount) return false;
    auto& pool = ctx.state.player(player).rune_pool;
    int from_pool = std::min(pool.energy, amount);
    pool.energy -= from_pool;
    amount -= from_pool;
    for (auto& [id, obj] : ctx.state.objects) {
        if (amount <= 0) break;
        if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
        if (!obj.location.has_value()) continue;
        obj.is_exhausted = true;
        ctx.events.emit(ObjectStateChangedEvent{id, "exhausted"});
        --amount;
    }
    return true;
}

// ── Battlefields and combat ──
/// The battlefield whose card object is `card_object` (a battlefield card's
/// own ctx.source, or a conquer / hold trigger's subject on a legend).
inline std::optional<BattlefieldId> battlefieldOfCard(const GameState& state,
                                                      GameObjectId card_object) {
    if (card_object == kInvalidId) return std::nullopt;
    for (const auto& bf : state.battlefields)
        if (bf.card_object_id == card_object) return bf.id;
    return std::nullopt;
}
/// Combat damage `player` assigned beyond lethal in their combat at `bf` this
/// turn — 0 if their most recent combat was somewhere else (or there was none).
inline int excessCombatDamageAt(const GameState& state, PlayerId player, BattlefieldId bf) {
    const auto& ps = state.player(player);
    return ps.last_combat_bf == bf ? ps.excess_combat_damage : 0;
}

/// Both ends of an Equip are on the board (CR 818: an Equipment you control
/// attaches to a unit you control). objectExists alone is true for every id
/// ever created, including a unit that died in response.
inline bool equipEndsOnBoard(const GameState& state, GameObjectId gear_id, GameObjectId unit_id) {
    if (!state.objectExists(gear_id) || !state.objectExists(unit_id)) return false;
    const auto& gear = state.getObject(gear_id);
    const auto& unit = state.getObject(unit_id);
    return gear.isGear() && gear.location.has_value() &&
           unit.isUnit() && unit.location.has_value();
}

/// Attach an unattached Equipment to a unit (the cost is already paid).
inline void attachEquipped(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id) {
    auto& state = ctx.state;
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name +
                        " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
}

// ── Equip: pay [energy] + one power (of `domain`, or [A] if nullopt), attach ──
// All-or-nothing: nothing is paid unless the whole cost can be.
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id,
                          int energy_cost, std::optional<Domain> domain) {
    if (!equipEndsOnBoard(ctx.state, gear_id, unit_id)) return false;
    if (payableEnergy(ctx.state, ctx.controller) < energy_cost) return false;
    if (!payableOnePower(ctx.state, ctx.controller, domain)) return false;
    // Energy first: a rune exhausted for it can still be recycled for the Power.
    if (!payEnergy(ctx, ctx.controller, energy_cost)) return false;
    if (!payOnePower(ctx, ctx.controller, domain)) return false;
    attachEquipped(ctx, gear_id, unit_id);
    return true;
}
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id,
                          GameObjectId unit_id, std::optional<Domain> domain) {
    return standardEquip(ctx, gear_id, unit_id, 0, domain);
}

// Attach a gear directly (no cost) to a unit (e.g. Edge of Night's
// play-from-facedown → attach). Detaches from any prior bearer first.
inline void attachFree(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id) {
    auto& state = ctx.state;
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    if (gear.attached_to.has_value() && state.objectExists(*gear.attached_to)) {
        auto& old = state.getObject(*gear.attached_to);
        old.attachment_might_bonus -= gear.might_bonus;
        auto it = std::find(old.attachments.begin(), old.attachments.end(), gear_id);
        if (it != old.attachments.end()) old.attachments.erase(it);
        old.recomputeMight();
    }
    gear.attached_to = unit_id;
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    ctx.events.logTrace("ATTACH (free): " + gear.name + " -> " + unit.name);
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
}

// ── Discard-then-act resumable helpers ──
// discardOneThenAct: discard exactly one (agent-chosen), then post_fn(ctx, discarded).
template <typename PostFn>
inline void discardOneThenAct(CardContext& ctx, const std::string& label,
                              PostFn&& post_fn) {
    auto& ri = ctx.state.chain.resuming.value();
    auto& ps = ctx.state.player(ctx.controller);
    auto build_choices = [&]() {
        std::vector<Intent> out;
        for (auto cid : ps.hand) {
            Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
            c.chosen_objects = {cid}; out.push_back(std::move(c));
        }
        return out;
    };
    switch (ri.resume_point) {
    case 0:
        if (ps.hand.empty()) { post_fn(ctx, kInvalidId); return; }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        ri.resume_point = 1;
        return;
    case 1: {
        GameObjectId chosen = kInvalidId;
        auto choice = ctx.executor.takeChoice();
        if (choice && !choice->chosen_objects.empty()) {
            chosen = choice->chosen_objects[0];
            ctx.executor.applyDiscard(ctx.controller, chosen);
        }
        post_fn(ctx, chosen);
        return;
    }
    }
}

// discardThenAct: discard up to `count` (agent-chosen), then post_fn(ctx).
template <typename PostFn>
inline void discardThenAct(CardContext& ctx, int count,
                           const std::string& label, PostFn&& post_fn) {
    auto& ri = ctx.state.chain.resuming.value();
    auto& ps = ctx.state.player(ctx.controller);
    auto build_choices = [&]() {
        std::vector<Intent> out;
        for (auto cid : ps.hand) {
            Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
            c.chosen_objects = {cid}; out.push_back(std::move(c));
        }
        return out;
    };
    switch (ri.resume_point) {
    case 0:
        if (ps.hand.empty() || count <= 0) { post_fn(ctx); return; }
        ri.resume_data = {count};
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        ri.resume_point = 1;
        return;
    case 1: {
        auto choice = ctx.executor.takeChoice();
        if (choice && !choice->chosen_objects.empty())
            ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
        if (!ri.resume_data.empty()) ri.resume_data[0]--;
        if (ri.resume_data.empty() || ri.resume_data[0] <= 0 || ps.hand.empty()) {
            post_fn(ctx); return;
        }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        return;
    }
    }
}

// ── Countering (CR 426) ──
// CR 420.4.b: non-triggered checks of "cards you've played" count FINALIZED
// cards, and a countered card was finalized — so countering does not undo the
// played count. CR 420.4.a.1 / 426.1.b: abilities that trigger on a card being
// played simply never trigger, because those fire at resolution. Nothing is
// reverted here; the function remains as the single place that would change.
inline void revertCounteredPlay(CardContext& /*ctx*/, const ChainItem& /*top*/) {}

/// Put a card that was removed from the chain without resolving where it
/// belongs: `dest` (trash — CR 426.1.a.1 — or hand for "return it to hand"),
/// unless it carries a "then banish it" replacement (CR 391.3.a: a Flow spell
/// that would leave the chain other than by its own execution is banished).
inline void disposeCounteredCard(CardContext& ctx, GameObjectId id,
                                 ZoneType dest = ZoneType::Trash) {
    if (!ctx.state.objectExists(id)) return;
    auto& obj = ctx.state.getObject(id);
    obj.location = std::nullopt;
    if (obj.banish_on_leave_chain) {
        obj.banish_on_leave_chain = false;
        obj.zone = ZoneType::Banishment;
        ctx.state.player(obj.owner).banishment.push_back(id);
        ctx.state.player(obj.owner).cards_banished_this_turn++;
        ctx.events.logTrace("COUNTER: " + obj.name + " -> banished (played for Flow)");
        ctx.events.emit(ObjectStateChangedEvent{id, "banished_owned"});
        return;
    }
    obj.zone = dest;
    if (dest == ZoneType::Hand) ctx.state.player(obj.owner).hand.push_back(id);
    else ctx.state.player(obj.owner).trash.push_back(id);
}

// "This can't be countered" (the spell's own card) or "Your spells and
// abilities can't be countered" (its controller). Every counter effect checks
// this before removing a chain item.
inline bool isUncounterable(const CardContext& ctx, const ChainItem& item) {
    if (ctx.state.player(item.controller).spells_cant_be_countered) return true;
    if (ctx.registry && ctx.state.objectExists(item.source)) {
        const Card* c = ctx.registry->get(ctx.state.getObject(item.source).card_def_id);
        if (c && c->cantBeCountered()) return true;
    }
    return false;
}

inline void counterChainTop(CardContext& ctx) {
    if (ctx.state.chain.items.empty()) return;
    auto& top = ctx.state.chain.items.back();
    if (!top.is_spell) return;
    if (isUncounterable(ctx, top)) return;
    auto countered = top.source;
    revertCounteredPlay(ctx, top);  // CR 425.1.b
    ctx.state.chain.items.pop_back();
    if (ctx.state.objectExists(countered)) {
        ctx.events.logTrace("COUNTER: " + ctx.state.getObject(countered).name + " countered");
        disposeCounteredCard(ctx, countered);
    }
}

// ── Gold tokens ──
inline void createGoldToken(CardContext& ctx) {
    auto loc = BaseLocation{ctx.controller};
    auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        0, {"Gold"}, KeywordSet{}, loc, false);
    if (ctx.state.objectExists(tid))
        ctx.state.getObject(tid).card_def_id = kGoldGearCardDefId;
}

inline GameObjectId createGoldExhausted(CardContext& ctx) {
    LocationId loc{BaseLocation{ctx.controller}};
    auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        0, {}, {}, loc, /*enter_ready=*/false);
    if (ctx.state.objectExists(tok)) {
        auto& gold = ctx.state.getObject(tok);
        gold.is_exhausted = true;
        // Bind the Gold token card so its "[Reaction] Kill this, [E]: Add" ability exists.
        gold.card_def_id = kGoldGearCardDefId;
    }
    return tok;
}

// ── Misc ──
inline GameObjectId findMostRecentlyPlayedSpell(const GameState& state, PlayerId player) {
    const auto& ps = state.player(player);
    for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
        if (!state.objectExists(*it)) continue;
        if (state.getObject(*it).isSpell()) return *it;
    }
    return kInvalidId;
}

inline void moveToLocation(EffectExecutor& exec, GameObjectId obj,
                           const std::optional<LocationId>& loc) {
    if (!loc) return;
    if (std::holds_alternative<BattlefieldLocation>(*loc))
        exec.moveToBattlefield(obj, std::get<BattlefieldLocation>(*loc).id);
    else
        exec.moveToBase(obj);
}

inline bool isMighty(const GameObject& obj) { return obj.current_might >= 5; }

inline bool hasTag(const GameObject& obj, const std::string& tag) {
    for (const auto& t : obj.tags) if (t == tag) return true;
    return false;
}

inline bool isEquipment(const GameObject& obj) {
    return obj.isGear() && hasTag(obj, "Equipment");
}

// ── Tag presence (Ivern / Bird-Cat-Dog-Poro family) ──
struct TagPresence {
    bool bird = false, cat = false, dog = false, poro = false;
    int count() const { return (int)bird + (int)cat + (int)dog + (int)poro; }
    bool allFour() const { return bird && cat && dog && poro; }
};

inline TagPresence scanFriendlyTags(const GameState& state, PlayerId controller) {
    TagPresence out;
    for (const auto& [id, obj] : state.objects) {
        if (!obj.isUnit() || obj.controller != controller) continue;
        if (!obj.location.has_value()) continue;
        for (const auto& tag : obj.tags) {
            if      (tag == "Bird") out.bird = true;
            else if (tag == "Cat")  out.cat  = true;
            else if (tag == "Dog")  out.dog  = true;
            else if (tag == "Poro") out.poro = true;
        }
    }
    return out;
}

// ── Empower costs ───────────────────────────────────────────────────────────
// "[Empower] [N]" / "[Empower] [N][R]" / "[Empower] [N][A][A]".
inline Card::EmpowerSpec empowerCost(int energy, int power = 0,
                                     Domain power_domain = Domain::Fury,
                                     bool any_domain = false) {
    Card::EmpowerSpec s;
    s.valid = true;
    s.cost.energy = energy;
    s.cost.power = power;
    s.cost.power_domain = power_domain;
    s.cost.power_any_domain = any_domain;
    return s;
}

// ── Flow costs ──────────────────────────────────────────────────────────────
inline Card::FlowCost flow(int energy, int power = 0,
                           Domain power_domain = Domain::Fury,
                           bool any_domain = false) {
    Card::FlowCost f;
    f.valid = true;
    f.energy = energy;
    f.power = power;
    f.power_domain = power_domain;
    f.any_domain = any_domain;
    return f;
}

// ── Passive-aura plumbing ───────────────────────────────────────────────────
// Card::applyPassiveAura is invoked once per (card, controller) with an
// instance in play and is not told WHICH instance, so implementations loop
// over every instance. A marker effect makes each instance get its effect once
// per aura pass no matter how many copies are in play (or how often the hook
// is called).
constexpr int kAuraOnceMarker = -777;

template <typename Fn>
void forEachSelfOnce(const Card& card, GameState& state, PlayerId controller, Fn&& fn) {
    for (auto& [sid, self] : state.objects) {
        if (self.card_def_id != card.cardDefId() || self.controller != controller) continue;
        if (!self.location.has_value() && self.zone != ZoneType::LegendZone) continue;
        bool seen = false;
        for (const auto& ae : self.aura_effects)
            if (ae.source == sid && ae.keyword == Keyword::Count &&
                ae.keyword_value == kAuraOnceMarker) { seen = true; break; }
        if (seen) continue;
        GameObject::AuraEffect marker;
        marker.source = sid;
        marker.keyword_value = kAuraOnceMarker;
        self.aura_effects.push_back(marker);
        fn(sid, self);
    }
}

/// "[Empowered][>] ..." — visit each Empowered instance once per aura pass.
template <typename Fn>
void forEachEmpoweredSelf(const Card& card, GameState& state, PlayerId controller, Fn&& fn) {
    forEachSelfOnce(card, state, controller, [&](GameObjectId sid, GameObject& self) {
        if (self.isEmpowered()) fn(sid, self);
    });
}

/// "+N [M]" from a continuous effect.
inline void auraMight(GameObject& obj, GameObjectId source, int amount) {
    GameObject::AuraEffect ae;
    ae.source = source;
    ae.might_bonus = amount;
    obj.aura_effects.push_back(ae);
}

/// True when `obj` already carries a continuous effect from `source` in this
/// aura pass. A battlefield card's applyPassiveAura runs once per battlefield
/// slot holding that card and is not told which slot, so a hook that walks
/// every slot with its def uses this to apply each slot's effect only once
/// when both players brought the same battlefield.
inline bool hasAuraFrom(const GameObject& obj, GameObjectId source) {
    for (const auto& ae : obj.aura_effects)
        if (ae.source == source) return true;
    return false;
}

/// Whether `obj` has `kw` as far as this aura pass has got: its printed and
/// "this turn" keywords plus what the pass has granted so far. For use inside
/// applyPassiveAura, where GameObject::hasKeyword() only sees the printed
/// ones (its aura cache is rebuilt after every hook has run). Equipment
/// keywords are granted before any hook, unit / gear / legend hooks run before
/// battlefield hooks, so a battlefield's "units here with [Keyword]" sees
/// both; between two unit hooks the order is not defined.
inline bool hasKeywordInAuraPass(const GameObject& obj, Keyword kw) {
    if (obj.keywords.has(kw)) return true;
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == kw) return true;
    return false;
}

/// A keyword from a continuous effect. For [Assault N] / [Shield N] pass N
/// (0 = the bare keyword, which is N = 1): the engine's aura fold adds the
/// +N [M] while the bearer is an attacker / a defender, so the effect itself
/// carries no might_bonus.
inline void auraKeyword(GameObject& obj, GameObjectId source, Keyword kw, int value = 0) {
    GameObject::AuraEffect ae;
    ae.source = source;
    ae.keyword = kw;
    ae.keyword_value = value;
    obj.aura_effects.push_back(ae);
}

// ── Board queries ───────────────────────────────────────────────────────────
/// Runes `player` controls (in base, ready or exhausted).
inline int runesControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player && obj.location.has_value()) ++n;
    return n;
}

/// Gear `player` controls on the board.
inline int gearControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.controller == player && obj.location.has_value()) ++n;
    return n;
}

/// Cards in `player`'s trash with the given name.
inline int namedInTrash(const GameState& state, PlayerId player, const std::string& name) {
    int n = 0;
    for (auto cid : state.player(player).trash)
        if (state.objectExists(cid) && state.getObject(cid).name == name) ++n;
    return n;
}

/// On-board units matching `pred`, honoring "can't be chosen by enemy spells
/// and abilities" for units `chooser` doesn't control.
template <typename Pred>
std::vector<GameObjectId> chooseableUnits(const GameState& state, PlayerId chooser, Pred&& pred) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : state.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.controller != chooser && obj.untargetable_by_enemy) continue;
        if (pred(obj)) out.push_back(id);
    }
    return out;
}

/// Other units `controller` has at the same location as `self`.
inline int otherFriendlyUnitsHere(const GameState& state, const GameObject& self) {
    if (!self.location.has_value()) return 0;
    int n = 0;
    for (const auto& [id, obj] : state.objects) {
        if (id == self.id || !obj.isUnit() || obj.controller != self.controller) continue;
        if (obj.location.has_value() && *obj.location == *self.location) ++n;
    }
    return n;
}

// ── Agent choices by hand ──
// For cards whose resolution asks a custom question (a unit from a computed
// list, then a destination, …) and so build their own MakeChoice options
// rather than going through Card::pickTarget. These two do NO bookkeeping:
// when the object picked is one the controller is choosing (not a card being
// sorted, not the opponent's own pick), filter the candidates with
// Card::offerableChoices and call Card::commitChosenObject on the pick, or
// Deflect (CR 809) goes unpaid and "chosen" triggers never see it.

/// A MakeChoice option that picks object `id`.
inline Intent objChoice(PlayerId p, GameObjectId id) {
    Intent c;
    c.type = IntentType::MakeChoice;
    c.player = p;
    c.chosen_objects = {id};
    return c;
}
/// The object the agent picked from objChoice options (kInvalidId if none).
inline GameObjectId takenObject(CardContext& ctx) {
    auto c = ctx.executor.takeChoice();
    return (c && !c->chosen_objects.empty()) ? c->chosen_objects[0] : kInvalidId;
}

/// A MakeChoice option carrying the int `v`, shown to the agent as `label`.
inline Intent valChoice(PlayerId p, int v, const std::string& label) {
    Intent c;
    c.type = IntentType::MakeChoice;
    c.player = p;
    c.chosen_value = v;
    c.choice_label = label;
    return c;
}
/// The int the agent picked from valChoice options (`fallback` if none).
inline int takenValue(CardContext& ctx, int fallback) {
    auto c = ctx.executor.takeChoice();
    return (c && c->chosen_value.has_value()) ? *c->chosen_value : fallback;
}

// ── Picking an object without "choosing" it ─────────────────────────────────
/// Ask the controller which of `options` to use when the pick is not a choice
/// of that object in the rules sense — which unit's buff to spend, which rune
/// to recycle, which trash card pays a cost. Unlike Card::pickTarget it
/// charges no Deflect, records nothing in chosen_objects and fires no "when
/// you choose" trigger.
///
/// Returns nullopt while the agent is being asked (the caller returns and is
/// re-entered), kInvalidId when there is nothing to pick (or, with
/// `may_decline`, when the controller picked nothing), else the object. A
/// single option is taken without asking unless it may be declined. The
/// answer is kept on the resolving chain item, so later entries — and the
/// resolution after a finalize pass — get the same one back; the caller checks
/// it is still what the effect needs.
///
/// Progress lives in resume_data[slot] (0 new / 1 asked / 2 answered) and
/// resume_data[slot + 1] (the object): `slot` must be 7 or more (0..6 belong
/// to the Card::pick* helpers) and two picks in one callback use slots two
/// apart.
inline std::optional<GameObjectId> selectObject(CardContext& ctx, const std::string& label,
                                                std::vector<GameObjectId> options,
                                                size_t slot = 7, bool may_decline = false) {
    std::sort(options.begin(), options.end());
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value())   // direct invocation: nobody to ask
        return options.empty() ? kInvalidId : options.front();
    auto& ri = *ri_opt;
    while (ri.resume_data.size() < slot + 2) ri.resume_data.push_back(0);
    auto answer = [&ri, slot](GameObjectId id) {
        ri.resume_data[slot] = 2;
        ri.resume_data[slot + 1] = static_cast<int32_t>(id);
        return id;
    };
    if (ri.resume_data[slot] == 2) return static_cast<GameObjectId>(ri.resume_data[slot + 1]);
    if (ri.resume_data[slot] == 1) {
        auto picked = ctx.executor.takeChoice();
        if (picked.has_value() && !picked->chosen_objects.empty() &&
            std::find(options.begin(), options.end(), picked->chosen_objects.front()) !=
                options.end())
            return answer(picked->chosen_objects.front());
        // "None", or an answer that is no longer on offer.
        return answer((may_decline || options.empty()) ? kInvalidId : options.front());
    }
    if (options.empty()) return answer(kInvalidId);
    if (options.size() == 1 && !may_decline) return answer(options.front());
    std::vector<Intent> choices;
    choices.reserve(options.size() + 1);
    for (auto id : options) {
        Intent c;
        c.type = IntentType::MakeChoice;
        c.player = ctx.controller;
        c.chosen_objects = {id};
        choices.push_back(std::move(c));
    }
    if (may_decline) {
        Intent none;
        none.type = IntentType::MakeChoice;
        none.player = ctx.controller;
        none.chosen_value = 0;
        none.choice_label = "None — " + label;
        choices.push_back(std::move(none));
    }
    ctx.events.logTrace("SELECT_PROMPT: " + label + " [" + std::to_string(options.size()) +
                        " options]");
    ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
    ri.resume_data[slot] = 1;
    return std::nullopt;
}

// ── Split damage (CR 355.14) ────────────────────────────────────────────────
// "Deal N damage split among <units>": the controller decides the division at
// resolution (CR 355.14.e), every unit that takes part receives at least 1
// (355.14.f-g), and at most N units can take part (355.14.c). The division is
// built one damage at a time: each prompt assigns the next point to a unit the
// controller picks, which can express every legal split and nothing else.
// Damage is marked only after the whole division is known.
//
// The card is re-entered after every answer, so progress lives on the
// resolving chain item: resume_point kSplitDamageAssign / Await / Done (clear
// of the values the Card::pick* / confirmOptional helpers leave there) and
// resume_data = [total, damage per pick, picked unit, picked unit, ...] — so a
// card that splits damage must not also call a Card::pick* helper in the same
// callback (they keep their answers in resume_data[0..6]).
//
// ENGINE-GAP: CR 355.14.a-b make every unit in a split a Target chosen when
// the spell or ability is finalized on the chain. The play/trigger path can
// only lock a fixed TargetRequirements::count of targets, so a variable
// number (1..N) of split targets cannot be declared there; they are chosen at
// resolution instead.
// ENGINE-GAP: ChainManager::stepResolve allows 16 re-entries per resolution
// (kMaxResumeIterations), so at most kSplitDamageMaxPicks prompts are issued;
// above that total each pick assigns ceil(total / kSplitDamageMaxPicks)
// damage, which coarsens the split for a source with more than 12 to divide.
constexpr int kSplitDamageAssign = 100;
constexpr int kSplitDamageAwait = 101;
constexpr int kSplitDamageDone = 102;
constexpr int kSplitDamageMaxPicks = 12;

/// Returns true once the damage has been dealt (or there was nothing to do),
/// false while a pick is pending. `legal` are the units the controller may
/// choose; `damaged` receives the units that were dealt damage.
inline bool splitDamageAmong(CardContext& ctx, const std::string& label, int total,
                             GameObjectId damage_source, std::vector<GameObjectId> legal,
                             std::vector<GameObjectId>& damaged) {
    std::sort(legal.begin(), legal.end());
    std::vector<std::pair<GameObjectId, int>> alloc;
    auto add = [&alloc](GameObjectId id, int n) {
        for (auto& a : alloc) if (a.first == id) { a.second += n; return; }
        alloc.emplace_back(id, n);
    };
    auto deal = [&]() {
        for (const auto& [id, n] : alloc) {
            if (n <= 0 || !ctx.state.objectExists(id)) continue;
            const auto& o = ctx.state.getObject(id);
            // "If you've chosen an enemy unit this turn" (as Card::pickTarget).
            if (o.isUnit() && o.location.has_value() && o.controller != ctx.controller)
                ctx.state.player(ctx.controller).chose_enemy_unit_this_turn = true;
            ctx.events.logTrace("SPLIT: " + label + " -> " + std::to_string(n) +
                                " to " + o.name);
            ctx.executor.dealDamage(id, n, damage_source);
            damaged.push_back(id);
        }
    };

    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) {
        // Direct invocation (no chain item to resume): no agent can be asked,
        // so default to an even spread over the legal units, like the other
        // choice helpers default to their first option.
        if (total <= 0 || legal.empty()) return true;
        for (int i = 0; i < total; ++i) add(legal[i % legal.size()], 1);
        deal();
        return true;
    }
    auto& ri = *ri_opt;
    if (ri.resume_point == kSplitDamageDone) return true;

    // Deflect (CR 809): choosing an opponent's Deflect unit costs [A] per
    // Deflect, once per unit chosen — charged when the unit first joins the
    // split, the way Card::pickTarget charges it. A unit whose Deflect cost
    // can't be paid is not offered; one already in the split stays available.
    auto alreadyPicked = [&ri](GameObjectId id) {
        for (size_t i = 2; i < ri.resume_data.size(); ++i)
            if (static_cast<GameObjectId>(ri.resume_data[i]) == id) return true;
        return false;
    };
    auto offered = [&]() {
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        std::vector<GameObjectId> out;
        for (auto t : legal)
            if (alreadyPicked(t) || ctx.executor.deflectCostFor(ctx.controller, t) <= payable)
                out.push_back(t);
        return out;
    };

    if (ri.resume_point == kSplitDamageAwait) {
        auto picked = ctx.executor.takeChoice();
        auto can_pick = offered();
        GameObjectId t = can_pick.empty() ? kInvalidId : can_pick.front();
        if (picked.has_value() && !picked->chosen_objects.empty() &&
            std::find(can_pick.begin(), can_pick.end(),
                      picked->chosen_objects.front()) != can_pick.end())
            t = picked->chosen_objects.front();
        if (t != kInvalidId && !alreadyPicked(t)) {
            const int cost = ctx.executor.deflectCostFor(ctx.controller, t);
            if (cost > 0) {
                ctx.executor.payAnyPower(ctx.controller, cost);
                ctx.events.logTrace("DEFLECT: pays " + std::to_string(cost) +
                                    " [A] to choose " + ctx.state.getObject(t).name);
            }
            ri.chosen_objects.push_back(t);
        }
        ri.resume_data.push_back(static_cast<int32_t>(t));
        ri.resume_point = kSplitDamageAssign;
    } else if (ri.resume_point != kSplitDamageAssign) {
        // First entry: latch the amount to divide.
        if (total <= 0 || legal.empty()) {
            ri.resume_point = kSplitDamageDone;
            return true;
        }
        // One legal unit: a single pick takes everything.
        int per_pick = legal.size() == 1
            ? total : (total + kSplitDamageMaxPicks - 1) / kSplitDamageMaxPicks;
        ri.resume_data = {total, per_pick};
        ri.resume_point = kSplitDamageAssign;
        if (offered().empty()) {   // nothing the controller can afford to choose
            ri.resume_point = kSplitDamageDone;
            return true;
        }
    }

    total = ri.resume_data[0];
    const int per_pick = std::max(1, ri.resume_data[1]);
    int remaining = total;
    for (size_t i = 2; i < ri.resume_data.size() && remaining > 0; ++i) {
        int n = std::min(per_pick, remaining);
        add(static_cast<GameObjectId>(ri.resume_data[i]), n);
        remaining -= n;
    }

    const auto can_pick = remaining > 0 ? offered() : std::vector<GameObjectId>{};
    if (remaining > 0 && !can_pick.empty()) {
        std::vector<Intent> options;
        options.reserve(can_pick.size());
        for (auto t : can_pick) options.push_back(objChoice(ctx.controller, t));
        ctx.events.logTrace("SPLIT_PROMPT: " + label + " (" + std::to_string(remaining) +
                            " of " + std::to_string(total) + " damage left to assign)");
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                   "split damage: " + label + " (" +
                                   std::to_string(remaining) + " left)");
        ri.resume_point = kSplitDamageAwait;
        return false;
    }

    ri.resume_point = kSplitDamageDone;
    deal();
    return true;
}

/// "Units can't move from here to base" (Vilemaw's Lair): true when `u` is at
/// a battlefield that says so. Only a move is stopped; a recall is not a move
/// (CR 458).
inline bool moveToBaseBlocked(const GameState& state, const GameObject& u) {
    const auto here = u.battlefieldId();
    if (!here.has_value()) return false;
    for (const auto& bf : state.battlefields)
        if (bf.id == *here) return bf.blocks_move_to_base;
    return false;
}

// ── Played from facedown (CR 811) ───────────────────────────────────────────
/// The battlefield the spell `spell` was hidden at, if this play of it is from
/// facedown (stamped by the engine as it leaves the facedown zone). A hidden
/// spell's targets, and any unit it plays, are tied to that battlefield (CR
/// 811.1.d.2 / 811.1.d.5).
inline std::optional<BattlefieldId> facedownPlayBattlefield(const GameState& state,
                                                            GameObjectId spell) {
    if (!state.objectExists(spell)) return std::nullopt;
    const auto& counters = state.getObject(spell).card_counters;
    auto turn = counters.find("__facedown_play_turn");
    auto bf = counters.find("__facedown_play_bf");
    if (turn == counters.end() || bf == counters.end()) return std::nullopt;
    if (turn->second != state.turn.turn_number || bf->second <= 0) return std::nullopt;
    return static_cast<BattlefieldId>(bf->second - 1);
}

// ── Where an effect-played unit goes (CR 355.2.a) ───────────────────────────
// "Play a unit / a unit token" with no location printed: its controller's base
// or a battlefield they control, their choice. One Card::pickMode mode per
// location:
//
//     auto where = playLocations(ctx.state, ctx.controller, facedownPlayBattlefield(...));
//     int mode = pickMode(ctx, label, where.count(), where.labels);
//     if (mode < 0 || mode >= where.count()) return;
//     ... where.locations[mode] ...
struct PlayLocations {
    std::vector<LocationId> locations;
    std::vector<std::string> labels;
    int count() const { return static_cast<int>(locations.size()); }
};
/// `only_bf`: the spell was played from facedown at that battlefield, so the
/// unit must be played there (CR 811.1.d.5). `include_base` = false for text
/// that names "a battlefield you control".
inline PlayLocations playLocations(const GameState& state, PlayerId player,
                                   std::optional<BattlefieldId> only_bf = std::nullopt,
                                   bool include_base = true) {
    PlayLocations out;
    auto bfName = [&state](const BattlefieldState& bf) {
        return state.objectExists(bf.card_object_id)
            ? state.getObject(bf.card_object_id).name
            : "Battlefield #" + std::to_string(static_cast<int>(bf.id));
    };
    if (only_bf.has_value()) {
        for (const auto& bf : state.battlefields)
            if (bf.id == *only_bf) {
                out.locations.push_back(LocationId{BattlefieldLocation{bf.id}});
                out.labels.push_back(bfName(bf));
            }
        return out;
    }
    if (include_base) {
        out.locations.push_back(LocationId{BaseLocation{player}});
        out.labels.push_back("Base");
    }
    for (const auto& bf : state.battlefields) {
        if (out.locations.size() >= 32) break;   // pickMode's mask is 32 bits
        if (!bf.controller.has_value() || *bf.controller != player) continue;
        out.locations.push_back(LocationId{BattlefieldLocation{bf.id}});
        out.labels.push_back(bfName(bf));
    }
    return out;
}

/// Destinations a card effect may move `u` to, int-coded for valChoice:
/// 0 = its controller's base, 1 + i = battlefield i. Excludes where it is now.
inline std::vector<int> moveDests(const GameState& state, const GameObject& u) {
    std::vector<int> out;
    if (!u.location.has_value()) return out;
    if (!u.isAtBase() && !moveToBaseBlocked(state, u)) out.push_back(0);
    for (const auto& bf : state.battlefields) {
        auto here = u.battlefieldId();
        if (here.has_value() && *here == bf.id) continue;
        out.push_back(1 + static_cast<int>(bf.id));
    }
    return out;
}
inline std::string destLabel(int d) {
    return d <= 0 ? std::string("Base") : "Battlefield #" + std::to_string(d - 1);
}
/// Move `u` to a moveDests destination.
inline void moveTo(CardContext& ctx, GameObjectId u, int dest) {
    if (dest <= 0) ctx.executor.moveToBase(u);
    else ctx.executor.moveToBattlefield(u, static_cast<BattlefieldId>(dest - 1));
}

} // namespace riftbound

// The Equip base classes (SimpleEquipGear / UniversalEquipGear) are built on
// the helpers above; included last so a card file that includes only this
// header still gets them.
#include "cards/gear/equip_base.h"
