// Audit-fix batch 2 — manual override cards.
//
// One class per card, prefixed AF2_ so the symbols are globally unique
// across all audit_fixes_*.cpp translation units (they all link into one
// binary). Registered LAST via registerAuditFixes2(), so these overrides
// win over both generated stubs and earlier manual classes for the same
// card ids.
//
// Cards in this batch:
//   29  Falling Star          (spell)        — two independent "Deal 3" targets
//   153 Overt Operation       (spell)        — spend buffs to ready + mass buff
//   365 Brutalizer            (equip gear)   — +2 [M] if attached this turn
//   435 Lucian, Merciless     (unit)         — Weaponmaster + first-conquer ready
//   471 Last Rites            (equip gear)   — optional play-from-trash on score
//   531 Seat of Power         (battlefield)  — draw 1 per other controlled BF
//   606 Flurry of Feathers    (spell)        — modal: counter OR 4 Bird tokens
//   640 Sprite Fountain       (gear)         — play effect + Deathknell repeat
//   657 Grim Resolve          (spell)        — +3 [M] + win-combat XP rider
//   720 Shepherd's Heirloom   (equip gear)   — gain 1 XP + [Equip] Spend 1 XP
//   764 Black Flame Altar     (battlefield)  — Temporary units here have Shield

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {

// ─── Local equip helper (self-contained copy of standardEquip) ──────────────
//
// The shared standardEquip()/SimpleEquipGear live in equip_cards.cpp and are
// file-local there, so we re-implement the "pay [N] energy + recycle one
// matching-domain rune for power, then attach" sequence here. Pre-checks both
// portions before mutating any state (an unaffordable equip must not silently
// recycle runes).
static bool af2_standardEquip(CardContext& ctx, GameObjectId gear_id,
                              GameObjectId unit_id, int energy_cost,
                              Domain domain) {
    auto& state = ctx.state;
    auto player = ctx.controller;
    auto& ps = state.player(player);
    auto base_loc = BaseLocation{player};

    int ready_count = 0;
    GameObjectId domain_rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        if (!obj.is_exhausted) ready_count++;
        if (domain_rune == kInvalidId) {
            for (auto d : obj.domains) {
                if (d == domain) { domain_rune = id; break; }
            }
        }
    }
    if (ready_count < energy_cost) return false;
    if (domain_rune == kInvalidId) return false;

    if (energy_cost > 0) {
        int e_remaining = energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (e_remaining <= 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            e_remaining--;
        }
    }

    {
        auto& dr = state.getObject(domain_rune);
        ctx.events.logTrace("  EQUIP_COST: recycled " + dr.name + " for power");
        dr.location = std::nullopt;
        dr.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), domain_rune);
    }

    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit.name);

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

// ─── [29] Falling Star ──────────────────────────────────────────────────────
// "Deal 3 to a unit. Deal 3 to a unit." — two SEPARATE target selections
// (may be the same unit or two different units). The generated stub read
// targets[0] twice (so it could only ever hit one unit). We defer both
// picks to resolve time via pickTargetPair: legal_a = any unit, legal_b =
// any unit (the same unit may be chosen again — both clauses say "a unit"
// with no exclusion, so doubling up on one unit is legal).
class AF2_FallingStar : public SpellCard {
public:
    AF2_FallingStar() : SpellCard(29) {}

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto all_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.location.has_value()) out.push_back(id);
            }
            return out;
        };

        auto [a, b] = pickTargetPair(
            ctx, "Falling Star",
            all_units(),
            [&](GameObjectId /*picked_a*/) { return all_units(); });

        // Suspend detection (mirrors MStarCrossed): if a pick is invalid AND
        // we're parked at a prompt-publish resume_point (10 or 12), the chain
        // manager will re-enter after the agent decides — return now without
        // applying anything.
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;

        auto hit = [&](GameObjectId t) {
            if (t == kInvalidId || !ctx.state.objectExists(t)) return;
            ctx.executor.dealDamage(t, 3, ctx.source);
            if (ctx.state.objectExists(t) &&
                ctx.state.getObject(t).hasLethalDamage()) {
                ctx.executor.killObject(t);
            }
        };
        hit(a);
        hit(b);
    }
};

