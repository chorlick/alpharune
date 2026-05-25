// Audit-fix batch 7 — manual overrides for 11 cards.
// Registered LAST (after generated stubs + earlier manual classes), so each
// AF7_* class wins over whatever exists for that card id. See
// /tmp/fix_batches/ENGINE_REFERENCE.md for the task contract.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace riftbound {

// ─── [95] Stupefy ───────────────────────────────────────────────────────────
// [Reaction] Give a unit -1 [M] this turn, to a minimum of 1 [M]. Draw 1.
// Gap: the -1M floor (minimum 1) wasn't enforced. giveTemporaryMight's third
// arg is the minimum; pass 1.
class AF7_Stupefy : public SpellCard {
public:
    AF7_Stupefy() : SpellCard(95) {}
    bool isReactionAbility() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.giveTemporaryMight(targets[0], -1, /*minimum=*/1);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
};

// ─── [285] The Arena's Greatest (Battlefield) ───────────────────────────────
// "At the start of each player's first Beginning Phase, that player gains 1
// point." Modeled as AtStartOfBeginning with a per-player "already fired"
// gate stored in card_counters keyed by player index. NOTE: the engine's
// AtStartOfBeginning dispatch (TriggerManager::onPhaseChanged) only fires for
// on-board objects whose controller == turn_player. A battlefield card object
// has controller == None, so this trigger does not currently fire from the
// engine for BF cards — a documented engine plumbing gap. The trigger body is
// CR-correct should that dispatch ever cover battlefield cards.
class AF7_TheArenasGreatest : public BattlefieldCard {
public:
    AF7_TheArenasGreatest() : BattlefieldCard(285) {}
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // "first Beginning Phase" per player: gate by player index key.
        std::string key = "__arenas_greatest_fired_" +
                          std::to_string(playerIndex(ctx.controller));
        if (self.card_counters[key] != 0) return;  // already fired for this player
        self.card_counters[key] = 1;
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("THE ARENA'S GREATEST: first Beginning Phase -> +1 point ("
                            + std::to_string(ps.score) + ")");
    }
};

// ─── [407] Ornn, Forge God (Weaponmaster unit) ──────────────────────────────
// [Deflect 2] (engine keyword), [Weaponmaster] (when you play me, may equip an
// Equipment to me for [A] less, even if already attached), and "I have +1 [M]
// for each friendly gear." The Weaponmaster on-play equip is reproduced inline
// (the WeaponmasterUnit base lives in another TU we can't subclass here). The
// per-friendly-gear self-buff is an ongoing passive via applyPassiveAura.
class AF7_Ornn : public UnitCard {
public:
    AF7_Ornn() : UnitCard(407) {}

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Weaponmaster: find a friendly Equipment gear and attach it to me
        // (free — "[A] less" reduces the simple equip cost to nothing here).
        if (!ctx.state.objectExists(ctx.source)) return;
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
        // "even if it's already attached" — detach from current bearer first.
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
        auto& self = ctx.state.getObject(ctx.source);
        gear.attached_to = ctx.source;
        self.attachments.push_back(best_gear);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
        ctx.events.logTrace("ORNN: Weaponmaster equip " + gear.name);
    }

    // "I have +1 [M] for each friendly gear." Recomputed every aura pass.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Locate this Ornn instance (on board, controlled by `controller`).
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            int gear_count = 0;
            for (auto& [gid, g] : state.objects) {
                if (g.isGear() && g.controller == controller && g.location.has_value())
                    ++gear_count;
            }
            if (gear_count <= 0) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = gear_count;
            self.aura_effects.push_back(ae);
        }
    }
};

// ─── [466] Switcheroo (Spell) ───────────────────────────────────────────────
// [Hidden] (engine), [Action] (engine). "Swap the Might of two units at the
// same battlefield this turn." Modeled via temporary-might deltas so each unit
// ends the turn with the other's current Might. Uses the pick-pair helper to
// choose two units; the second must be at the SAME battlefield as the first.
class AF7_Switcheroo : public SpellCard {
public:
    AF7_Switcheroo() : SpellCard(466) {}
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> first_legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
            first_legal.push_back(id);
        }
        auto second_fn = [&](GameObjectId picked_a) {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(picked_a)) return out;
            auto bf_a = ctx.state.getObject(picked_a).battlefieldId();
            if (!bf_a) return out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == picked_a) continue;
                if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
                if (obj.battlefieldId() != bf_a) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Switcheroo", first_legal, second_fn);
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a == kInvalidId || b == kInvalidId) return;
        if (!ctx.state.objectExists(a) || !ctx.state.objectExists(b)) return;
        int might_a = ctx.state.getObject(a).current_might;
        int might_b = ctx.state.getObject(b).current_might;
        // Swap: bump each toward the other's current Might (this turn).
        ctx.executor.giveTemporaryMight(a, might_b - might_a);
        ctx.executor.giveTemporaryMight(b, might_a - might_b);
        ctx.events.logTrace("SWITCHEROO: swapped Might " + std::to_string(might_a) +
                            " <-> " + std::to_string(might_b));
    }
};

