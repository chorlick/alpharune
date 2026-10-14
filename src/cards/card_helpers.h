#pragma once
/// @file card_helpers.h
/// Cross-cutting card helpers used across multiple card types. Type-specific
/// bases live in their type dirs (gear/equip_base.h, units/weaponmaster_base.h).
#include "engine/deck_ops.h"
#include "engine/trash_replacement.h"
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "engine/resource_payment.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace riftbound {

// The engine-known Gold gear token's card_def_id.
constexpr CardDefId kGoldGearCardDefId = 326;

// ── Paying one Power from a card effect ──
// One Power of `domain` (any domain if nullopt), the way CR 164.2 lets it be
// paid: floating Power of that domain, then universal Power, then recycling a
// matching rune — ready or exhausted (an exhausted one first, since a ready
// rune can still be exhausted for Energy).
/// The object ids a run of integer slots holds (`resume_data`, a reflexive
/// trigger's data): each slot is the id's number, -1 for kInvalidId.
template <class It>
inline std::vector<GameObjectId> objectIdsFromRaw(It first, It last) {
    std::vector<GameObjectId> ids;
    for (; first != last; ++first) ids.push_back(GameObjectId::fromRaw(*first));
    return ids;
}
inline std::vector<GameObjectId> objectIdsFromRaw(const std::vector<int32_t>& raw) {
    return objectIdsFromRaw(raw.begin(), raw.end());
}

inline bool runeMatchesDomain(const GameObject& rune, std::optional<Domain> domain) {
    if (!domain.has_value()) return true;
    for (auto d : rune.domains) if (d == *domain) return true;
    return false;
}
inline bool payableOnePower(const GameState& state, PlayerId player,
                            std::optional<Domain> domain) {
    return resource::payablePower(state, player, domain.value_or(Domain::Count)) >= 1;
}
/// "Its controller", said of a card that an effect has taken off the board
/// (killed, returned to hand, banished, recycled): the player who controlled
/// it when it left. Off the board the object is its owner's again (CR 124.1,
/// forgetBoardState), so the live `controller` of a unit that was taken and
/// then died names the wrong player; the move recorded who it was
/// (GameObject::LastKnown::controller, CR 359.3.e.13 "look back"). For a card
/// still on the board this is its controller now.
inline PlayerId controllerWhenItLeft(const GameState& state, GameObjectId id) {
    if (!state.objectExists(id)) return PlayerId::None;
    const auto& obj = state.getObject(id);
    if (obj.location.has_value() || !obj.last_known.valid) return obj.controller;
    return obj.last_known.controller;
}
/// "Kill X. Its controller ...": the controller the second instruction may
/// reference after this effect's own kill of `id`.
///  - still on the board (the death was replaced by a recall, "I can't be
///    killed"): its controller now;
///  - it died: who controlled it as it died (CR 359.3.e.13);
///  - it left the board another way — the death was replaced by a banish
///    (Smite): PlayerId::None. "The unit was moved to a non-board zone by an
///    effect other than [this one] and thus no longer has a controller that
///    [it] can reference" (Vendetta rules FAQ, the Hidden Blade examples).
inline PlayerId controllerAfterKill(const GameState& state, GameObjectId id) {
    if (!state.objectExists(id)) return PlayerId::None;
    const auto& obj = state.getObject(id);
    if (obj.location.has_value()) return obj.controller;
    if (obj.last_known.valid && obj.last_known.died) return obj.last_known.controller;
    return PlayerId::None;
}
/// `id` as it left the board, for an ability that is resolving (or being
/// finalized): the snapshot that travelled with the ability's chain item if
/// `id` is the object whose leaving triggered it (ChainItem::
/// subject_as_it_left — each death of a card that died twice has its own),
/// otherwise the object's latest (GameObject::last_known). Check `.valid`.
inline const GameObject::LastKnown& asItLeft(const CardContext& ctx, GameObjectId id) {
    const auto& item = ctx.state.chain.resuming;
    if (item.has_value() && item->triggering_subject == id &&
        item->subject_as_it_left.has_value())
        return *item->subject_as_it_left;
    return ctx.state.getObject(id).last_known;
}
/// controllerWhenItLeft for an ability on the chain: see asItLeft.
inline PlayerId controllerWhenItLeft(const CardContext& ctx, GameObjectId id) {
    if (!ctx.state.objectExists(id)) return PlayerId::None;
    const auto& item = ctx.state.chain.resuming;
    if (item.has_value() && item->triggering_subject == id &&
        item->subject_as_it_left.has_value())
        return item->subject_as_it_left->controller;
    return controllerWhenItLeft(ctx.state, id);
}
inline bool payOnePower(CardContext& ctx, PlayerId player,
                        std::optional<Domain> domain) {
    // The engine's own payment (engine/resource_payment.h): floating Power,
    // then a recycled rune from the player's base, an exhausted one first.
    return ctx.executor.payResources(player, 0, 1, resource::maskOf(domain.value_or(Domain::Count)));
}

// ── Paying Energy inside an effect ("you may pay [N] to ...") ───────────────
/// Energy `player` could pay right now: floating pool + ready runes in their
/// base.
inline int payableEnergy(const GameState& state, PlayerId player) {
    return resource::payableEnergy(state, player);
}

/// Whether `player` can pay [amount] now — floating Energy, ready runes, or
/// the Energy their [Reaction] [Add] abilities would add while paying (CR
/// 430.3, 445.2.c; testing phase D30). What a "you may pay [N]" asks before
/// it offers the "may".
inline bool canPayEnergy(CardContext& ctx, PlayerId player, int amount) {
    if (amount <= 0) return true;
    return ctx.executor.canPayWithAdds(player, amount, 0, resource::kAnyDomain);
}

/// Pay `amount` Energy: floating pool first, then exhaust ready runes in the
/// player's base (lowest id first). Returns false (paying nothing) if it can't
/// be covered.
inline bool payEnergy(CardContext& ctx, PlayerId player, int amount) {
    if (amount <= 0) return true;
    return ctx.executor.payResources(player, amount, 0, resource::kAnyDomain);
}

// ── Card::mayChooseAtFinalization, the common shapes ──
/// Yes if `object` is a unit `controller` controls on the board and `also`
/// holds for it; No otherwise (an object that does not exist included).
template <typename Pred>
inline Card::FinalizeChoice friendlyUnitChoice(const GameState& state, PlayerId controller,
                                               GameObjectId object, Pred&& also) {
    if (object == kInvalidId || !state.objectExists(object)) return Card::FinalizeChoice::No;
    const auto& u = state.getObject(object);
    if (!u.isUnit() || u.controller != controller || !u.location.has_value())
        return Card::FinalizeChoice::No;
    return also(u) ? Card::FinalizeChoice::Yes : Card::FinalizeChoice::No;
}
inline Card::FinalizeChoice friendlyUnitChoice(const GameState& state, PlayerId controller,
                                               GameObjectId object) {
    return friendlyUnitChoice(state, controller, object, [](const GameObject&) { return true; });
}

// ── "... at end of turn" ──
/// "Disempower it at end of turn" / "Empower it at end of turn" (Tornado
/// Warrior, Sanction): a delayed triggered ability (CR 391.2) of the Ending
/// Step (CR 317.1.a), about `subject`, put on the chain there with priority.
/// It fires on the card's onTrigger with firing_trigger AtEndOfTurn and the
/// subject as triggering_subject; it ends if the subject leaves the board
/// first (CR 391.5.a, DelayedAbility::subject).
inline void atEndOfTurnAbout(CardContext& ctx, CardDefId card, GameObjectId subject,
                             int which = 0) {
    DelayedAbility da;
    da.source = ctx.source;
    da.card_def_id = card;
    da.controller = ctx.controller;
    da.trigger = TriggerType::AtEndOfTurn;
    da.expires_on_turn = ctx.state.turn.turn_number;
    da.subject = subject;
    da.ability_index = which;   // the chain item's ability_index when it fires
    ctx.state.delayed_abilities.push_back(da);
}

