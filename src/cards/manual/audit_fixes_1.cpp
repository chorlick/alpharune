// Audit-fix batch 1 — isolated override file.
//
// Each class below is registered LAST (via registerAuditFixes1), so it wins
// over the generated stub AND any earlier manual class for the same card id.
// Classes are prefixed AF1_ for global uniqueness across all audit_fixes_*.cpp.
//
// See /tmp/fix_batches/ENGINE_REFERENCE.md for the task spec.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {

// ─────────────────────────────────────────────────────────────────────────────
// [27] Darius, Trifarian
// "When you play your second card in a turn, give me +2 [M] this turn and
//  ready me."
// FIX: previously fired only on WhenYouPlayAUnit. "Second card" counts ANY
// card type, so register all three play triggers (unit/spell/gear) and gate
// on cards_played_this_turn == 2.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_DariusTrifarian : public UnitCard {
public:
    AF1_DariusTrifarian() : UnitCard(27) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayAUnit,
                TriggerType::WhenYouPlayASpell,
                TriggerType::WhenYouPlayAGear};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Trigger fires AFTER the play, so cards_played_this_turn already
        // counts the just-played card. The second card => count == 2.
        if (ctx.state.player(ctx.controller).cards_played_this_turn != 2) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("DARIUS TRIFARIAN: second card played, +2M and ready");
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [133] Flurry of Blades
// "[Reaction] Deal 1 to all units at battlefields."
// FIX: only units AT BATTLEFIELDS (the generated stub hit base units too).
// ─────────────────────────────────────────────────────────────────────────────
class AF1_FlurryOfBlades : public SpellCard {
public:
    AF1_FlurryOfBlades() : SpellCard(133) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // AoE: collect targets BEFORE killing (CR — avoid iterator invalidation).
        std::vector<GameObjectId> to_damage;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (!obj.isAtBattlefield()) continue;  // exclude base units
            to_damage.push_back(id);
        }
        for (auto id : to_damage) {
            if (ctx.state.objectExists(id))
                ctx.executor.dealDamage(id, 1, ctx.source);
        }
        for (auto id : to_damage) {
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
            }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [355] Disarming Rake
