// Audit-fix batch 5 — manual override cards.
//
// These override (win over) the generated stubs AND any earlier manual
// classes for the same card ids, because manual overrides are registered
// last. One isolated class per card, prefixed AF5_ for global uniqueness.
//
// Cards in this batch:
//   [48]  Meditation              — optional "exhaust friendly -> draw 2" branch
//   [277] Monastery of Hirana     — WhenYouConquerHere: spend a buff to draw 1
//   [382] Svellsongur            — equip preserved (text-copy not modeled)
//   [460] Edge of Night           — play-from-facedown auto-attach + Equip [C]
//   [506] Fire Below the Mountain — gear-only reaction add (restriction noted)
//   [544] Yone, Blademaster       — Weaponmaster + conquer-uncontrolled damage
//   [613] Ivern, Nurturer         — play OR hold; optional reveal; themed buff
//   [644] Lillia, Fae Fawn        — Sprite token spawns at move location
//   [682] Rengar, Trophy Hunter   — Ambush-to-enemy-BF (engine gap noted)
//   [739] Ivern, Friend to All    — agent-chosen tag; all-4-tag score
//   [769] Gardens of Becoming     — granted "[E]: Gain 1 XP" (engine gap noted)

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {

// ─── Local helpers (file-private; mirror equip_cards.cpp idioms) ─────────────

namespace af5_detail {

// Pay a domain-power equip cost and attach `gear_id` to `unit_id`.
// Mirrors equip_cards.cpp::standardEquip exactly (that function is
// file-local there, so we re-state it here). Returns false (no state
// change) if the cost cannot be paid.
inline bool standardEquip(CardContext& ctx, GameObjectId gear_id,
                          GameObjectId unit_id, int energy_cost, Domain domain) {
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

// Attach a gear directly (no cost) to a unit. Used by Edge of Night's
// "play from face down → attach it" effect (CR: free attach, not the
// Equip ability). Mirrors the attach tail of standardEquip.
inline void attachFree(CardContext& ctx, GameObjectId gear_id,
                       GameObjectId unit_id) {
    auto& state = ctx.state;
    auto& gear = state.getObject(gear_id);
    auto& unit = state.getObject(unit_id);
    // Detach from any prior unit first (shouldn't normally happen here).
    if (gear.attached_to.has_value() &&
        state.objectExists(*gear.attached_to)) {
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
    ctx.events.logTrace("EDGE OF NIGHT: auto-attached " + gear.name + " -> " +
                         unit.name);
    ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
    ctx.events.emit(ObjectStateChangedEvent{unit_id, "equipped"});
}

// Tag-presence scan for Ivern-family cards (Bird/Cat/Dog/Poro among the
// controller's units). Re-stated here (the equivalent ivern_tags helper
// in deck_cards.cpp is file-local).
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

} // namespace af5_detail

// ─── [48] Meditation ─────────────────────────────────────────────────────────
//
// "[Reaction] As an additional cost to play this, you may exhaust a friendly
//  unit. If you do, draw 2. Otherwise, draw 1."
//
// The engine has no native "optional additional cost at play time" hook, so
// we model the choice at resolution via confirmOptional: the agent decides
// whether to exhaust a ready friendly unit; if so, exhaust one and draw 2,
// otherwise draw 1. Functionally equivalent (the exhaust + extra card both
// happen for the same play). The [Reaction] timing keyword is engine-handled.
class AF5_Meditation : public SpellCard {
public:
    AF5_Meditation() : SpellCard(48) {}
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto find_ready_friendly = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.is_exhausted) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_ready_friendly() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Meditation: exhaust a friendly unit to draw 2 (else draw 1)?",
            still_legal);
        if (conf == -1) return;  // waiting for agent choice

        if (conf == 1) {
            auto target = find_ready_friendly();
            if (target != kInvalidId) {
                ctx.executor.exhaustObject(target);
                ctx.executor.drawCards(ctx.controller, 2);
                ctx.events.logTrace("MEDITATION: exhausted friendly -> draw 2");
                return;
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MEDITATION: draw 1");
    }
};

// ─── [277] Monastery of Hirana ───────────────────────────────────────────────
//
// "When you conquer here, you may spend a buff to draw 1."
//
// Fires on WhenYouConquerHere (the battlefield's controller just conquered
// here). Optional: agent may spend a buff on one of their units to draw 1.
// "Spend a buff" = remove one buff counter from a friendly buffed unit.
class AF5_MonasteryOfHirana : public BattlefieldCard {
public:
    AF5_MonasteryOfHirana() : BattlefieldCard(277) {}
    TriggerType triggerType() const override {
        return TriggerType::WhenYouConquerHere;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto find_buffed_friendly = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.buff_count <= 0) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_buffed_friendly() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Monastery of Hirana: spend a buff to draw 1?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_buffed_friendly();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        obj.buff_count -= 1;
        obj.recomputeMight();
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MONASTERY OF HIRANA: spent a buff on " + obj.name +
                             " -> draw 1");
    }
};