// ── Dependent keywords ──
/// [Legion] (CR 812.1.c): "a card different than the one with the Legion
/// ability has been Finalized by you on the same turn". The count includes
/// the card itself once it has been played, so another card makes it 2.
/// For a triggered [Legion] ability this is its trigger condition (CR
/// 727.1.c.1: the ability must be Active for its trigger to be evaluated).
inline bool legionActive(const GameState& state, PlayerId player) {
    return state.player(player).cards_played_this_turn >= 2;
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

/// Attach an Equipment to a unit (the cost is already paid). Every attach
/// is EffectExecutor::attachGear: on-board checks, detach from a previous
/// bearer, rules text Inactive (CR 718.2).
inline void attachEquipped(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id) {
    ctx.executor.attachGear(gear_id, unit_id);
}

// Attach a gear directly (no cost) to a unit (e.g. Edge of Night's
// play-from-facedown → attach). Detaches from any prior bearer first.
inline void attachFree(CardContext& ctx, GameObjectId gear_id, GameObjectId unit_id) {
    ctx.executor.attachGear(gear_id, unit_id);
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
        RIFT_TRACE(ctx.events, "COUNTER: " + obj.name + " -> banished (played for Flow)");
        ctx.events.emit(ObjectStateChangedEvent{id, "banished_owned"});
        return;
    }
    if (dest == ZoneType::Hand) {
        // From the chain: both players know which card went back.
        deck::putInHand(ctx.state, id, knowledge::nobody(), deck::Place::Public);
    } else {
        obj.zone = dest;
        ctx.state.player(obj.owner).trash.push_back(id);
        settleInTrash(ctx.state, ctx.events, id);
    }
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

/// The finalized spells and abilities on the chain `ctx`'s item may choose
/// (CR 355.9.a.2) and that meet `pred`: never the item itself (CR 355.9.c),
/// newest first — the order Card::pickChainItem offers them in.
template <typename Pred>
inline std::vector<ChainItemId> chainItemChoices(const CardContext& ctx, Pred&& pred) {
    std::vector<ChainItemId> out;
    const auto& items = ctx.state.chain.items;
    const ChainItemId self = ctx.state.chain.resuming.has_value()
        ? ctx.state.chain.resuming->id : ChainItemId{kInvalidId};
    for (auto it = items.rbegin(); it != items.rend(); ++it) {
        if (it->status != ChainItemStatus::Finalized) continue;
        if (it->id == self) continue;
        if (it->is_spell && it->source == ctx.source) continue;
        if (pred(*it)) out.push_back(it->id);
    }
    return out;
}
/// The same over a bare state, for a play gate (hasLegalTargets): whether any
/// finalized item on the chain meets `pred`.
template <typename Pred>
inline bool anyChainItem(const GameState& state, Pred&& pred) {
    for (const auto& ci : state.chain.items)
        if (ci.status == ChainItemStatus::Finalized && pred(ci)) return true;
    return false;
}
/// The chain item `id`, or null if it is not on the chain.
inline const ChainItem* chainItem(const GameState& state, ChainItemId id) {
    for (const auto& ci : state.chain.items) if (ci.id == id) return &ci;
    return nullptr;
}
/// Counter the chosen chain item `id` (CR 426): a spell goes to its owner's
/// trash (or `dest`), an ability is removed; one that "can't be countered"
/// stays. Through EffectExecutor::removeChainItem — the one way a card takes
/// an item off the chain. Returns true if it was countered.
inline bool counterChosenItem(CardContext& ctx, ChainItemId id, ZoneType dest = ZoneType::Trash) {
    const ChainItem* item = chainItem(ctx.state, id);
    if (!item) return false;
    if (isUncounterable(ctx, *item)) {
        RIFT_TRACE(ctx.events, "COUNTER: that spell or ability can't be countered");
        return false;
    }
    revertCounteredPlay(ctx, *item);  // CR 425.1.b
    auto removed = ctx.executor.removeChainItem(id);
    if (!removed.has_value()) return false;
    const std::string name = ctx.state.objectExists(removed->source)
        ? ctx.state.getObject(removed->source).name.get() : std::string("?");
    if (removed->is_spell) {
        RIFT_TRACE(ctx.events, "COUNTER: " + name + " countered");
        disposeCounteredCard(ctx, removed->source, dest);
    } else {
        RIFT_TRACE(ctx.events, "COUNTER: ability of " + name + " countered");
    }
    return true;
}


// ── Gold tokens ──
inline void createGoldToken(CardContext& ctx) {
    auto loc = BaseLocation{ctx.controller};
    auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        // CR 187.5: a domainless gear token with its
                                        // ability and no tag ("Gold" is its name).
                                        0, {}, KeywordSet{}, loc, false, {},
                                        kGoldGearCardDefId);
    (void)tid;
}

inline GameObjectId createGoldExhausted(CardContext& ctx) {
    LocationId loc{BaseLocation{ctx.controller}};
    auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                        0, {}, {}, loc, /*enter_ready=*/false, {},
                                        kGoldGearCardDefId,   // the Gold card: its "[Reaction] Kill this, [E]: Add"
                                        // Exhausted as its play is announced; "Your
                                        // tokens enter ready" (Renata Glasc,
                                        // Industrialist) replaces that (CR 184.1, 376).
                                        /*enters_exhausted=*/true);
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

/// For the triggerCondition of a "Once each turn, when ..." ability
/// (CR 383.3.e.1: "If its trigger condition would be fulfilled and it has
/// already been performed that many times, it does not trigger"): true while
/// an instance of `source`'s ability for `trigger` is on the chain, pending,
/// finalized or resolving — that instance is this turn's performance, so the
/// event does not trigger another. The card keeps its own "performed on turn
/// N" counter, written as the ability resolves, and tests both.
inline bool triggeredAbilityOnChain(const GameState& state, GameObjectId source,
                                    TriggerType trigger) {
    auto same = [&](const ChainItem& ci) {
        return ci.source == source && ci.is_ability && !ci.is_activated_ability &&
               ci.reflexive_index < 0 && ci.fired_trigger == trigger;
    };
    if (state.chain.resuming.has_value() && same(*state.chain.resuming)) return true;
    for (const auto& ci : state.chain.items)
        if (same(ci)) return true;
    return false;
}

/// The move that triggered the ability being checked or resolved ("When I
/// move", "When a unit moves from here"): where it started and ended. While
/// TriggerManager is announcing a move (a triggerCondition is being asked) it
/// is that move; otherwise the one recorded on the resolving chain item. Null
/// when the ability was not triggered by a move.
inline const ChainItem::Move* triggeringMove(const GameState& state) {
    if (state.chain.move_being_announced.has_value()) return &*state.chain.move_being_announced;
    if (state.chain.resuming.has_value() && state.chain.resuming->triggering_move.has_value())
        return &*state.chain.resuming->triggering_move;
    return nullptr;
}
/// The battlefield a location is, if it is one.
inline std::optional<BattlefieldId> battlefieldOfLocation(const LocationId& loc) {
    if (std::holds_alternative<BattlefieldLocation>(loc))
        return std::get<BattlefieldLocation>(loc).id;
    return std::nullopt;
}