// "When you play me, you may kill a gear."
// FIX: generated impl killed targets[0] unconditionally, but onTrigger gets an
// EMPTY targets vector, so it was a no-op. Use confirmOptional ("you may") +
// pickTarget at resolve time over the legal gear list.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_DisarmingRake : public UnitCard {
public:
    AF1_DisarmingRake() : UnitCard(355) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto gearTargets = [&]() {
            std::vector<GameObjectId> gears;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear()) continue;
                if (!obj.location.has_value()) continue;  // must be on board
                gears.push_back(id);
            }
            return gears;
        };
        // "you may"
        auto still_legal = [&]() { return !gearTargets().empty(); };
        int conf = confirmOptional(ctx, "Disarming Rake: kill a gear?", still_legal);
        if (conf == -1) return;  // yielded for agent input
        if (conf == 0) return;   // declined or no legal gear

        auto gears = gearTargets();
        GameObjectId picked = pickTarget(ctx, "Disarming Rake: choose a gear", gears);
        if (picked == kInvalidId) return;  // yielded or fizzled
        ctx.executor.killObject(picked);
        ctx.events.logTrace("DISARMING RAKE: killed a gear");
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [431] Akshan, Mischievous
// "[Weaponmaster] You may pay [O][O] as an additional cost to play me. When you
//  play me, if you paid the additional cost, move an enemy gear to your base.
//  You control it until I leave the board. If it's an Equipment, attach it to
//  me."
// FIX: was an empty stub. Weaponmaster is engine-handled. The optional
// additional cost is NOT modeled at play-time by the engine, so (as with
// MNamiHeadstrong) we approximate by ALWAYS performing the gear-steal on play
// when an enemy gear exists — losing the "if you paid" gate. We take control
// permanently (until-leave-board reversion is not modeled generically) and
// attach the gear to Akshan if it's an Equipment.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_AkshanMischievous : public UnitCard {
public:
    AF1_AkshanMischievous() : UnitCard(431) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        PlayerId me = ctx.controller;

        // Find an enemy gear on the board.
        GameObjectId target_gear = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear()) continue;
            if (obj.controller == me) continue;       // must be enemy-controlled
            if (!obj.location.has_value()) continue;   // on board
            target_gear = id;
            break;
        }
        if (target_gear == kInvalidId) return;

        // Detach it from whatever it was equipped to (it moves zones).
        auto& gear = ctx.state.getObject(target_gear);
        bool is_equipment = false;
        for (auto& tag : gear.tags) {
            if (tag == "Equipment") { is_equipment = true; break; }
        }
        if (gear.attached_to.has_value()) {
            ctx.executor.unattachGear(target_gear);
        }

        // Take control (approximation: permanent — until-leave-board is not
        // modeled) and move it to my base.
        ctx.executor.takeControl(target_gear, me, /*until_end_of_turn=*/false);
        ctx.executor.moveToBase(target_gear);
        ctx.events.logTrace("AKSHAN: stole enemy gear (approx — no additional-cost gate)");

        // If it's an Equipment, attach it to me.
        if (is_equipment && ctx.state.objectExists(target_gear) &&
            ctx.state.objectExists(ctx.source)) {
            auto& g = ctx.state.getObject(target_gear);
            auto& self = ctx.state.getObject(ctx.source);
            g.attached_to = ctx.source;
            self.attachments.push_back(target_gear);
            g.location = self.location;
            g.zone = self.zone;
            g.controller = me;
            self.attachment_might_bonus += g.might_bonus;
            self.recomputeMight();
            ctx.events.emit(ObjectStateChangedEvent{target_gear, "attached"});
            ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
            ctx.events.logTrace("AKSHAN: attached stolen Equipment to me");
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [470] Ezreal, Prodigy
// "When you play me, discard 1, then draw 2.
//  Optional additional costs you pay cost [1] or [A] less."
// FIX: preserve discard-1-then-draw-2. The second clause (optional additional
// costs cost less) is NOT implementable: the engine does not model optional
// additional costs at play time, so there is no cost to reduce. Documented as
// a gap below; the discard/draw clause is fully reproduced.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_EzrealProdigy : public UnitCard {
public:
    AF1_EzrealProdigy() : UnitCard(470) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "discard 1, then draw 2" — resumable discard via the executor's
        // pending-choice slot (resume_point 0 = publish, 1 = commit + draw).
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty()) {
                // Can't discard — still draw 2 (CR: "discard 1, then draw 2";
                // an empty hand means the discard does nothing).
                ctx.executor.drawCards(ctx.controller, 2);
                return;
            }
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "discard 1 (Ezreal, Prodigy)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }
            ctx.executor.drawCards(ctx.controller, 2);
            ctx.events.logTrace("EZREAL PRODIGY: discarded 1, drew 2");
            return;
        }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [529] Ravenbloom Conservatory (battlefield)