// ─── [153] Overt Operation ────────────────────────────────────────────────
// "[Action] For each friendly unit, you may spend its buff to ready it. Then
//  buff all friendly units. (Each one that doesn't have a buff gets a +1 [M]
//  buff.)"
//
// The per-unit "you may spend its buff to ready it" is modeled as ONE batched
// confirmOptional (the resumable resume-slot scheme reserves a fixed set of
// slots, so it can't drive N independent per-unit prompts in a single
// onResolve). If the agent confirms, every friendly unit that currently has a
// buff spends one buff (-1 [M]) and is readied. Then every friendly unit is
// buffed (+1 [M]) per the engine's buff convention.
class AF2_OvertOperation : public SpellCard {
public:
    AF2_OvertOperation() : SpellCard(153) {}

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value()) {
                    out.push_back(id);
                }
            }
            return out;
        };

        // "you may spend its buff to ready it" — gate the whole spend-to-ready
        // step on one agent decision; legal only if some friendly unit
        // actually has a buff to spend.
        int answer = confirmOptional(ctx, "Overt Operation: spend buffs to ready",
            [&]() {
                for (auto id : friendly_units()) {
                    if (ctx.state.getObject(id).buff_count > 0) return true;
                }
                return false;
            });
        if (answer == -1) return;  // yielded for agent input

        if (answer == 1) {
            for (auto id : friendly_units()) {
                auto& u = ctx.state.getObject(id);
                if (u.buff_count <= 0) continue;
                // Spend one buff: -1 [M], then ready.
                u.buff_count -= 1;
                u.temp_might_bonus -= 1;
                u.recomputeMight();
                ctx.executor.readyObject(id);
                ctx.events.logTrace("OVERT OPERATION: spent buff to ready " + u.name);
            }
        }

        // "Then buff all friendly units." (Engine buff = +1 [M].)
        for (auto id : friendly_units()) {
            ctx.executor.buffUnit(id);
        }
        ctx.events.logTrace("OVERT OPERATION: buffed all friendly units");
    }
};

// ─── [365] Brutalizer ───────────────────────────────────────────────────────
// "[Equip] [G]. If this was attached to me this turn, I have an additional
//  +2 [M]."
// We stamp the equip turn into a card_counter on the gear at attach time, then
// grant the +2 [M] aura to the bearer (via applyPassiveAura) while that turn
// matches the current turn number.
class AF2_Brutalizer : public GearCard {
public:
    AF2_Brutalizer() : GearCard(365) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        bool ok = af2_standardEquip(ctx, ctx.source, unit, /*energy=*/0, Domain::Calm);
        if (ok && ctx.state.objectExists(ctx.source)) {
            ctx.state.getObject(ctx.source).card_counters["__brutalizer_attach_turn"] =
                ctx.state.turn.turn_number;
        }
        return ok;
    }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [gid, gear] : state.objects) {
            if (gear.card_def_id != cardDefId()) continue;
            if (gear.controller != controller || !gear.attached_to.has_value()) continue;
            auto it = gear.card_counters.find("__brutalizer_attach_turn");
            if (it == gear.card_counters.end()) continue;
            if (it->second != state.turn.turn_number) continue;  // not attached this turn
            GameObjectId unit_id = *gear.attached_to;
            if (!state.objectExists(unit_id)) continue;
            GameObject::AuraEffect ae;
            ae.source = gid;
            ae.might_bonus = 2;
            state.getObject(unit_id).aura_effects.push_back(ae);
        }
    }
};

// ─── [435] Lucian, Merciless ────────────────────────────────────────────────
// "[Weaponmaster] (When you play me, you may [Equip] one of your Equipment to
//  me for [A] less, even if it's already attached.)
//  The first time I conquer each turn, ready me."
//
// [Weaponmaster] is the keyword surface; we reproduce the WhenYouPlayMe equip
// behavior (find a friendly Equipment, detach from its current bearer, attach
// to self for free) AND add the WhenIConquer "first time each turn, ready me".
class AF2_LucianMerciless : public UnitCard {
public:
    AF2_LucianMerciless() : UnitCard(435) {}

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            weaponmasterEquip(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIConquer) {
            firstConquerReady(ctx);
        }
    }