/// Which play of its controller's turn `card` was — 1 for "your first card
/// this turn", 2 for "your second card" — or 0 if it was not played this turn.
/// Stamped on the card by TriggerManager::onCardPlayed.
inline int playOrdinalThisTurn(const GameState& state, GameObjectId card) {
    if (!state.objectExists(card)) return 0;
    const auto& counters = state.getObject(card).card_counters;
    auto turn = counters.find("__play_ordinal_turn");
    if (turn == counters.end() || turn->second != state.turn.turn_number + 1) return 0;
    auto n = counters.find("__play_ordinal");
    return n == counters.end() ? 0 : n->second;
}

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
    for (const auto& [id, obj] : state.objects.onBoard()) {
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
    // (Only objects in play can pass the test below: the walk is over those.
    // `fn` is a continuous-effect hook; it does not move objects.)
    for (auto& [sid, self] : state.objects.inPlay()) {
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

/// A keyword "if they didn't already" ("Friendly buffed units have [Deflect]
/// if they didn't already" — Spirit's Refuge): granted only where the object
/// has that keyword from no other source. The card pushes it unconditionally;
/// GameEngine::recalculateAuras drops it, after EVERY aura hook of the pass
/// has run (applyPassiveAura and applyMightDependentAura alike), from each
/// object that has the keyword printed, "this turn", from an Equipment or from
/// any other continuous effect — so the result does not depend on the order
/// the hooks ran in. Of several such effects on one object one is kept.
inline void auraKeywordIfAbsent(GameObject& obj, GameObjectId source, Keyword kw, int value = 0) {
    GameObject::AuraEffect ae;
    ae.source = source;
    ae.keyword = kw;
    ae.keyword_value = value;
    ae.only_if_absent = true;
    obj.aura_effects.push_back(ae);
}

// ── Board queries ───────────────────────────────────────────────────────────
// A list a card offers to an agent is built with boardObjects / chooseableUnits:
// both return ascending object ids, so the order of the options is a property
// of the helper and not of GameState::objects' hash order (which differs with
// the order the objects were created in).

/// Objects on the board matching `pred`, in ascending id order.
template <typename Pred>
std::vector<GameObjectId> boardObjects(const GameState& state, Pred&& pred) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : state.objects.onBoard())
        if (obj.location.has_value() && pred(obj)) out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

/// Runes `player` controls (in base, ready or exhausted).
inline int runesControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects.onBoard())
        if (obj.isRune() && obj.controller == player && obj.location.has_value()) ++n;
    return n;
}