// ─── [521] Emperor's Dais (Battlefield) ─────────────────────────────────────
// "When you conquer here, you may pay [1] and return a unit you control here to
// its owner's hand. If you do, play a 2 [M] Sand Soldier unit token here."
// Optional ([1] cost + bounce); if performed, a 2M Sand Soldier token enters
// at this battlefield. The [1] is paid by exhausting a ready rune (mirrors the
// equip cost-payment idiom). Skips if the player can't pay or has no unit here.
class AF7_EmperorsDais : public BattlefieldCard {
public:
    AF7_EmperorsDais() : BattlefieldCard(521) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Find "here" — the battlefield whose card object is this card.
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        // A friendly unit must be here to bounce.
        GameObjectId bounce_target = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) { bounce_target = id; break; }
        }

        // Count a ready rune to pay [1].
        auto base_loc = BaseLocation{ctx.controller};
        GameObjectId pay_rune = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            pay_rune = id;
            break;
        }

        // "You may" — only offer when both the [1] and a unit-here exist.
        int decision = confirmOptional(ctx, "Emperor's Dais: pay [1], return a unit",
            [&]() {
                return bounce_target != kInvalidId && pay_rune != kInvalidId;
            });
        if (decision == -1) return;  // waiting on agent choice
        if (decision == 0) return;   // declined / not legal

        // Re-validate the pick (state may have shifted while deciding).
        if (!ctx.state.objectExists(bounce_target) || !ctx.state.objectExists(pay_rune)) return;
        // Pay [1] — exhaust the ready rune.
        ctx.state.getObject(pay_rune).is_exhausted = true;
        // Return the unit to its owner's hand.
        ctx.executor.bounceToHand(bounce_target);
        // "If you do" — play a 2M Sand Soldier unit token here.
        auto loc = LocationId{BattlefieldLocation{here}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sand Soldier", 2,
                                 {"Sand Soldier"}, KeywordSet{}, loc);
        ctx.events.logTrace("EMPEROR'S DAIS: paid [1], bounced unit, spawned 2M Sand Soldier");
    }
};

// ─── [602] Wuju Apprentice (Unit) ───────────────────────────────────────────
// [Hunt] (When I conquer or hold, gain 1 XP.) and
// [Level 6][>] When you play me, draw 1.
// manual-partial: the Level-6 play-draw was present; the [Hunt] XP gain was
// missing. Two triggers now: WhenYouPlayMe (level-gated draw) and
// WhenIConquerOrHold (Hunt +1 XP).
class AF7_WujuApprentice : public UnitCard {
public:
    AF7_WujuApprentice() : UnitCard(602) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquerOrHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIConquerOrHold) {
            // [Hunt] — gain 1 XP on conquer or hold.
            ctx.state.player(ctx.controller).xp += 1;
            ctx.events.logTrace("WUJU APPRENTICE: [Hunt] +1 XP");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // [Level 6] — only with 6+ XP.
            if (ctx.state.player(ctx.controller).xp < 6) return;
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("WUJU APPRENTICE: Level 6, draw 1");
        }
    }
};

// ─── [615] Scuttle Crab (Unit) ──────────────────────────────────────────────
// "(Units with 0 [M] can conquer and hold.)" — keyword/reminder, engine.
// "When you play me, draw 1."
// "[Deathknell][>] Choose an opponent. They reveal their hand. You can look at
//  their facedown cards this turn. Gain 1 XP. (When I die, get the effects.)"
// manual-partial: the play-draw existed; the entire Deathknell was missing.
// Two triggers now: WhenYouPlayMe (draw 1) and WhenIDie (Deathknell). The
// "reveal hand / look at facedown" portion is observation-only — we emit
// CardRevealedEvents so observation tracking records them, but the only
// mechanical state change is "Gain 1 XP".
class AF7_ScuttleCrab : public UnitCard {
public:
    AF7_ScuttleCrab() : UnitCard(615) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("SCUTTLE CRAB: play — draw 1");
            return;
        }
        if (ctx.firing_trigger == TriggerType::WhenIDie) {
            // [Deathknell] — choose an opponent (only one in 1v1), reveal
            // their hand (observation), gain 1 XP.
            auto opp = opponent(ctx.controller);
            for (auto card_id : ctx.state.player(opp).hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                auto& obj = ctx.state.getObject(card_id);
                ctx.events.emit(CardRevealedEvent{
                    card_id, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller,
                    ZoneType::Hand});
            }
            ctx.state.player(ctx.controller).xp += 1;
            ctx.events.logTrace("SCUTTLE CRAB: [Deathknell] reveal opp hand, +1 XP");
        }
    }
};