private:
    void weaponmasterEquip(CardContext& ctx) {
        // Find any friendly Equipment gear on board.
        GameObjectId best_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            bool is_equipment = false;
            for (auto& tag : obj.tags) {
                if (tag == "Equipment") { is_equipment = true; break; }
            }
            if (!is_equipment) continue;
            best_gear = id;
            break;
        }
        if (best_gear == kInvalidId) return;

        auto& gear = ctx.state.getObject(best_gear);
        // "Even if it's already attached" — detach first.
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(), old.attachments.end(), best_gear);
                if (it != old.attachments.end()) old.attachments.erase(it);
                old.recomputeMight();
            }
            gear.attached_to = std::nullopt;
        }

        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        gear.attached_to = ctx.source;
        self.attachments.push_back(best_gear);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();
        ctx.events.logTrace("LUCIAN MERCILESS: weaponmaster-equips " + gear.name);
        ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
    }

    void firstConquerReady(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        int turn_id = ctx.state.turn.turn_number + 1;  // +1 so 0 = "never"
        int& last = self.card_counters["__lucian_conquer_ready_turn"];
        if (last == turn_id) return;  // already fired this turn
        last = turn_id;
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("LUCIAN MERCILESS: first conquer this turn -> ready me");
    }
};

// ─── [471] Last Rites ─────────────────────────────────────────────────────
// "[Equip] — [P], Recycle 2 cards from your trash.
//  When I conquer or hold, you may play a unit from your trash. (You still
//  pay its costs.)"
//
// Fix vs prior manual impl: the "you may" is now an actual agent decision
// (confirmOptional) and the unit choice is an agent pick (pickTarget) rather
// than always grabbing the most-recent unit. The cost-payment portion remains
// playIgnoringCost — the EffectExecutor cannot drive payCardCost from inside a
// chain-resolving trigger (a standing engine limitation, documented below).
class AF2_LastRites : public GearCard {
public:
    AF2_LastRites() : GearCard(471) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.trash.size() < 2) return false;

        // Pre-check a Chaos power rune is available before committing.
        bool has_chaos = false;
        auto base_loc = BaseLocation{ctx.controller};
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            for (auto d : obj.domains) {
                if (d == Domain::Chaos) { has_chaos = true; break; }
            }
            if (has_chaos) break;
        }
        if (!has_chaos) return false;

        // Pay the additional cost: recycle 2 cards from trash (back-of-trash;
        // which-2 is auto, an agent-choice refinement for later).
        for (int i = 0; i < 2; ++i) {
            auto card_id = ps.trash.back();
            ps.trash.pop_back();
            ctx.events.logTrace("  EQUIP_COST: recycled " +
                                 ctx.state.getObject(card_id).name + " from trash");
            ctx.state.getObject(card_id).zone = ZoneType::MainDeck;
            ps.main_deck.insert(ps.main_deck.begin(), card_id);
        }
        return af2_standardEquip(ctx, ctx.source, unit, /*energy=*/0, Domain::Chaos);
    }

    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId /*unit*/,
                           const std::vector<GameObjectId>& /*targets*/) override {
        auto units_in_trash = [&]() {
            std::vector<GameObjectId> out;
            auto& ps = ctx.state.player(ctx.controller);
            for (auto card_id : ps.trash) {
                if (!ctx.state.objectExists(card_id)) continue;
                if (ctx.state.getObject(card_id).isUnit()) out.push_back(card_id);
            }
            return out;
        };

        // "you may play a unit from your trash"
        int answer = confirmOptional(ctx, "Last Rites: play a unit from trash",
            [&]() { return !units_in_trash().empty(); });
        if (answer == -1) return;     // yielded for agent input
        if (answer == 0) return;      // declined

        GameObjectId picked = pickTarget(ctx, "Last Rites: which unit from trash",
                                         units_in_trash());
        if (picked == kInvalidId) return;  // yielded / no legal target

        auto& ps = ctx.state.player(ctx.controller);
        ctx.events.logTrace("LAST RITES: plays " +
                             ctx.state.getObject(picked).name +
                             " from trash (cost-payment not yet wired — engine limitation)");
        // NOTE: text says "You still pay its costs", but EffectExecutor cannot
        // drive the engine's payCardCost cursor from inside a chain-resolving
        // trigger, so we play it for free for now (documented engine gap).
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), picked),
                       ps.trash.end());
    }
};