/// Gear `player` controls on the board.
inline int gearControlled(const GameState& state, PlayerId player) {
    int n = 0;
    for (const auto& [id, obj] : state.objects.onBoard())
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
/// and abilities" for units `chooser` doesn't control. Ascending id order.
template <typename Pred>
std::vector<GameObjectId> chooseableUnits(const GameState& state, PlayerId chooser, Pred&& pred) {
    std::vector<GameObjectId> out;
    for (const auto& [id, obj] : state.objects.onBoard()) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.controller != chooser && obj.untargetable_by_enemy) continue;
        if (pred(obj)) out.push_back(id);
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// Other units `controller` has at the same location as `self`.
inline int otherFriendlyUnitsHere(const GameState& state, const GameObject& self) {
    if (!self.location.has_value()) return 0;
    int n = 0;
    for (const auto& [id, obj] : state.objects.atLocation(*self.location)) {
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
/// The "no" of a yes/no a card builds by hand: valChoice(p, 0, label) marked
/// as the decline (Intent::declines), so an agent can tell it from the first
/// of two modes. Card::confirmOptional does this itself.
inline Intent declineChoice(PlayerId p, const std::string& label) {
    Intent c = valChoice(p, 0, label);
    c.declines = true;
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
        ri.resume_data[slot + 1] = static_cast<int32_t>(id.raw());
        if (id == kInvalidId) Card::nothingToDecide(ri);
        return id;
    };
    if (ri.resume_data[slot] == 2) return GameObjectId::fromRaw(ri.resume_data[slot + 1]);
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
    RIFT_TRACE(ctx.events, "SELECT_PROMPT: " + label + " [" + std::to_string(options.size()) +
                        " options]");
    ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
    ri.resume_data[slot] = 1;
    return std::nullopt;
}

// ── "When you play me from face down" ───────────────────────────────────────
/// Whether `source`'s current play is a play from face down, this turn: the
/// stamps GameEngine::executePlayFromHidden writes as it plays a hidden card
/// (`__played_from_facedown`, `__facedown_play_turn`). They belong to one play:
/// Card::forgetPlayPayments drops them as every play begins, so a card played
/// from face down, returned and played from hand reads false (testing phase
/// B2). The one test for Edge of Night, Evelynn, Entrancing and Tornado
/// Warrior's trigger conditions; nothing else needs to clear it.
inline bool playedFromFacedown(const GameState& state, GameObjectId source) {
    if (!state.objectExists(source)) return false;
    const auto& c = state.getObject(source).card_counters;
    const auto played = c.find("__played_from_facedown");
    if (played == c.end() || played->second != 1) return false;
    const auto turn = c.find("__facedown_play_turn");
    return turn == c.end() || turn->second == state.turn.turn_number;
}

// ── Up to one target ("deal 2 to up to one enemy unit") ─────────────────────
/// A target that may be none (CR 355.13) — or, with `allow_none` false, a
/// second required target of an instruction of its own, where pickTarget is
/// already used — chosen like any target: an object
/// whose Deflect can't be paid is not offered, and choosing one pays its
/// Deflect and counts as choosing it (Card::commitChosenObject). Asked in a
/// finalize pass (the item's choices), with "None" offered after the objects.
/// Returns nullopt while the agent is being asked; kInvalidId for none (also
/// when nothing can be chosen — stored, so nothing is asked later); else the
/// object. As the item resolves, a chosen object no longer in `options`, or no
/// longer the same object (CR 359.3.e.4), comes back as kInvalidId: a
/// mistarget. Progress lives in resume_data[slot] and [slot + 1] (`slot` 7 or
/// more, as selectObject).
inline std::optional<GameObjectId> selectTargetOrNone(Card& card, CardContext& ctx,
                                                      const std::string& label,
                                                      std::vector<GameObjectId> options,
                                                      size_t slot = 7, bool allow_none = true) {
    std::sort(options.begin(), options.end());
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) return options.empty() ? kInvalidId : options.front();
    auto& ri = *ri_opt;
    while (ri.resume_data.size() < slot + 2) ri.resume_data.push_back(0);
    auto store = [&ri, slot](GameObjectId id) {
        ri.resume_data[slot] = 2;
        ri.resume_data[slot + 1] = static_cast<int32_t>(id.raw());
        if (id == kInvalidId) Card::nothingToDecide(ri);
        return id;
    };
    if (ri.resume_data[slot] == 2) {
        const auto id = GameObjectId::fromRaw(ri.resume_data[slot + 1]);
        if (id != kInvalidId && !ri.finalizing &&
            (std::find(options.begin(), options.end(), id) == options.end() ||
             !ctx.state.stillTheChosenObject(ri, id))) {
            RIFT_TRACE(ctx.events, "TGT_MISTARGET: " + label + " — the chosen target is no longer legal");
            return GameObjectId{kInvalidId};
        }
        return id;
    }
    const auto offered = card.offerableChoices(ctx, options);
    if (ri.resume_data[slot] == 1) {
        auto picked = ctx.executor.takeChoice();
        if (picked.has_value() && !picked->chosen_objects.empty() &&
            std::find(offered.begin(), offered.end(), picked->chosen_objects.front()) !=
                offered.end()) {
            const GameObjectId t = picked->chosen_objects.front();
            card.commitChosenObject(ctx, t);
            return store(t);
        }
        return store(kInvalidId);
    }
    if (offered.empty()) {
        if (!allow_none) Card::requiredChoiceMissing(ri);   // CR 403.4
        return store(kInvalidId);
    }
    std::vector<Intent> choices;
    for (auto id : offered) choices.push_back(objChoice(ctx.controller, id));
    if (allow_none) choices.push_back(valChoice(ctx.controller, 0, "None — " + label));
    RIFT_TRACE(ctx.events, "TGT_PROMPT: " + label + " [" + std::to_string(offered.size()) +
                        " options, or none]");
    ctx.executor.requestChoice(ctx.controller, std::move(choices), "target: " + label);
    ri.resume_data[slot] = 1;
    return std::nullopt;
}

// ── Split damage (CR 355.14) ────────────────────────────────────────────────
// "Deal N damage split among <units>". Two separate decisions, at the two
// moments the rules give them:
//
//   * WHO (CR 355.14.a-b): every unit that takes part is a Target, chosen when
//     the spell or ability is finalized on the chain — one prompt per target,
//     "No more targets" (offered first) from the second on, at most N of them (355.14.c).
//     Each is chosen like any target: a unit that can't be chosen is not
//     offered, Deflect (CR 809) is paid as it is picked, and it counts as
//     chosen. The opponent responds knowing the targets.
//   * HOW MUCH (CR 355.14.e): decided as the effect resolves. Every target
//     gets at least 1 (355.14.f-g); the rest is assigned one point per prompt,
//     so every legal division can be expressed. A target that is no longer
//     legal is dropped (CR 359.3.e.5); if the damage has shrunk below the
//     number of targets the controller says which cease to be targets
//     (355.14.h).
//
// The card opts its callback into choices at finalization (kFinalizeResolve /
// kFinalizeTrigger) and calls this helper where its choices end: the helper is
// the callback's end-of-choices marker (it calls Card::finalizingChoices), so
// the card needs none of its own for the split. Without a finalize pass (an
// item an effect put on the chain) both decisions are made at resolution.
//
// Progress lives on the resolving chain item: resume_point kSplit* (clear of
// the values the Card::pick* / confirmOptional helpers leave there) and
// resume_data = [number of targets, targets locked, target ..., one target per
// extra point assigned ...] — so a card that splits damage must not also call
// a Card::pick* helper in the same callback (they keep their answers in
// resume_data[0..6]).
constexpr int kSplitDamageTargets = 100;
constexpr int kSplitDamageTargetsAwait = 101;
constexpr int kSplitDamageDone = 102;
constexpr int kSplitDamageAssign = 103;
constexpr int kSplitDamageAssignAwait = 104;
constexpr int kSplitDamageCeaseAwait = 105;

/// Returns true once the damage has been dealt (or there was nothing to do),
/// false while a prompt is pending or the finalize pass has ended. `legal` are
/// the units the controller may choose right now; `total` the damage to divide
/// right now; `damaged` receives the units that were dealt damage.
/// `may_choose_none`: the text says "any number of" / "up to" units, so zero is
/// a legal choice (CR 355.13) — the first prompt then also offers "No more
/// targets", after the units (an agent that takes the first option still
/// damages something). Without it at least one target is chosen.
inline bool splitDamageAmong(CardContext& ctx, const std::string& label, int total,
                             GameObjectId damage_source, std::vector<GameObjectId> legal,
                             std::vector<GameObjectId>& damaged,
                             bool may_choose_none = false) {
    std::sort(legal.begin(), legal.end());
    auto dealAll = [&](const std::vector<std::pair<GameObjectId, int>>& alloc) {
        for (const auto& [id, n] : alloc) {
            if (n <= 0 || !ctx.state.objectExists(id)) continue;
            RIFT_TRACE(ctx.events, "SPLIT: " + label + " -> " + std::to_string(n) +
                                " to " + ctx.state.getObject(id).name);
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
        std::vector<std::pair<GameObjectId, int>> alloc;
        for (int i = 0; i < total; ++i) {
            const GameObjectId id = legal[i % legal.size()];
            auto it = std::find_if(alloc.begin(), alloc.end(),
                                   [id](const auto& a) { return a.first == id; });
            if (it == alloc.end()) alloc.emplace_back(id, 1); else ++it->second;
        }
        dealAll(alloc);
        return true;
    }
    auto& ri = *ri_opt;
    if (ri.resume_point == kSplitDamageDone) return true;
    const bool ours = ri.resume_point == kSplitDamageTargets ||
                      ri.resume_point == kSplitDamageTargetsAwait ||
                      ri.resume_point == kSplitDamageAssign ||
                      ri.resume_point == kSplitDamageAssignAwait ||
                      ri.resume_point == kSplitDamageCeaseAwait;
    if (!ours) {
        if (total <= 0 || legal.empty()) {
            // Nothing to choose: the item is marked done, which is this
            // helper's stored "nothing to decide" — a finalize pass may end
            // here (the rule is at Card::finalizingChoices).
            ri.resume_point = kSplitDamageDone;
            Card::nothingToDecide(ri);
            if (legal.empty() && !may_choose_none) Card::requiredChoiceMissing(ri);   // CR 403.4
            return true;
        }
        ri.resume_data = {0, 0};
        ri.resume_point = kSplitDamageTargets;
    }
    auto& data = ri.resume_data;
    auto targetCount = [&]() { return static_cast<size_t>(data[0]); };
    auto isTarget = [&](GameObjectId id) {
        for (size_t i = 0; i < targetCount(); ++i)
            if (GameObjectId::fromRaw(data[2 + i]) == id) return true;
        return false;
    };

    // ── Who: the targets (at finalization when there is a finalize pass) ──
    if (data[1] == 0) {
        // Units not yet targeted whose Deflect the controller can pay.
        auto offered = [&]() {
            const int payable = ctx.executor.payableAnyPower(ctx.controller);
            std::vector<GameObjectId> out;
            for (auto t : legal)
                if (!isTarget(t) && ctx.executor.deflectCostFor(ctx.controller, t) <= payable)
                    out.push_back(t);
            return out;
        };
        bool stop = false;
        if (ri.resume_point == kSplitDamageTargetsAwait) {
            const GameObjectId t = takenObject(ctx);
            const auto can_pick = offered();
            if (t != kInvalidId &&
                std::find(can_pick.begin(), can_pick.end(), t) != can_pick.end()) {
                const int cost = ctx.executor.deflectCostFor(ctx.controller, t);
                if (cost > 0) {
                    ctx.executor.payAnyPower(ctx.controller, cost);
                    RIFT_TRACE(ctx.events, "DEFLECT: pays " + std::to_string(cost) +
                                        " [A] to choose " + ctx.state.getObject(t).name);
                }
                ri.chosen_objects.push_back(t);
                // "If you've chosen an enemy unit this turn" (as Card::pickTarget).
                ctx.state.noteChosen(ctx.controller, t, &ri);
                data.insert(data.begin() + 2 + static_cast<long>(targetCount()),
                            static_cast<int32_t>(t.raw()));
                ++data[0];
                RIFT_TRACE(ctx.events, "SPLIT_TARGET: " + label + " -> " +
                                    ctx.state.getObject(t).name);
            } else {
                stop = true;   // "No more targets"
            }
            ri.resume_point = kSplitDamageTargets;
        }
        const auto can_pick = offered();
        if (!stop && static_cast<int>(targetCount()) < total && !can_pick.empty()) {
            std::vector<Intent> options;
            options.reserve(can_pick.size() + 1);
            // "No more targets" first, as "Move no more units" is: an agent
            // that takes the first option stops at one target.
            if (targetCount() >= 1)
                options.push_back(valChoice(ctx.controller, 0, "No more targets"));
            for (auto t : can_pick) options.push_back(objChoice(ctx.controller, t));
            if (targetCount() == 0 && may_choose_none)
                options.push_back(valChoice(ctx.controller, 0, "No more targets"));
            RIFT_TRACE(ctx.events, "SPLIT_PROMPT: " + label + " (target " +
                                std::to_string(targetCount() + 1) + ")");
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                                       "split damage: " + label + " — choose a target");
            ri.resume_point = kSplitDamageTargetsAwait;
            return false;
        }
        data[1] = 1;   // locked
        ri.resume_point = kSplitDamageAssign;
    }

    // ── End of the choices made at finalization (CR 355.14.b) ──
    if (Card::finalizingChoices(ctx)) return false;

    // ── How much: the division (at resolution, CR 355.14.e) ──
    // Targets that are still legal, in the order they were chosen.
    std::vector<GameObjectId> live;
    for (size_t i = 0; i < targetCount(); ++i) {
        const auto t = GameObjectId::fromRaw(data[2 + i]);
        if (t != kInvalidId && std::find(legal.begin(), legal.end(), t) != legal.end())
            live.push_back(t);
    }
    if (total <= 0 || live.empty()) {
        ri.resume_point = kSplitDamageDone;
        return true;
    }
    // CR 355.14.h: more targets than damage — the controller says which cease
    // to be targets, one at a time, down to one target per point.
    if (ri.resume_point == kSplitDamageCeaseAwait) {
        const GameObjectId t = takenObject(ctx);
        const GameObjectId drop =
            std::find(live.begin(), live.end(), t) != live.end() ? t : live.back();
        for (size_t i = 0; i < targetCount(); ++i)
            if (GameObjectId::fromRaw(data[2 + i]) == drop)
                data[2 + i] = static_cast<int32_t>(kInvalidId.raw());
        live.erase(std::find(live.begin(), live.end(), drop));
        ri.resume_point = kSplitDamageAssign;
    }
    if (static_cast<int>(live.size()) > total) {
        std::vector<Intent> options;
        for (auto t : live) options.push_back(objChoice(ctx.controller, t));
        ctx.executor.requestChoice(ctx.controller, std::move(options),
            "split damage: " + label + " — more targets than damage: which one is dropped");
        ri.resume_point = kSplitDamageCeaseAwait;
        return false;
    }
    // Every target gets 1; each further point goes where the controller says.
    const size_t extras_at = 2 + targetCount();
    if (ri.resume_point == kSplitDamageAssignAwait) {
        const GameObjectId t = takenObject(ctx);
        data.push_back(static_cast<int32_t>(
            (std::find(live.begin(), live.end(), t) != live.end() ? t : live.front()).raw()));
        ri.resume_point = kSplitDamageAssign;
    }
    std::vector<std::pair<GameObjectId, int>> alloc;
    for (auto t : live) alloc.emplace_back(t, 1);
    int remaining = total - static_cast<int>(live.size());
    if (live.size() == 1) {
        alloc.front().second += remaining;   // nothing to decide
        remaining = 0;
    }
    for (size_t i = extras_at; i < data.size() && remaining > 0; ++i) {
        const auto t = GameObjectId::fromRaw(data[i]);
        auto it = std::find_if(alloc.begin(), alloc.end(),
                               [t](const auto& a) { return a.first == t; });
        if (it == alloc.end()) it = alloc.begin();   // that target has since dropped out
        ++it->second;
        --remaining;
    }
    if (remaining > 0) {
        std::vector<Intent> options;
        options.reserve(live.size());
        for (auto t : live) options.push_back(objChoice(ctx.controller, t));
        RIFT_TRACE(ctx.events, "SPLIT_PROMPT: " + label + " (" + std::to_string(remaining) +
                            " of " + std::to_string(total) + " damage left to assign)");
        ctx.executor.requestChoice(ctx.controller, std::move(options),
                                   "split damage: " + label + " (" +
                                   std::to_string(remaining) + " left)");
        ri.resume_point = kSplitDamageAssignAwait;
        return false;
    }
    ri.resume_point = kSplitDamageDone;
    dealAll(alloc);
    return true;
}

/// True when `u` may not MOVE to base: it is at a battlefield that says
/// "Units can't move from here to base" (Vilemaw's Lair), or the unit itself
/// can't ("I can't move to base" — Determined Sentry; "Units can't move to
/// base" — Minotaur Reckoner's aura). The same three conditions the Standard
/// Move generator reads (GameEngine::standardMoveDestinations). Only a move
/// is stopped; a recall is not a move (CR 458).
inline bool moveToBaseBlocked(const GameState& state, const GameObject& u) {
    if (u.cant_move_to_base || u.aura_cant_move_to_base) return true;
    const auto here = u.battlefieldId();
    if (!here.has_value()) return false;
    for (const auto& bf : state.battlefields)
        if (bf.id == *here) return bf.blocks_move_to_base;
    return false;
}

/// Whether the resolving effect of `mover` can move unit `u` to its base at
/// all — what EffectExecutor::moveToBase refuses: "can't move to base"
/// (moveToBaseBlocked), "can't be moved by enemy spells and abilities", "they
/// can't move it this turn". A card that offers units to move to base offers
/// only these, so a move that is offered happens.
inline bool effectCanMoveToBase(const GameState& state, PlayerId mover, const GameObject& u) {
    if (!u.isUnit() || !u.location.has_value() || u.isAtBase()) return false;
    if (moveToBaseBlocked(state, u)) return false;
    if (u.cant_be_moved_by_enemy && mover != u.controller) return false;
    if (u.cant_move_this_turn && mover == u.controller) return false;
    return true;
}

// ── Where an effect plays a unit (CR 355.2.a) ───────────────────────────────
// A unit played by an effect goes where a unit played from hand may go: its
// controller's base or a battlefield they control. pickEffectPlayLocation
// asks through Card::pickMode (mode 0 = base, mode 1 + i =
// state.battlefields[i]), so it composes with confirmOptional / pickTarget
// and can be asked at finalization — but a card that calls it must not use
// pickMode for anything else in the same callback. With no battlefield to
// offer there is nothing to choose and nothing is asked — then or later.

/// Returns -1 while waiting for the agent, else the mode to hand to
/// effectPlayLocation.
inline int pickEffectPlayLocation(Card& card, CardContext& ctx, const std::string& label) {
    const auto& bfs = ctx.state.battlefields;
    const bool base_only = ctx.state.player(ctx.controller).units_play_base_only;
    uint32_t mask = 1u;
    std::vector<std::string> labels{"Base"};
    for (size_t i = 0; i < bfs.size() && i < 31; ++i) {
        labels.push_back("Battlefield #" + toString(bfs[i].id));
        if (!base_only && !bfs[i].blocks_unit_play && bfs[i].controller.has_value() &&
            *bfs[i].controller == ctx.controller)   // as battlefieldTakesUnitsFrom
            mask |= 1u << (i + 1);
    }
    if (mask == 1u) {
        // Nothing to choose: the base. Stored as the answer, so the question
        // is not asked later — as the item resolves — should a battlefield
        // have come under its controller's control by then.
        card.settleMode(ctx, 0);
        return 0;
    }
    const int m = card.pickMode(ctx, label, static_cast<int>(labels.size()), labels, mask);
    return m == -1 ? -1 : std::max(0, m);
}
/// The location a pickEffectPlayLocation mode stands for; the base if that
/// battlefield is no longer the player's to play to.
inline LocationId effectPlayLocation(const GameState& state, PlayerId player, int mode) {
    const auto& bfs = state.battlefields;
    if (mode >= 1 && static_cast<size_t>(mode) <= bfs.size() &&
        !state.player(player).units_play_base_only) {
        const auto& bf = bfs[mode - 1];
        if (!bf.blocks_unit_play && bf.controller.has_value() && *bf.controller == player)
            return BattlefieldLocation{bf.id};
    }
    return BaseLocation{player};
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
    return BattlefieldId::fromRaw(bf->second - 1);
}

// ── A hidden card's choices are bound to its battlefield (CR 811.1.d) ───────
// One rule, read by every helper below that offers a location for a unit and
// by any card whose pick list is of board objects:
//   - a hidden SPELL chooses among the options at the battlefield it was
//     played from (811.1.d.2);
//   - a play effect of a hidden PERMANENT chooses among the options at the
//     battlefield the permanent was played to (811.1.d.3) — where it is now;
//     once the permanent has left the board the effect can no longer tell and
//     has no legal option (811.1.d.3.a);
//   - a unit either of them plays is played at that battlefield (811.1.d.5).
// "Unless it explicitly restricts targeting in a way that makes this
// impossible" (Tideturner: "a unit you control at another location") is the
// card's to say: such a card does not call these.
struct HiddenBinding {
    bool bound = false;                    // this play of the card is from facedown
    std::optional<BattlefieldId> bf;       // the battlefield; none = no option anywhere
};
inline HiddenBinding hiddenBinding(const GameState& state, GameObjectId source) {
    HiddenBinding out;
    const auto hidden_at = facedownPlayBattlefield(state, source);
    if (!hidden_at.has_value()) return out;
    out.bound = true;
    const auto& card = state.getObject(source);
    if (card.isSpell()) out.bf = hidden_at;
    else out.bf = card.battlefieldId();    // played to; nullopt once it has left
    return out;
}
/// `options` as a card played from facedown may choose from them: only the
/// board objects at its battlefield. Unchanged for any other play. For the
/// pick list of a hidden spell that chooses as it resolves, and of a hidden
/// permanent's "When you play me".
inline std::vector<GameObjectId> atHiddenBattlefield(const GameState& state, GameObjectId source,
                                                    std::vector<GameObjectId> options) {
    const HiddenBinding bind = hiddenBinding(state, source);
    if (!bind.bound) return options;
    options.erase(std::remove_if(options.begin(), options.end(), [&](GameObjectId id) {
        if (!bind.bf.has_value() || !state.objectExists(id)) return true;
        const auto at = state.getObject(id).battlefieldId();
        return !at.has_value() || *at != *bind.bf;
    }), options.end());
    return options;
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
/// Whether a unit `player` plays may go to `bf` at all, whoever controls it:
/// not where "Units can't be played here." (BattlefieldState::blocks_unit_play
/// — Rockfall Path), and to no battlefield for a player who may only play
/// units to base (PlayerState::units_play_base_only — Mageseeker Warden).
/// Every helper below that offers a battlefield for a unit asks this, so a
/// card that uses them needs no mask of its own.
inline bool battlefieldTakesUnitsFrom(const GameState& state, PlayerId player,
                                      const BattlefieldState& bf) {
    return !bf.blocks_unit_play && !state.player(player).units_play_base_only;
}

/// `only_bf`: the spell was played from facedown at that battlefield, so the
/// unit must be played there (CR 811.1.d.5) — and nowhere when units can't be
/// played there: the rule restricts the choice (CR 811.1.d), it does not lift
/// a card's "Units can't be played here." (CR 002, 054.1), and two
/// restrictions that contradict leave no location (Vendetta FAQ, Perched
/// Grimwyrm at Rockfall Path). `include_base` = false for text that names "a
/// battlefield you control". An empty result means the unit is not played.
inline PlayLocations playLocations(const GameState& state, PlayerId player,
                                   std::optional<BattlefieldId> only_bf = std::nullopt,
                                   bool include_base = true) {
    PlayLocations out;
    auto bfName = [&state](const BattlefieldState& bf) {
        return state.objectExists(bf.card_object_id)
            ? state.getObject(bf.card_object_id).name
            : "Battlefield #" + std::to_string(static_cast<int>(bf.id.raw()));
    };
    if (only_bf.has_value()) {
        for (const auto& bf : state.battlefields)
            if (bf.id == *only_bf && battlefieldTakesUnitsFrom(state, player, bf)) {
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
        if (!battlefieldTakesUnitsFrom(state, player, bf)) continue;
        out.locations.push_back(LocationId{BattlefieldLocation{bf.id}});
        out.labels.push_back(bfName(bf));
    }
    return out;
}

// ── A target locked in as the spell was played ──────────────────────────────
/// For a spell that takes its single target through Card::pickTarget
/// (needsPlayTimeTarget): the target it was played with — from hand by the
/// engine's play-time choice, from facedown by the play itself, among the
/// objects at that battlefield (CR 811.1.d.2). The engine hands it to
/// onResolve in `targets` while it is still legal. Returns that target;
/// kInvalidId if one was locked and is no longer legal (a mistarget);
/// std::nullopt if none was locked, and the card asks with pickTarget:
///
///     auto locked = lockedPlayTarget(ctx, targets);
///     GameObjectId t = locked ? *locked : pickTarget(ctx, label, legal);
inline std::optional<GameObjectId> lockedPlayTarget(const CardContext& ctx,
                                                    const std::vector<GameObjectId>& targets) {
    if (!targets.empty()) return targets[0];
    const auto& ri = ctx.state.chain.resuming;
    if (ri.has_value() && !ri->targets.empty()) return kInvalidId;
    return std::nullopt;
}

// ── Playing several unit tokens (CR 355.2.a) ────────────────────────────────
// "Play N ... unit tokens" with no location printed: each token goes to its
// controller's base or a battlefield they control, chosen token by token.
// `make(location)` creates one token there and returns its id. The card is
// re-entered after every answer, so progress lives on the resolving chain
// item: resume_point kTokenPlayAwait while a location is being asked, and
// resume_data[kTokenPlaySlot] = tokens played so far followed by their ids
// (clear of the slots the Card::pick* helpers use). Returns true once all
// `count` tokens are played (`made` holds them), false while waiting. When
// there is nothing to choose (no battlefield controlled, or no chain item to
// resume) the tokens go to the only / first location without a prompt.
constexpr int kTokenPlayAwait = 110;
constexpr size_t kTokenPlaySlot = 8;
template <typename MakeFn>
bool playTokensChoosingLocations(CardContext& ctx, const std::string& label, int count,
                                 MakeFn&& make, std::vector<GameObjectId>& made,
                                 std::optional<BattlefieldId> only_bf = std::nullopt) {
    made.clear();
    const auto where = playLocations(ctx.state, ctx.controller, only_bf);
    if (count <= 0 || where.count() == 0) return true;
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value()) {
        for (int i = 0; i < count; ++i) made.push_back(make(where.locations[0]));
        return true;
    }
    auto& data = ri_opt->resume_data;
    while (data.size() <= kTokenPlaySlot) data.push_back(0);
    auto collect = [&]() {
        made.clear();
        for (size_t i = kTokenPlaySlot + 1; i < data.size(); ++i)
            made.push_back(GameObjectId::fromRaw(data[i]));
    };
    // The controller chose among `where.locations`: an additional copy a
    // replacement plays with a token (Zilean, Time Mage) gets the same choice.
    // `made` lists the tokens the card asked for; a card that acts on every
    // token played reads EffectExecutor::lastTokensCreated after each make.
    auto play_one = [&](int option) {
        if (option < 0 || option >= where.count()) option = 0;
        ctx.executor.setTokenLocationChoices(where.locations);
        const GameObjectId tok = make(where.locations[option]);
        ctx.executor.clearTokenLocationChoices();
        data.push_back(static_cast<int32_t>(tok.raw()));
        ++data[kTokenPlaySlot];
    };
    if (ri_opt->resume_point == kTokenPlayAwait) {
        play_one(takenValue(ctx, 0));
        ri_opt->resume_point = 0;
    }
    while (data[kTokenPlaySlot] < count) {
        if (where.count() > 1) {
            std::vector<Intent> options;
            for (int i = 0; i < where.count(); ++i)
                options.push_back(valChoice(ctx.controller, i, label + ": " + where.labels[i]));
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                label + " (token " + std::to_string(data[kTokenPlaySlot] + 1) + " of " +
                std::to_string(count) + ")");
            ri_opt->resume_point = kTokenPlayAwait;
            return false;
        }
        play_one(0);
    }
    collect();
    return true;
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
        out.push_back(1 + static_cast<int>(bf.id.raw()));
    }
    return out;
}
inline std::string destLabel(int d) {
    return d <= 0 ? std::string("Base") : "Battlefield #" + std::to_string(d - 1);
}
/// Move `u` to a moveDests destination.
inline void moveTo(CardContext& ctx, GameObjectId u, int dest) {
    if (dest <= 0) ctx.executor.moveToBase(u);
    else ctx.executor.moveToBattlefield(u, BattlefieldId::fromRaw(dest - 1));
}
/// Where `player` may put a unit token an effect plays ("They can be played
/// to your base or to battlefields you control"), as moveDests codes: 0 =
/// their base, 1 + i = battlefield i. A battlefield units can't be played to
/// is left out (battlefieldTakesUnitsFrom).
inline std::vector<int> tokenPlayDests(const GameState& state, PlayerId player) {
    std::vector<int> out{0};
    for (const auto& bf : state.battlefields)
        if (bf.controller.has_value() && *bf.controller == player &&
            battlefieldTakesUnitsFrom(state, player, bf))
            out.push_back(1 + static_cast<int>(bf.id.raw()));
    return out;
}
/// The same for the token a resolving card plays: when that card was played
/// from facedown the token must be played at its battlefield (CR 811.1.d.5,
/// hiddenBinding) — one destination, or none when units can't be played
/// there (the rule restricts the choice; it does not lift "Units can't be
/// played here.": playLocations has the citation).
inline std::vector<int> tokenPlayDests(const CardContext& ctx) {
    const HiddenBinding bind = hiddenBinding(ctx.state, ctx.source);
    if (!bind.bound) return tokenPlayDests(ctx.state, ctx.controller);
    std::vector<int> out;
    if (bind.bf.has_value())
        for (const auto& bf : ctx.state.battlefields)
            if (bf.id == *bind.bf && battlefieldTakesUnitsFrom(ctx.state, ctx.controller, bf))
                out.push_back(1 + static_cast<int>(bf.id.raw()));
    return out;
}
/// The location a moveDests / tokenPlayDests code stands for.
inline LocationId destLocation(PlayerId player, int dest) {
    if (dest <= 0) return LocationId{BaseLocation{player}};
    return LocationId{BattlefieldLocation{BattlefieldId::fromRaw(dest - 1)}};
}
/// tokenPlayDests as locations: what a card that asked with pickTokenDest
/// passes to createToken as `location_choices`.
inline std::vector<LocationId> tokenPlayLocations(const GameState& state, PlayerId player) {
    std::vector<LocationId> out;
    for (int d : tokenPlayDests(state, player)) out.push_back(destLocation(player, d));
    return out;
}
/// tokenPlayDests(ctx) as locations: the `location_choices` of a card that
/// asked with pickTokenDest.
inline std::vector<LocationId> tokenPlayLocations(const CardContext& ctx) {
    std::vector<LocationId> out;
    for (int d : tokenPlayDests(ctx)) out.push_back(destLocation(ctx.controller, d));
    return out;
}
/// Ask ctx.controller where a unit token they are playing goes (one of
/// tokenPlayDests(ctx), so a card played from facedown is held to its
/// battlefield). Returns the code; -1 while the agent is deciding and -2 when
/// there is nowhere the token can be played (the caller must return for
/// both: `if (dest < 0) return;`). Not asked when there is one place only.
/// This is the callback's Card::pickMode.
inline int pickTokenDest(Card& card, CardContext& ctx, const std::string& label) {
    const auto dests = tokenPlayDests(ctx);
    if (dests.empty()) return -2;
    if (dests.size() == 1) {
        // Nothing to choose. Stored as the answer, so it is not asked later —
        // as the item resolves — should another place have opened by then.
        card.settleMode(ctx, dests.front());
        return dests.front();
    }
    uint32_t mask = 0;
    int modes = 1;
    for (int d : dests) {
        if (d < 32) mask |= 1u << d;
        modes = std::max(modes, d + 1);
    }
    std::vector<std::string> labels;
    for (int d = 0; d < modes; ++d) labels.push_back(destLabel(d));
    return card.pickMode(ctx, label, modes, labels, mask);
}