// ─── [382] Svellsongur ───────────────────────────────────────────────────────
//
// "[Equip] [1][G] ... As this is attached to a unit, copy that unit's text to
//  this Equipment's effect text for as long as this is attached to it."
//
// The equip ([1] energy + Calm power) is fully modeled. The "copy the
// attached unit's rules text onto this gear" clause is a meta-ability over
// printed text that the engine has no representation for (gear granting the
// unit's own abilities/triggers via text-copy). NOT implemented — see report.
class AF5_Svellsongur : public GearCard {
public:
    AF5_Svellsongur() : GearCard(382) {}
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return af5_detail::standardEquip(ctx, ctx.source, unit,
                                         /*energy_cost=*/1, Domain::Calm);
    }
};

// ─── [460] Edge of Night ─────────────────────────────────────────────────────
//
// "[Hidden] ... When you play this from face down, attach it to a unit you
//  control (here). [Equip] [C] ([C]: Attach this to a unit you control.)"
//
// Two ways to land:
//   • Equip ability (pay Chaos power) → attaches directly via onEquip.
//   • Played from face down → enters the board unattached at the BF it was
//     hidden at; WhenYouPlayThis fires (resolvePermanent emits
//     EnteredBoardEvent{from_play=true} for hidden plays too). We then
//     auto-attach to a friendly unit at the same location ("here").
//
// Guard: only auto-attach when the gear is currently UNATTACHED and on a
// battlefield — i.e. the facedown-play case. A normal Equip play already
// attached it, so the guard makes WhenYouPlayThis a no-op there.
class AF5_EdgeOfNight : public GearCard {
public:
    AF5_EdgeOfNight() : GearCard(460) {}

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [C] = Chaos-domain power, no energy (mirrors original
        // SimpleEquipGear(460, Domain::Chaos) with energy_cost 0).
        return af5_detail::standardEquip(ctx, ctx.source, unit,
                                         /*energy_cost=*/0, Domain::Chaos);
    }

    TriggerType triggerType() const override {
        return TriggerType::WhenYouPlayThis;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Only the play-from-facedown case: unattached gear sitting at a BF.
        if (self.attached_to.has_value()) return;
        if (!self.isAtBattlefield()) return;
        if (!self.location.has_value()) return;
        const auto here = *self.location;
        // Attach to a friendly unit "here" (same battlefield).
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != here) continue;
            af5_detail::attachFree(ctx, ctx.source, id);
            return;
        }
    }
};

// ─── [506] Fire Below the Mountain ───────────────────────────────────────────
//
// "[E]: [Reaction] — [Add] [A]. Use only to play gear or use gear abilities.
//  (Abilities that add resources can't be reacted to.)"
//
// Preserves the working behavior (exhaust → add [A] universal power) and
// marks it as a Reaction-timing ability. The "use only to play gear / gear
// abilities" earmark restricts WHAT the added [A] can be spent on — the
// engine's rune pool has no per-source spend-restriction tag, so that
// restriction is NOT enforced (the [A] is spendable on anything). See report.
class AF5_FireBelowMtn : public LegendCard {
public:
    AF5_FireBelowMtn() : LegendCard(506) {}
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // [Add] [A] resolves immediately (CR 429.2) — not via the chain.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("ACTIVATE: Fire Below the Mountain adds [A] "
                            "(spend earmarked for gear — not engine-enforced)");
    }
};