// ─── [531] Seat of Power ──────────────────────────────────────────────────
// "When you conquer here, draw 1 for each other battlefield you or allies
//  control."
// The generated stub always drew exactly 1. We count battlefields controlled
// by the conquering player, EXCLUDING this battlefield ("other"), and draw
// that many. (1v1: "you or allies" = the controller.)
class AF2_SeatOfPower : public BattlefieldCard {
public:
    AF2_SeatOfPower() : BattlefieldCard(531) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int count = 0;
        for (const auto& bf : ctx.state.battlefields) {
            if (!bf.controller.has_value() || *bf.controller != ctx.controller) continue;
            if (bf.card_object_id == ctx.source) continue;  // skip "here"
            count++;
        }
        if (count > 0) {
            ctx.executor.drawCards(ctx.controller, count);
        }
        ctx.events.logTrace("SEAT OF POWER: conquer -> draw " + std::to_string(count));
    }
};

// ─── [606] Flurry of Feathers ─────────────────────────────────────────────
// "[Reaction] Choose one —
//   • Counter a spell.
//   • Play four 1 [M] Bird unit tokens with [Deflect]."
// Fix vs prior manual impl: the mode is now an explicit agent choice
// (pickMode) instead of auto-selecting counter when a spell is present.
class AF2_FlurryOfFeathers : public SpellCard {
public:
    AF2_FlurryOfFeathers() : SpellCard(606) {}

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& chain = ctx.state.chain;
        bool counter_legal = !chain.items.empty() &&
                             chain.items.back().is_spell &&
                             chain.items.back().source != ctx.source;

        uint32_t legal_mask = 0b10;  // mode 1 (spawn birds) always legal
        if (counter_legal) legal_mask |= 0b01;

        int mode = pickMode(ctx, "Flurry of Feathers", /*num_modes=*/2,
                            {"Counter a spell", "Play four Bird tokens"},
                            legal_mask);
        if (mode == -1) return;   // yielded for agent input
        if (mode == -2) mode = 1; // no legal mode (shouldn't happen — birds always legal)

        if (mode == 0 && counter_legal) {
            auto victim = chain.items.back();
            chain.items.pop_back();
            ctx.events.logTrace("FLURRY OF FEATHERS: countered spell (id=" +
                                 std::to_string(victim.source) + ")");
            if (ctx.state.objectExists(victim.source)) {
                auto& sp = ctx.state.getObject(victim.source);
                auto owner = sp.owner;
                sp.zone = ZoneType::Trash;
                if (owner != PlayerId::None) {
                    ctx.state.player(owner).trash.push_back(victim.source);
                }
            }
            return;
        }

        // Mode 1: spawn four 1 [M] Bird tokens with [Deflect] at controller's base.
        KeywordSet kw; kw.set(Keyword::Deflect);
        for (int i = 0; i < 4; ++i) {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                      /*might=*/1, /*tags=*/{"Bird"}, kw,
                                      BaseLocation{ctx.controller},
                                      /*enter_ready=*/false);
        }
        ctx.events.logTrace("FLURRY OF FEATHERS: spawned 4 Bird tokens");
    }
};

// ─── [640] Sprite Fountain ────────────────────────────────────────────────
// "[Temporary] (engine-handled self-kill at Beginning Phase.)
//  When you play this, play a ready 3 [M] Sprite unit token with [Temporary]
//  to your base.
//  [Deathknell][>] Repeat this gear's play effect. (When this dies, get the
//  effect.)"
// The [Temporary] self-kill is the engine-handled keyword. We fire the
// token-spawn play effect on BOTH WhenYouPlayThis AND WhenIDie (Deathknell
// repeat of the play effect).
class AF2_SpriteFountain : public GearCard {
public:
    AF2_SpriteFountain() : GearCard(640) {}

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenIDie};
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Both the play trigger and the Deathknell repeat run the same effect:
        // play a ready 3 [M] Sprite token with [Temporary] to the base.
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite",
                                  /*might=*/3, /*tags=*/{"Fae"}, kw,
                                  BaseLocation{ctx.controller},
                                  /*enter_ready=*/true);
        ctx.events.logTrace("SPRITE FOUNTAIN: play 3M Sprite token (Temporary)");
    }
};

// ─── [657] Grim Resolve ───────────────────────────────────────────────────
// "[Action] Give a friendly unit +3 [M] this turn. When it wins a combat this
//  turn, gain 2 XP."
// The +3 [M] (working) is preserved. The "win-combat this turn" rider is
// registered as an object-scoped delayed ability (target_filter = the buffed
// unit, trigger = WhenIWinCombat); when it fires, onTrigger grants 2 XP.
class AF2_GrimResolve : public SpellCard {
public:
    AF2_GrimResolve() : SpellCard(657) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Grim Resolve", legal);
        if (picked == kInvalidId) return;  // yielded / fizzle