/// "Move a unit" with no destination printed: ask ctx.controller where `u`
/// goes — one of moveDests, or only the battlefields among them with
/// `battlefields_only` ("move it to a battlefield"). Returns the moveDests
/// code, -1 while the agent is deciding (the caller must return) and -2 when
/// there is nowhere to move it. This is the callback's Card::pickMode.
inline int pickMoveDest(Card& card, CardContext& ctx, const std::string& label,
                        const GameObject& u, bool battlefields_only = false) {
    uint32_t mask = 0;
    for (int d : moveDests(ctx.state, u))
        if (d < 32 && !(d == 0 && battlefields_only)) mask |= 1u << d;
    int modes = 1;
    for (const auto& bf : ctx.state.battlefields)
        modes = std::max(modes, 2 + static_cast<int>(bf.id.raw()));
    std::vector<std::string> labels;
    for (int d = 0; d < modes; ++d) labels.push_back(destLabel(d));
    return card.pickMode(ctx, label, modes, labels, mask);
}

// ── Several objects, one prompt each ("up to N", "any number of") ───────────
/// "Buff up to four friendly units", "spend any number of buffs", "move any
/// number of your token units": one selectObject prompt per pick, each with a
/// "None" option that ends the list, until the controller stops, `max_picks`
/// are made (-1 = no limit: "any number") or nothing is left to pick. An
/// object listed k times in `options` can be picked k times (a unit with two
/// buffs to spend). For objects the controller picks without "choosing" them
/// in the rules sense — no Deflect, no "chosen" bookkeeping (selectObject).
///
/// Returns the picks in pick order (possibly none), or nullopt while a prompt
/// is pending. It keeps its answers in two resume_data slots per pick from
/// `first_slot` on and does not touch resume_point, so it composes with the
/// Card::pick* helpers and confirmOptional in one callback. There is no cap
/// of its own: the re-entry bound per chain item is 1024.
///
/// Re-entry. The caller passes the list as it is NOW every time it is entered
/// — after each answer, and at resolution after a finalize pass. A pick that
/// is already stored is never asked again and is not re-derived from the
/// list; the list only says whether it still counts:
///   - a stored pick that is in the current list is returned (and uses up one
///     instance of that object for the prompts that follow);
///   - a stored pick that is NOT in the current list (it left the board, was
///     readied, lost its buff, ... since it was named) is DROPPED from the
///     result: the vector is shorter, no kInvalidId is put in its place and no
///     other object is asked for instead. So every id returned is in the
///     caller's current list, and "is it still legal" is the list's predicate.
/// Once the picks have ended — the controller said "None", `max_picks` were
/// made, or nothing was left to pick — that is stored too, so a later entry
/// with a longer list (a finalized ability resolving after something new
/// became eligible) asks nothing and names nothing new. A prompt whose pick
/// has not been stored yet is offered what the current list still holds.
inline std::optional<std::vector<GameObjectId>> selectSeveral(
        CardContext& ctx, const std::string& label, std::vector<GameObjectId> options,
        int max_picks = -1, size_t first_slot = 7) {
    std::sort(options.begin(), options.end());
    std::vector<GameObjectId> picked;
    const auto& item = ctx.state.chain.resuming;
    for (int k = 0; max_picks < 0 || k < max_picks; ++k) {
        const size_t slot = first_slot + 2 * static_cast<size_t>(k);
        if (item.has_value() && item->resume_data.size() > slot + 1 &&
            item->resume_data[slot] == 2) {
            // Answered on an earlier entry: take the stored pick as it is.
            const auto stored = GameObjectId::fromRaw(item->resume_data[slot + 1]);
            if (stored == kInvalidId) break;              // "None" / nothing was left
            auto it = std::find(options.begin(), options.end(), stored);
            if (it == options.end()) continue;            // no longer in the list: dropped
            picked.push_back(stored);
            options.erase(it);                            // one instance
            continue;
        }
        if (options.empty()) {
            // Nothing left to pick: store the end of the list (no prompt), so
            // a later entry does not start asking again.
            if (item.has_value()) (void)selectObject(ctx, label, {}, slot, /*may_decline=*/true);
            break;
        }
        std::vector<GameObjectId> offer = options;
        offer.erase(std::unique(offer.begin(), offer.end()), offer.end());
        auto one = selectObject(ctx, label, offer, slot, /*may_decline=*/true);
        if (!one.has_value()) return std::nullopt;
        if (*one == kInvalidId) break;
        picked.push_back(*one);
        options.erase(std::find(options.begin(), options.end(), *one));   // one instance
    }
    return picked;
}