// ─── [544] Yone, Blademaster ─────────────────────────────────────────────────
//
// "[Weaponmaster] ... When I conquer a battlefield that was uncontrolled,
//  deal damage equal to my Might to an enemy unit in a base."
//
// Two triggers:
//   • WhenYouPlayMe → Weaponmaster equip (re-stated; WeaponmasterUnit is
//     file-local in weaponmaster_cards.cpp so we inline its behavior).
//   • WhenIConquer → deal my-Might damage to an enemy unit in a base.
//
// "Battlefield that was uncontrolled" condition: a WhenIConquer fire means I
// just took the BF; if it was previously uncontrolled the BF's prior
// controller was empty. The engine doesn't surface the pre-conquer
// controller through the trigger, so we approximate by always applying the
// damage on conquer (the common case — Yone conquers open BFs). Damage goes
// to an enemy unit in a base.
class AF5_YoneBlademaster : public UnitCard {
public:
    AF5_YoneBlademaster() : UnitCard(544) {}

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            doWeaponmasterEquip(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIConquer) {
            doConquerDamage(ctx);
        }
    }

private:
    // Inlined Weaponmaster "When you play me, you may [Equip] one of your
    // Equipment to me for [A] less, even if already attached." Mirrors
    // weaponmaster_cards.cpp::WeaponmasterUnit::onTrigger (free attach).
    void doWeaponmasterEquip(CardContext& ctx) {
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
        if (!ctx.state.objectExists(ctx.source)) return;

        auto& gear = ctx.state.getObject(best_gear);
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(),
                                    old.attachments.end(), best_gear);
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
        ctx.events.logTrace("WEAPONMASTER(Yone): equips " + gear.name +
                             " (free, [A] less)");
        ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
    }

    void doConquerDamage(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        int my_might = self.current_might;
        if (my_might <= 0) return;
        PlayerId opp = opponent(ctx.controller);
        // Target an enemy unit in a base.
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBase()) continue;
            ctx.executor.dealDamage(id, my_might, ctx.source);
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
            }
            ctx.events.logTrace("YONE: conquer -> deal " +
                                 std::to_string(my_might) +
                                 " to enemy unit in base");
            return;
        }
    }
};