// ─── [651] Jhin, Meticulous Killer (Unit) ───────────────────────────────────
// [Vision] (When you play me, look at top card, may recycle it) — ENGINE-
// handled via the Vision keyword in TriggerManager::onCardPlayed; no card code
// needed. "If you've spent [4] or more to play a spell this turn, you may play
// me for [B]." — an ALTERNATIVE PLAY COST. The Card surface exposes no
// alternative-play-cost hook (only selfCostReduction, which can't switch the
// cost's domain or gate on per-turn spell spend), so the [B] alt-cost is NOT
// implementable here. We register a clean UnitCard so the Vision keyword path
// runs and the generated no-op stub is replaced; the alt-cost is documented as
// an engine gap. See report.
class AF7_JhinMeticulousKiller : public UnitCard {
public:
    AF7_JhinMeticulousKiller() : UnitCard(651) {}
};

// ─── [702] Conscription (Spell) ─────────────────────────────────────────────
// "You may spend 5 XP as an additional cost to play this. Choose an enemy unit
//  at a battlefield with 3 [M] or less. If you paid the additional cost,
//  choose any enemy unit at a battlefield instead. Take control of it, exhaust
//  it, and recall it." Optional 5-XP additional cost (only if xp >= 5). When
//  paid, the target restriction (3M-or-less) is lifted. Resolve order:
//  confirmOptional (pay 5 XP?) -> pickTarget over the appropriate legal set ->
//  takeControl + exhaust + recall (moveToBase).
class AF7_Conscription : public SpellCard {
public:
    AF7_Conscription() : SpellCard(702) {}
    // Broad play-time requirement; the 3M cap / "any" widening is applied at
    // resolve based on whether 5 XP was paid.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        // Legal if any enemy unit sits at a battlefield (might cap is checked
        // at resolve; if 5 XP would be paid the cap is irrelevant).
        auto opp = opponent(controller);
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Optional additional cost: spend 5 XP (only offered if affordable).
        int paid = confirmOptional(ctx, "Conscription: spend 5 XP (any enemy unit)",
            [&]() { return ps.xp >= 5; });
        if (paid == -1) return;  // waiting on agent

        bool widened = (paid == 1);
        if (widened) ps.xp -= 5;

        // Build the legal enemy-unit set under the current restriction.
        auto opp = opponent(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            if (!widened && obj.current_might > 3) continue;
            legal.push_back(id);
        }
        GameObjectId picked = pickTarget(ctx, "Conscription target", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Take control of it, exhaust it, and recall it (to its base).
        ctx.executor.takeControl(picked, ctx.controller, /*until_end_of_turn=*/false);
        ctx.executor.exhaustObject(picked);
        ctx.executor.moveToBase(picked);
        ctx.events.logTrace("CONSCRIPTION: take control + exhaust + recall (paid="
                            + std::to_string(widened) + ")");
    }
};

// ─── [748] Hextech Gauntlets (Equipment Gear) ───────────────────────────────
// "[Equip] [3][A]. This ability's Energy cost is reduced by the Might of the
//  unit you choose." effect_text: "When I conquer, if you assigned 3 or more
//  excess damage, draw 1." might_bonus=3.
// manual-partial: was a plain [3][A] equip with no might-based reduction and no
// conquer rider. We reproduce the [A] equip payment, reduce the [3] energy by
// the target unit's current Might (floored at 0), and add the WhenIConquer
// rider. The "3+ excess damage" condition has no engine signal exposed to the
// equipped trigger (excess-damage assignment isn't threaded through), so the
// rider draws on conquer unconditionally as the best-effort approximation.
class AF7_HextechGauntlets : public GearCard {
public:
    AF7_HextechGauntlets() : GearCard(748) {}

    bool hasEquipAbility() const override { return true; }

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        if (!ctx.state.objectExists(unit)) return false;
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        // Energy cost = [3] reduced by chosen unit's Might (min 0).
        int reduction = state.getObject(unit).current_might;
        int energy_cost = 3 - reduction;
        if (energy_cost < 0) energy_cost = 0;