// "When you defend here, reveal the top card of your Main Deck. If it's a
//  spell, put it in your hand. Otherwise, recycle it."
// FIX: was an empty stub. Reveal (emit CardRevealedEvent, public), then route
// spell→hand / non-spell→bottom of deck.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_RavenbloomConservatory : public BattlefieldCard {
public:
    AF1_RavenbloomConservatory() : BattlefieldCard(529) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.main_deck.empty()) return;

        // Top of deck = back of vector.
        GameObjectId top = ps.main_deck.back();
        if (!ctx.state.objectExists(top)) return;
        auto& card = ctx.state.getObject(top);

        // Reveal it publicly.
        ctx.events.emit(CardRevealedEvent{
            /*card=*/top,
            /*card_def_id=*/card.card_def_id,
            /*owner=*/card.owner,
            /*revealed_to_all=*/true,
            /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::MainDeck,
        });

        if (card.isSpell()) {
            // Put it in hand.
            ps.main_deck.pop_back();
            card.zone = ZoneType::Hand;
            card.location = std::nullopt;
            ps.hand.push_back(top);
            ctx.events.logTrace("RAVENBLOOM: revealed spell " + card.name +
                                 " -> hand");
        } else {
            // Recycle it (bottom of owner's main deck). Pop first so
            // recycleCards re-inserts it at the bottom exactly once.
            ps.main_deck.pop_back();
            ctx.executor.recycleCards(ctx.controller, {top});
            ctx.events.logTrace("RAVENBLOOM: revealed non-spell " + card.name +
                                 " -> recycled");
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [605] Enthusiastic Promoter
// "[Backline] When I hold, [Buff] all units here. (Give each a +1 [M] buff if
//  it doesn't have one.)"
// FIX: a [Buff] is a permanent +1 [M] buff counter (not temp might), and the
// reminder text gates it to units that don't ALREADY have a buff. Backline is
// engine-handled. Apply to ALL units here (both players), only those with no
// existing buff.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_EnthusiasticPromoter : public UnitCard {
public:
    AF1_EnthusiasticPromoter() : UnitCard(605) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.battlefieldId() != bf_id) continue;
            // "if it doesn't have one" — skip units that already have a buff.
            if (obj.buff_count > 0) continue;
            ctx.executor.buffUnit(id);
        }
        ctx.events.logTrace("ENTHUSIASTIC PROMOTER: +1M buff to unbuffed units here");
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [622] Vilemaw
// "[Ambush] Enemy units here with less Might than me don't deal combat damage.
//  When I hold, draw 1."
// FIX: preserve the WhenIHold draw-1 (working). The static "enemy units here
// with less Might don't deal combat damage" clause is NOT implementable: the
// engine's combat-damage step only suppresses a unit's contribution via
// is_stunned, and there is no generic per-unit "deals no combat damage" flag
// or AuraEffect field to drive it. Documented as a gap. Ambush is engine-
// handled.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_Vilemaw : public UnitCard {
public:
    AF1_Vilemaw() : UnitCard(622) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.events.logTrace("VILEMAW: hold — draw 1");
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [656] Gemhand Hunter
// "[Hunt] (When I conquer or hold, gain 1 XP.)
//  [Level 6][>] I have +1 [M]. (While you have 6+ XP, get the effect.)"
// FIX: implement Hunt (gain 1 XP on conquer/hold) and make Level 6 +1 [M] a
// CONTINUOUS passive (via applyPassiveAura) rather than a one-shot snapshot at
// play time, so it switches on/off as XP crosses the threshold.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_GemhandHunter : public UnitCard {
public:
    AF1_GemhandHunter() : UnitCard(656) {}

    // Hunt: gain 1 XP when I conquer or hold.
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("GEMHAND HUNTER: Hunt — gain 1 XP");
    }

    // Level 6: continuous +1 [M] while controller has 6+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 6) return;
        // Find every on-board instance of this card controlled by `controller`
        // and push a +1 might aura effect onto it.
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != card_def_id_) continue;
            if (obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = id;
            ae.might_bonus = 1;
            obj.aura_effects.push_back(ae);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// [712] Vex, Apathetic
// "[Deflect] When an opponent plays a unit while I'm at a battlefield, [Stun]
//  it. They can't move it this turn."
// FIX: Deflect is engine-handled (from the registry keyword set). The
// "opponent plays a unit -> stun it" clause is NOT implementable: there is no
// engine trigger for an OPPONENT playing a unit (WhenYouPlayAUnit fires only
// on the controller's own plays), nor a "can't move this unit this turn" per-
// unit flag. Documented as a gap. We register a no-op override so this card's
// stub is explicit (Deflect still applies via the registry).
// ─────────────────────────────────────────────────────────────────────────────
class AF1_VexApathetic : public UnitCard {
public:
    AF1_VexApathetic() : UnitCard(712) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// [756] Deceiver (legend)
// "When you conquer or hold, you may discard 1 and exhaust me to play a ready
//  Reflection unit token there. Then do this: It becomes a copy of another unit
//  there. Give it [Temporary]."
// FIX: the token must be created at the scored battlefield ("there"), must
// become a copy of another unit there, and must gain [Temporary].
// Resume machine: case 0 = exhaust legend + publish discard choice;
// case 1 = commit discard, create token at the scored BF, copy another unit
// there, grant Temporary.
//
// Approximations: "there" = a battlefield the controller controls/has units at
// (the score event doesn't carry the BF id to legend triggers). The "copy of
// another unit there" is picked deterministically (highest-might other unit at
// that BF) to avoid resume-slot collision with the discard machine.
// ─────────────────────────────────────────────────────────────────────────────
class AF1_Deceiver : public LegendCard {
public:
    AF1_Deceiver() : LegendCard(756) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }

    // Find a battlefield the controller is "at" / controls — best guess for
    // "there". Prefer one the controller controls; fall back to any BF with
    // friendly units.
    static std::optional<BattlefieldId> scoredBattlefield(GameState& state,
                                                           PlayerId controller) {
        // Controlled battlefield first.
        for (auto& bf : state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == controller)
                return bf.id;
        }
        // Otherwise any BF with a friendly unit.
        for (auto& bf : state.battlefields) {
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != controller) continue;
                if (obj.battlefieldId() == bf.id) return bf.id;
            }
        }
        return std::nullopt;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        switch (ri.resume_point) {
        case 0: {
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& legend = ctx.state.getObject(ctx.source);
            if (legend.is_exhausted) return;          // can't pay the exhaust cost
            auto& ps = ctx.state.player(ctx.controller);
            if (ps.hand.empty()) return;              // can't pay the discard cost
            // Cost paid: exhaust the legend.
            legend.is_exhausted = true;
            // Publish the discard choice (which card of hand to discard).
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "discard 1 (Deceiver)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }

            // Determine "there" — the scored battlefield.
            auto bf = scoredBattlefield(ctx.state, ctx.controller);
            LocationId loc = bf.has_value()
                ? LocationId{BattlefieldLocation{*bf}}
                : LocationId{BaseLocation{ctx.controller}};

            // Create a ready Reflection unit token there. [Temporary] up front.
            KeywordSet kw;
            kw.set(Keyword::Temporary);
            GameObjectId token = ctx.executor.createToken(
                ctx.controller, CardType::Unit, "Reflection", 0, {}, kw, loc,
                /*enter_ready=*/true);
            ctx.events.logTrace("DECEIVER: created ready Reflection token there");

            // "It becomes a copy of another unit there." Pick the highest-
            // might OTHER unit at the same location (any controller).
            if (bf.has_value()) {
                GameObjectId best = kInvalidId;
                int best_might = -1;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (id == token) continue;
                    if (!obj.isUnit()) continue;
                    if (obj.battlefieldId() != *bf) continue;
                    if (obj.current_might > best_might) {
                        best_might = obj.current_might;
                        best = id;
                    }
                }
                if (best != kInvalidId) {
                    ctx.executor.copyUnit(token, best);
                    // copyUnit overwrites keywords from the copied def, so
                    // re-grant [Temporary] afterwards (CR: the token keeps it).
                    if (ctx.state.objectExists(token)) {
                        ctx.state.getObject(token).keywords.set(Keyword::Temporary);
                    }
                    ctx.events.logTrace("DECEIVER: token copied a unit there, kept [Temporary]");
                }
            }
            return;
        }
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
void registerAuditFixes1(CardRegistry& registry) {
    registry.registerCard(27,  std::make_unique<AF1_DariusTrifarian>());
    registry.registerCard(133, std::make_unique<AF1_FlurryOfBlades>());
    registry.registerCard(355, std::make_unique<AF1_DisarmingRake>());
    registry.registerCard(431, std::make_unique<AF1_AkshanMischievous>());
    registry.registerCard(470, std::make_unique<AF1_EzrealProdigy>());
    registry.registerCard(529, std::make_unique<AF1_RavenbloomConservatory>());
    registry.registerCard(605, std::make_unique<AF1_EnthusiasticPromoter>());
    registry.registerCard(622, std::make_unique<AF1_Vilemaw>());
    registry.registerCard(656, std::make_unique<AF1_GemhandHunter>());
    registry.registerCard(712, std::make_unique<AF1_VexApathetic>());
    registry.registerCard(756, std::make_unique<AF1_Deceiver>());
}

} // namespace riftbound