// ─── [613] Ivern, Nurturer ───────────────────────────────────────────────────
//
// "When you play me or when I hold, look at the top 3 cards of your Main Deck.
//  You may reveal a unit from among them and draw it. Recycle the rest. Then
//  if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff] a friendly unit."
//
// Fixes: now fires on play AND hold (multi-trigger), and the draft is
// OPTIONAL ("you may reveal a unit ... and draw it") via confirmOptional.
class AF5_IvernNurturer : public UnitCard {
public:
    AF5_IvernNurturer() : UnitCard(613) {}

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);

        // Peek the top 3 (top = back of vector). We do not pop yet so that
        // a confirmOptional suspend/resume doesn't lose the peeked set; the
        // pop happens after the agent decides.
        std::vector<GameObjectId> peeked;
        for (int i = 0; i < 3; ++i) {
            int idx = static_cast<int>(ps.main_deck.size()) - 1 - i;
            if (idx < 0) break;
            peeked.push_back(ps.main_deck[idx]);
        }
        if (peeked.empty()) return;

        // Is there a unit among the peeked cards?
        auto has_unit = [&]() -> bool {
            for (auto id : peeked) {
                if (!ctx.state.objectExists(id)) continue;
                if (ctx.state.getObject(id).isUnit()) return true;
            }
            return false;
        };

        // "You may reveal a unit ... and draw it." Optional.
        int conf = confirmOptional(ctx,
            "Ivern, Nurturer: reveal a unit from top 3 and draw it?", has_unit);
        if (conf == -1) return;  // waiting for agent

        // Now actually remove the peeked cards from the top of the deck.
        std::vector<GameObjectId> top3;
        for (int i = 0; i < 3 && !ps.main_deck.empty(); ++i) {
            top3.push_back(ps.main_deck.back());
            ps.main_deck.pop_back();
        }

        GameObjectId drafted = kInvalidId;
        std::vector<GameObjectId> rest;
        bool themed = false;

        if (conf == 1) {
            // Draft the first unit; rest get recycled.
            for (auto id : top3) {
                if (!ctx.state.objectExists(id)) { rest.push_back(id); continue; }
                const auto& obj = ctx.state.getObject(id);
                if (drafted == kInvalidId && obj.isUnit()) {
                    drafted = id;
                } else {
                    rest.push_back(id);
                }
            }
        } else {
            // Declined the reveal/draw — everything is recycled.
            rest = top3;
        }

        if (drafted != kInvalidId) {
            const auto& drafted_obj = ctx.state.getObject(drafted);
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            ctx.events.logTrace("IVERN NURTURER: drafted " + drafted_obj.name);
            // Move it to hand directly (already popped off the deck).
            auto& obj = ctx.state.getObject(drafted);
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(drafted);

            for (const auto& t : drafted_obj.tags) {
                if (t == "Bird" || t == "Cat" || t == "Dog" || t == "Poro") {
                    themed = true; break;
                }
            }
        }

        // Recycle the rest to the bottom of the deck.
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
        }

        // Themed reveal → [Buff] a friendly unit (+1 buff counter).
        if (themed) {
            for (auto& [oid, u] : ctx.state.objects) {
                if (!u.isUnit() || u.controller != ctx.controller) continue;
                if (!u.location.has_value()) continue;
                ctx.executor.buffUnit(oid);
                ctx.events.logTrace("IVERN NURTURER: themed draft — buff " +
                                     u.name);
                break;
            }
        }
    }
};

// ─── [644] Lillia, Fae Fawn ──────────────────────────────────────────────────
//
// "[Accelerate] ... When I move from a location, play a 3 [M] Sprite unit
//  token with [Temporary] there."
//
// Fix: token now spawns at the location involved in the move (the source's
// current location after the move) rather than always the controller's base.
// The WhenIMove trigger fires post-move, so `ctx.source.location` is the
// destination — that's the best "there" available without the engine passing
// the move's `from` location into onTrigger.
class AF5_LilliaFaeFawn : public UnitCard {
public:
    AF5_LilliaFaeFawn() : UnitCard(644) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc = BaseLocation{ctx.controller};  // fallback
        if (ctx.state.objectExists(ctx.source)) {
            const auto& self = ctx.state.getObject(ctx.source);
            if (self.location.has_value()) loc = *self.location;
        }
        KeywordSet kw; kw.set(Keyword::Temporary);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite",
                                 /*might=*/3, /*tags=*/{"Fae"}, kw, loc,
                                 /*enter_ready=*/true);
        ctx.events.logTrace("LILLIA: creates 3M Sprite token (Temporary) at "
                            "move location");
    }
};

// ─── [682] Rengar, Trophy Hunter ─────────────────────────────────────────────
//
// "[Ambush] ... I can [Ambush] to a battlefield where there are enemy units,
//  even if you don't have units there."
//
// The base [Ambush] keyword is engine-handled. The EXTENSION — allowing
// Ambush to a BF where only ENEMY units are present (no friendly units) — is
// hardcoded in GameEngine::generateClosedStateActions, which gates Ambush
// play targets on `bf.hasUnitsFrom(player)`. There is no per-card virtual
// hook to relax that gate, and editing the engine is out of scope for this
// task. NOT implemented — see report. (Kept as an explicit override so this
// card id resolves to a defined Card with the correct identity.)
class AF5_RengarTrophy : public UnitCard {
public:
    AF5_RengarTrophy() : UnitCard(682) {}
};

