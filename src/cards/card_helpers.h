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
#include "cards/gear/equip_base.h"   // canonical standardEquip(+energy,Domain) + bases
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
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
inline bool payOnePower(CardContext& ctx, PlayerId player,
                        std::optional<Domain> domain) {
    auto& state = ctx.state;
    auto& pool = state.player(player).rune_pool;
    if (domain.has_value()) {
        int& own = pool.power[static_cast<int>(*domain)];
        if (own > 0) { --own; return true; }
    } else {
        for (int di = 0; di < static_cast<int>(Domain::Count); ++di)
            if (pool.power[di] > 0) { --pool.power[di]; return true; }
    }
    if (pool.universal_power > 0) { --pool.universal_power; return true; }
    const LocationId base_loc{BaseLocation{player}};
    GameObjectId rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != base_loc) continue;
        if (!runeMatchesDomain(obj, domain)) continue;
        if (rune == kInvalidId || (obj.is_exhausted && !state.getObject(rune).is_exhausted))
            rune = id;
    }
    if (rune == kInvalidId) return false;
    auto& dr = state.getObject(rune);
    dr.location = std::nullopt;
    dr.zone = ZoneType::RuneDeck;
    state.player(player).rune_deck.insert(state.player(player).rune_deck.begin(), rune);
    ctx.events.logTrace("  ADDL_COST: recycled " + dr.name + " for power");
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

inline bool standardEquip(CardContext& ctx, GameObjectId gear_id,
                          GameObjectId unit_id, std::optional<Domain> domain) {
    auto& state = ctx.state;
    if (!state.objectExists(gear_id) || !state.objectExists(unit_id)) return false;
    if (!payOnePower(ctx, ctx.controller, domain)) return false;
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
    return true;
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
// rather than going through Card::pickTarget.

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

/// Destinations a card effect may move `u` to, int-coded for valChoice:
/// 0 = its controller's base, 1 + i = battlefield i. Excludes where it is now.
inline std::vector<int> moveDests(const GameState& state, const GameObject& u) {
    std::vector<int> out;
    if (!u.location.has_value()) return out;
    if (!u.isAtBase()) out.push_back(0);
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