/// selectSeveral for objects that are CHOSEN in the rules sense — targets
/// ("move any number of friendly units at a battlefield", "return up to two
/// cards from your trash": CR 355.5, 355.13): each one is offered only if it
/// can be chosen (Card::offerableChoices: its Deflect payable, and while a
/// spell chooses before its cost only what leaves it payable) and choosing it
/// pays its Deflect and counts as choosing it (Card::commitChosenObject:
/// "when you choose a friendly unit", "your spells that choose me cost
/// less"). Same slots and re-entry contract as selectSeveral.
inline std::optional<std::vector<GameObjectId>> selectSeveralTargets(
        Card& card, CardContext& ctx, const std::string& label,
        std::vector<GameObjectId> options, int max_picks = -1, size_t first_slot = 7) {
    std::sort(options.begin(), options.end());
    std::vector<GameObjectId> picked;
    const auto& item = ctx.state.chain.resuming;
    for (int k = 0; max_picks < 0 || k < max_picks; ++k) {
        const size_t slot = first_slot + 2 * static_cast<size_t>(k);
        if (item.has_value() && item->resume_data.size() > slot + 1 &&
            item->resume_data[slot] == 2) {
            const auto stored = GameObjectId::fromRaw(item->resume_data[slot + 1]);
            if (stored == kInvalidId) break;
            auto it = std::find(options.begin(), options.end(), stored);
            if (it == options.end() || !ctx.state.stillTheChosenObject(*item, stored))
                continue;                                  // no longer legal: dropped
            picked.push_back(stored);
            options.erase(it);
            continue;
        }
        std::vector<GameObjectId> offer = options;
        offer.erase(std::unique(offer.begin(), offer.end()), offer.end());
        const bool answering = item.has_value() && item->resume_data.size() > slot &&
                               item->resume_data[slot] == 1;
        if (!answering) offer = card.offerableChoices(ctx, offer);
        if (offer.empty()) {
            if (item.has_value()) (void)selectObject(ctx, label, {}, slot, /*may_decline=*/true);
            break;
        }
        auto one = selectObject(ctx, label, offer, slot, /*may_decline=*/true);
        if (!one.has_value()) return std::nullopt;
        if (*one == kInvalidId) break;
        if (answering) card.commitChosenObject(ctx, *one);   // chosen now: Deflect, "chosen"
        picked.push_back(*one);
        options.erase(std::find(options.begin(), options.end(), *one));
    }
    return picked;
}