// ─── [739] Ivern, Friend to All ──────────────────────────────────────────────
//
// "As you play me, choose Bird, Cat, Dog, or Poro. I gain that tag.
//  When I conquer or hold, score 1 point if your units have all of the
//  following tags among them — Bird, Cat, Dog, and Poro."
//
// Fix: the tag choice is now an AGENT decision (pickMode) instead of a
// hardcoded "Bird". We run it on the AsYouPlayMe trigger (which resolves on
// the chain, so pickMode surfaces a real choice; in a non-chain context it
// falls back to the first mode = Bird). Scoring uses the all-4-tag union.
class AF5_IvernFriend : public UnitCard {
public:
    AF5_IvernFriend() : UnitCard(739) {}

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::AsYouPlayMe, TriggerType::WhenIConquerOrHold};
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::AsYouPlayMe) {
            doChooseTag(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIConquerOrHold) {
            doScore(ctx);
        }
    }

private:
    void doChooseTag(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return;
        static const std::vector<std::string> kTags = {"Bird", "Cat", "Dog",
                                                        "Poro"};
        int mode = pickMode(ctx, "Ivern, Friend to All: choose a tag",
                            /*num_modes=*/4, kTags);
        if (mode == -1) return;  // waiting for agent choice
        if (mode < 0 || mode >= 4) mode = 0;
        const std::string& chosen = kTags[mode];
        auto& self = ctx.state.getObject(ctx.source);
        auto& tags = self.tags;
        if (std::find(tags.begin(), tags.end(), chosen) == tags.end()) {
            tags.push_back(chosen);
        }
        ctx.events.logTrace("IVERN FRIEND: chose tag [" + chosen + "]");
    }

    void doScore(CardContext& ctx) {
        auto pres = af5_detail::scanFriendlyTags(ctx.state, ctx.controller);
        if (!pres.allFour()) {
            ctx.events.logTrace("IVERN FRIEND: union not satisfied (count=" +
                                 std::to_string(pres.count()) + "/4) — no score");
            return;
        }
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("IVERN FRIEND: all-4-tag union satisfied — score 1 -> " +
                             std::to_string(ps.score));
    }
};

// ─── [769] Gardens of Becoming ───────────────────────────────────────────────
//
// "Units here have \"[E]: Gain 1 XP.\""
//
// This grants an ACTIVATED ability ("[E]: Gain 1 XP") to every unit at this
// battlefield. The engine's activated-ability enumeration
// (GameEngine::generateActivateAbilityActions) only reads abilities from each
// object's OWN Card via card_registry_.get(obj.card_def_id)->activatedAbilities();
// there is no mechanism for a battlefield aura to inject an activated ability
// onto units there (the aura system only grants keywords / might, not
// activated abilities). Implementing this faithfully requires a new engine
// hook (granted-ability auras), which is out of scope. NOT implemented — see
// report. Kept as an explicit override for a defined card identity.
class AF5_GardensOfBecoming : public BattlefieldCard {
public:
    AF5_GardensOfBecoming() : BattlefieldCard(769) {}
};

// ─── Registration ─────────────────────────────────────────────────────────────

void registerAuditFixes5(CardRegistry& registry) {
    registry.registerCard(48,  std::make_unique<AF5_Meditation>());
    registry.registerCard(277, std::make_unique<AF5_MonasteryOfHirana>());
    registry.registerCard(382, std::make_unique<AF5_Svellsongur>());
    registry.registerCard(460, std::make_unique<AF5_EdgeOfNight>());
    registry.registerCard(506, std::make_unique<AF5_FireBelowMtn>());
    registry.registerCard(544, std::make_unique<AF5_YoneBlademaster>());
    registry.registerCard(613, std::make_unique<AF5_IvernNurturer>());
    registry.registerCard(644, std::make_unique<AF5_LilliaFaeFawn>());
    registry.registerCard(682, std::make_unique<AF5_RengarTrophy>());
    registry.registerCard(739, std::make_unique<AF5_IvernFriend>());
    registry.registerCard(769, std::make_unique<AF5_GardensOfBecoming>());
}

} // namespace riftbound