        ctx.executor.giveTemporaryMight(picked, 3);

        // Arm the "when it wins a combat this turn, gain 2 XP" rider.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenIWinCombat;
        da.target_filter = picked;
        da.expires_on_turn = ctx.state.turn.turn_number;  // this turn only
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("GRIM RESOLVE: +3M and armed win-combat XP rider");
    }

    // Single non-play trigger: when the delayed ability fires (the buffed unit
    // won a combat), gain 2 XP.
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("GRIM RESOLVE: buffed unit won combat -> gain 2 XP");
    }
};

// ─── [720] Shepherd's Heirloom ────────────────────────────────────────────
// "When you play this, gain 1 XP.
//  [Equip] — Spend 1 XP (Pay the cost: Attach this to a unit you control.)"
// Fix vs prior manual impl (which equipped with [Y] power and skipped the XP
// gain): the play trigger now grants 1 XP, and the equip cost is "Spend 1 XP"
// (not a power rune).
class AF2_ShepherdsHeirloom : public GearCard {
public:
    AF2_ShepherdsHeirloom() : GearCard(720) {}

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("SHEPHERD'S HEIRLOOM: play -> gain 1 XP");
    }

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.xp < 1) return false;  // can't pay the cost
        if (!ctx.state.objectExists(unit)) return false;
        ps.xp -= 1;  // Spend 1 XP
        ctx.events.logTrace("  EQUIP_COST: spent 1 XP");

        auto& gear = ctx.state.getObject(ctx.source);
        auto& u = ctx.state.getObject(unit);
        gear.attached_to = unit;
        u.attachments.push_back(ctx.source);
        gear.location = u.location;
        gear.zone = u.zone;
        u.attachment_might_bonus += gear.might_bonus;
        u.recomputeMight();
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + u.name);
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }
};

// ─── [764] Black Flame Altar ──────────────────────────────────────────────
// "Units here with [Temporary] have [Shield]. (+1 [M] while they're
//  defenders.)"
// Battlefield aura: for every unit at this battlefield that has [Temporary],
// grant [Shield] via an AuraEffect. The Shield combat-might bump is handled by
// the engine's normal Shield keyword (recomputeMight adds shield_value while
// defending); granting the keyword is what this card contributes.
class AF2_BlackFlameAltar : public BattlefieldCard {
public:
    AF2_BlackFlameAltar() : BattlefieldCard(764) {}

    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Find this battlefield's id (the BF whose card_object_id is an
        // instance of this card def).
        for (auto& [bid, bf_obj] : state.objects) {
            if (bf_obj.card_def_id != cardDefId()) continue;
            if (!bf_obj.isBattlefield()) continue;
            // Locate the BattlefieldState whose card_object_id == bid.
            std::optional<BattlefieldId> bf_id;
            for (const auto& bf : state.battlefields) {
                if (bf.card_object_id == bid) { bf_id = bf.id; break; }
            }
            if (!bf_id) continue;
            // Grant [Shield] to each unit at this BF that has [Temporary].
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != *bf_id) continue;
                if (!u.hasKeyword(Keyword::Temporary)) continue;
                GameObject::AuraEffect ae;
                ae.source = bid;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
};

// ─── Registration ───────────────────────────────────────────────────────────

void registerAuditFixes2(CardRegistry& registry) {
    registry.registerCard(29,  std::make_unique<AF2_FallingStar>());
    registry.registerCard(153, std::make_unique<AF2_OvertOperation>());
    registry.registerCard(365, std::make_unique<AF2_Brutalizer>());
    registry.registerCard(435, std::make_unique<AF2_LucianMerciless>());
    registry.registerCard(471, std::make_unique<AF2_LastRites>());
    registry.registerCard(531, std::make_unique<AF2_SeatOfPower>());
    registry.registerCard(606, std::make_unique<AF2_FlurryOfFeathers>());
    registry.registerCard(640, std::make_unique<AF2_SpriteFountain>());
    registry.registerCard(657, std::make_unique<AF2_GrimResolve>());
    registry.registerCard(720, std::make_unique<AF2_ShepherdsHeirloom>());
    registry.registerCard(764, std::make_unique<AF2_BlackFlameAltar>());
}

} // namespace riftbound