/// "Gain N XP." Every card effect that gives XP goes through here so
/// `xp_gained_this_turn` ("If you've gained XP this turn") is kept.
inline void gainXp(CardContext& ctx, PlayerId player, int amount) {
    if (amount <= 0) return;
    auto& ps = ctx.state.player(player);
    ps.xp += amount;
    ps.xp_gained_this_turn += amount;
    RIFT_TRACE(ctx.events, "XP: " + std::string(toString(player)) + " gains " +
                        std::to_string(amount) + " XP (now " + std::to_string(ps.xp) + ")");
}

// ── A pick made by a player other than the controller ───────────────────────
/// selectObject for a choice that belongs to `chooser`, who need not be
/// ctx.controller: "each player must kill one of their units", "the defender
/// must kill one of their units here". The pick is mandatory. Same contract
/// and slots as selectObject (nullopt while `chooser` is being asked,
/// kInvalidId when there is nothing to pick); a card that asks both players
/// uses slots two apart.
inline std::optional<GameObjectId> selectObjectAs(CardContext& ctx, PlayerId chooser,
                                                  const std::string& label,
                                                  std::vector<GameObjectId> options,
                                                  size_t slot = 7) {
    std::sort(options.begin(), options.end());
    auto& ri_opt = ctx.state.chain.resuming;
    if (!ri_opt.has_value())   // direct invocation: nobody to ask
        return options.empty() ? kInvalidId : options.front();
    auto& ri = *ri_opt;
    while (ri.resume_data.size() < slot + 2) ri.resume_data.push_back(0);
    auto answer = [&ri, slot](GameObjectId id) {
        ri.resume_data[slot] = 2;
        ri.resume_data[slot + 1] = static_cast<int32_t>(id.raw());
        if (id == kInvalidId) Card::nothingToDecide(ri);
        return id;
    };
    if (ri.resume_data[slot] == 2) return GameObjectId::fromRaw(ri.resume_data[slot + 1]);
    if (ri.resume_data[slot] == 1) {
        const GameObjectId picked = takenObject(ctx);
        if (std::find(options.begin(), options.end(), picked) != options.end())
            return answer(picked);
        return answer(options.empty() ? kInvalidId : options.front());
    }
    if (options.empty()) return answer(kInvalidId);
    if (options.size() == 1) return answer(options.front());
    std::vector<Intent> choices;
    choices.reserve(options.size());
    for (auto id : options) choices.push_back(objChoice(chooser, id));
    RIFT_TRACE(ctx.events, "SELECT_PROMPT (" + std::string(toString(chooser)) + "): " + label +
                        " [" + std::to_string(options.size()) + " options]");
    ctx.executor.requestChoice(chooser, std::move(choices), label);
    ri.resume_data[slot] = 1;
    return std::nullopt;
}

} // namespace riftbound

// The Equip base classes (SimpleEquipGear / UniversalEquipGear) are built on
// the helpers above; included last so a card file that includes only this
// header still gets them.
#include "cards/gear/equip_base.h"