        // Pre-check: enough ready runes for energy AND one rune to recycle for
        // [A]. Bail (no state change) if either is unpayable.
        int ready_count = 0;
        GameObjectId any_rune = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            if (!obj.is_exhausted) ready_count++;
            if (any_rune == kInvalidId) any_rune = id;
        }
        if (ready_count < energy_cost) return false;
        if (any_rune == kInvalidId) return false;

        // Pay energy: exhaust `energy_cost` ready runes.
        int e_remaining = energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (e_remaining <= 0) break;
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            obj.is_exhausted = true;
            e_remaining--;
        }
        // Recycle any rune for [A] power.
        {
            auto& r = state.getObject(any_rune);
            ctx.events.logTrace("  EQUIP_COST: recycled " + r.name + " for [A]");
            r.location = std::nullopt;
            r.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), any_rune);
        }

        // Attach.
        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: Hextech Gauntlets -> " + unit_obj.name +
                            " (energy paid=" + std::to_string(energy_cost) +
                            ", reduced by Might " + std::to_string(reduction) + ")");
        gear.attached_to = unit;
        unit_obj.attachments.push_back(ctx.source);
        gear.location = unit_obj.location;
        gear.zone = unit_obj.zone;
        unit_obj.attachment_might_bonus += gear.might_bonus;
        unit_obj.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }

    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId /*unit*/,
                            const std::vector<GameObjectId>& /*targets*/) override {
        // "if you assigned 3 or more excess damage" — excess-damage assignment
        // isn't surfaced to the equipped trigger, so draw unconditionally on
        // conquer (best-effort; documented in report).
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("HEXTECH GAUNTLETS: conquer -> draw 1");
    }
};

// ─── [771] Star Spring (Battlefield) ────────────────────────────────────────
// "The first time a player plays a non-token unit here each turn, they may move
//  another unit they control here to its base." Modeled as WhenYouPlayAUnit on
//  the battlefield card; gated to (a) the unit was played AT this battlefield,
//  (b) it's non-token, and (c) it's the first such play this turn (tracked in
//  card_counters keyed by player + turn). The "may move another unit here to
//  its base" is an optional move (confirmOptional + pick a different friendly
//  unit at this BF -> moveToBase). NOTE: the engine's WhenYouPlayAUnit dispatch
//  only fires for on-board objects whose controller == the playing player; a
//  battlefield card has controller == None, so this trigger does not currently
//  fire from the engine for BF cards — a documented engine plumbing gap. The
//  body is CR-correct should that dispatch cover battlefield cards.
class AF7_StarSpring : public BattlefieldCard {
public:
    AF7_StarSpring() : BattlefieldCard(771) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        // Find the just-played unit: the newest unit controlled by ctx.controller
        // that sits at this battlefield and is non-token. (Trigger carries no
        // played-object id, so we self-select.)
        GameObjectId played = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.isToken()) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != here) continue;
            if (id > played || played == kInvalidId) played = id;  // newest id heuristic
        }
        if (played == kInvalidId) return;

        // First-time-this-turn gate, per player.
        auto& self = ctx.state.getObject(ctx.source);
        std::string key = "__starspring_p" + std::to_string(playerIndex(ctx.controller));
        if (self.card_counters[key] == ctx.state.turn.turn_number &&
            ctx.state.turn.turn_number != 0) {
            return;  // already triggered this turn for this player
        }

        // Build "another unit they control here" set (exclude the played one).
        std::vector<GameObjectId> movable;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == played) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) movable.push_back(id);
        }

        int decision = confirmOptional(ctx, "Star Spring: move another unit here to base",
            [&]() { return !movable.empty(); });
        if (decision == -1) return;  // waiting on agent
        // Mark first-play handled regardless of yes/no (the "first time" is
        // consumed by reaching the decision).
        self.card_counters[key] = ctx.state.turn.turn_number;
        if (decision == 0) return;

        // Re-pick a still-legal movable unit and move it to its base.
        for (auto id : movable) {
            if (!ctx.state.objectExists(id)) continue;
            ctx.executor.moveToBase(id);
            ctx.events.logTrace("STAR SPRING: moved a unit here to its base");
            break;
        }
    }
};

// ─── Registration ───────────────────────────────────────────────────────────

void registerAuditFixes7(CardRegistry& registry) {
    registry.registerCard(95,  std::make_unique<AF7_Stupefy>());
    registry.registerCard(285, std::make_unique<AF7_TheArenasGreatest>());
    registry.registerCard(407, std::make_unique<AF7_Ornn>());
    registry.registerCard(466, std::make_unique<AF7_Switcheroo>());
    registry.registerCard(521, std::make_unique<AF7_EmperorsDais>());
    registry.registerCard(602, std::make_unique<AF7_WujuApprentice>());
    registry.registerCard(615, std::make_unique<AF7_ScuttleCrab>());
    registry.registerCard(651, std::make_unique<AF7_JhinMeticulousKiller>());
    registry.registerCard(702, std::make_unique<AF7_Conscription>());
    registry.registerCard(748, std::make_unique<AF7_HextechGauntlets>());
    registry.registerCard(771, std::make_unique<AF7_StarSpring>());
}

} // namespace riftbound
