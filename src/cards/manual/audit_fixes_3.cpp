// Audit-fix override file — batch 3.
// One class per card, prefixed AF3_ for global uniqueness. Registered LAST
// (after generated stubs and earlier manual classes) so these win.
//
// Cards:
//   45  Defy             — counter, cost-gate [4], drop un-printed draw
//   209 Cull the Weak    — each player kills one of their units (agent choice)
//   366 Emperor's Divide — move ANY number of friendly units at a BF to base
//   440 Jax, Unrelenting — Weaponmaster equip + pay [1] draw 1 on attach
//   486 Glasc Mixologist — Deathknell: may play unit cost<=3 from trash free
//   534 Treasure Hoard   — WhenYouConquerHere: may pay [1] -> Gold token (exh)
//   609 Mosstomper       — Hunt 2 + Level 3 static +1M & Deflect
//   642 Hwei             — WhenIMove: draw1/discard1, branch on discarded type
//   675 Master Yi        — Hunt 2 + Level 6 static Deflect & Ganking
//   765 Dusk Rose Lab    — beginning: may kill a unit you control here -> draw 1

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {

// ─── Shared helper: revert a countered play's bookkeeping (CR 425.1.b) ───────
// Mirror of deck_cards.cpp's revertCounteredPlay (which is file-local there).
static void af3_revertCounteredPlay(CardContext& ctx, const ChainItem& top) {
    auto& ps = ctx.state.player(top.controller);
    if (ps.cards_played_this_turn > 0) --ps.cards_played_this_turn;
    if (ps.last_spell_energy_spent == top.total_energy_spent) {
        ps.last_spell_energy_spent = 0;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// [45] Defy — [Reaction] Counter a spell that costs no more than [4] (and [A]).
// Fix: gate on the countered spell's cost (<= 4); drop the un-printed "draw 1".
// ═════════════════════════════════════════════════════════════════════════════
class AF3_Defy : public SpellCard {
public:
    AF3_Defy() : SpellCard(45) {}

    // Playable only if there is a counterable spell on the chain. The [4]
    // cost gate is re-checked at resolve via the CardDB def lookup; only the
    // printed energy cost is modeled ([A] = additional power, not tracked).
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (!it->is_spell) continue;
            if (it->card_def_id == kInvalidId) continue;
            return true;  // cost re-checked at resolve via def lookup
        }
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;

        // Cost gate: "no more than [4]". Read the static energy cost of the
        // spell being countered from the CardDB.
        int cost = 0;
        if (top.card_def_id != kInvalidId) {
            cost = ctx.executor.cardDB().get(top.card_def_id).energy_cost;
        }
        if (cost > 4) {
            ctx.events.logTrace("DEFY: cannot counter — cost " +
                                 std::to_string(cost) + " exceeds [4]");
            return;  // illegal target; spell resolves normally
        }

        auto countered = top.source;
        af3_revertCounteredPlay(ctx, top);  // CR 425.1.b
        ctx.state.chain.items.pop_back();
        if (ctx.state.objectExists(countered)) {
            auto& obj = ctx.state.getObject(countered);
            ctx.events.logTrace("COUNTER: " + obj.name + " countered by Defy -> trash");
            obj.zone = ZoneType::Trash;
            obj.location = std::nullopt;
            ctx.state.player(obj.owner).trash.push_back(countered);
        }
        // NOTE: removed the previously-present (un-printed) "draw 1".
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [209] Cull the Weak — Each player kills one of their units.
// Fix: controller chooses their own unit via pickTarget. The opponent's pick
// is modeled as their first available unit (the engine's choice helpers only
// plumb the controller's decision — see report).
// ═════════════════════════════════════════════════════════════════════════════
class AF3_CullTheWeak : public SpellCard {
public:
    AF3_CullTheWeak() : SpellCard(209) {}

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Controller's own kill — agent-chosen via pickTarget.
        std::vector<GameObjectId> own;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value()) {
                own.push_back(id);
            }
        }
        if (!own.empty()) {
            GameObjectId picked = pickTarget(ctx, "Cull the Weak (your unit)", own);
            // Suspended for agent decision — chain manager re-enters.
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;
            }
            if (picked != kInvalidId && ctx.state.objectExists(picked)) {
                ctx.executor.killObject(picked);
            }
        }

        // Opponent's own kill — first available (opponent choice not plumbable).
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == opp && obj.location.has_value()) {
                ctx.executor.killObject(id);
                break;
            }
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [366] Emperor's Divide — Move any number of friendly units at a battlefield
// to their base.
// Fix: choose a battlefield (the play-time target), then iteratively let the
// agent move ANY SUBSET of friendly units there to base (a "done" option
// stops). Previously moved ALL.
// ═════════════════════════════════════════════════════════════════════════════
class AF3_EmperorsDivide : public SpellCard {
public:
    AF3_EmperorsDivide() : SpellCard(366) {}

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true,
                                   .must_be_at_battlefield = true};
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming.value();

        // The battlefield is fixed by the play-time target on case 0.
        auto build_choices = [&](BattlefieldId bf, std::vector<GameObjectId>& units) {
            units.clear();
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                auto ubf = obj.battlefieldId();
                if (ubf && *ubf == bf) units.push_back(id);
            }
            std::vector<Intent> out;
            for (auto uid : units) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {uid};
                out.push_back(std::move(c));
            }
            // Trailing "done" choice (empty chosen_objects).
            Intent done;
            done.type = IntentType::MakeChoice;
            done.player = ctx.controller;
            out.push_back(std::move(done));
            return out;
        };

        switch (ri.resume_point) {
        case 0: {
            if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
            auto bf_opt = ctx.state.getObject(targets[0]).battlefieldId();
            if (!bf_opt) return;
            BattlefieldId bf = *bf_opt;
            std::vector<GameObjectId> units;
            auto choices = build_choices(bf, units);
            if (choices.size() <= 1) return;  // no friendly units here
            // Stash the battlefield id for re-entry.
            ri.resume_data = {static_cast<int32_t>(bf)};
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Emperor's Divide: move a unit to base, or stop");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            BattlefieldId bf = static_cast<BattlefieldId>(ri.resume_data[0]);

            // "done" = empty chosen_objects.
            if (!choice || choice->chosen_objects.empty()) return;
            auto uid = choice->chosen_objects[0];
            if (ctx.state.objectExists(uid)) {
                ctx.executor.moveToBase(uid);
                ctx.events.logTrace("EMPEROR'S DIVIDE: moved a unit to base");
            }
            // Offer the remaining units (+ done) again.
            std::vector<GameObjectId> units;
            auto choices = build_choices(bf, units);
            if (choices.size() <= 1) return;  // none left
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "Emperor's Divide: move a unit to base, or stop");
            // stay at resume_point = 1
            return;
        }
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [440] Jax, Unrelenting — [Weaponmaster] + "When you attach an Equipment to
// me, you may pay [1] to draw 1."
// Reproduces the Weaponmaster equip-on-play (engine has no separate handler;
// it lives in the Card), then, if a gear was attached to Jax, offers the
// optional pay-[1]-draw-1 rider.
// (No engine event exists for a LATER equip, so the rider is offered for the
//  Weaponmaster equip that happens here — see report.)
// ═════════════════════════════════════════════════════════════════════════════
class AF3_JaxUnrelenting : public UnitCard {
public:
    AF3_JaxUnrelenting() : UnitCard(440) {}

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();

        switch (ri.resume_point) {
        case 0: {
            if (!ctx.state.objectExists(ctx.source)) return;

            // ── Weaponmaster: equip one of your Equipment to me ──
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

            if (best_gear != kInvalidId) {
                auto& gear = ctx.state.getObject(best_gear);
                // Detach from current unit ("even if already attached").
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
                ctx.events.emit(ObjectStateChangedEvent{best_gear, "attached"});
                ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
                ctx.events.logTrace("JAX, UNRELENTING: equipped " + gear.name);
            } else {
                // Nothing equipped — nothing attached, so no rider.
                return;
            }
            // Fall through to the optional rider below.
            break;
        }
        }

        // ── Rider: "you may pay [1] to draw 1" (an Equipment was attached) ──
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Jax, Unrelenting: pay [1] to draw 1?",
                                    still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / no energy
        ps.rune_pool.energy -= 1;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("JAX, UNRELENTING: paid [1] -> draw 1");
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [486] Glasc Mixologist — [Deathknell] You may play a unit with cost no more
// than [3] (and [A]) from your trash, ignoring its cost.
// Fix: cost gate (energy cost <= 3) + "you may" optionality + agent choice of
// which eligible unit to play.
// ═════════════════════════════════════════════════════════════════════════════
class AF3_GlascMixologist : public UnitCard {
public:
    AF3_GlascMixologist() : UnitCard(486) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }

    // Eligible units in trash: card_type Unit with printed energy cost <= 3.
    std::vector<GameObjectId> eligibleTrashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            int cost = ctx.executor.cardDB().get(obj.card_def_id).energy_cost;
            if (cost > 3) continue;
            out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() {
            return !eligibleTrashUnits(ctx).empty();
        };
        int conf = confirmOptional(ctx,
            "Glasc Mixologist: play a <=3-cost unit from trash?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing eligible

        auto eligible = eligibleTrashUnits(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Glasc Mixologist (unit from trash)",
                                          eligible);
        // Suspended for agent decision.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Remove from trash, then play ignoring cost.
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("GLASC MIXOLOGIST: played a unit from trash for free");
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [534] Treasure Hoard — When you conquer here, you may pay [1] to play a Gold
// gear token exhausted.
// ═════════════════════════════════════════════════════════════════════════════
class AF3_TreasureHoard : public BattlefieldCard {
public:
    AF3_TreasureHoard() : BattlefieldCard(534) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx,
            "Treasure Hoard: pay [1] for a Gold token?", still_legal);
        if (conf == -1) return;
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;

        // "here" = the battlefield this card lives at; place the token there.
        LocationId loc{BaseLocation{ctx.controller}};
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) {
                loc = LocationId{BattlefieldLocation{bf.id}};
                break;
            }
        }
        auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                             0, {}, {}, loc, /*enter_ready=*/false);
        // "exhausted" — gear normally enters ready, so force-exhaust.
        if (ctx.state.objectExists(tok)) {
            ctx.state.getObject(tok).is_exhausted = true;
        }
        ctx.events.logTrace("TREASURE HOARD: paid [1] -> Gold gear token (exhausted)");
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [609] Mosstomper — [Hunt 2] + [Level 3][>] I have +1 [M] and [Deflect].
// Fix: Hunt 2 (gain 2 XP on conquer/hold) + ONGOING static Level-3 buff via
// applyPassiveAura (was a one-shot on-play buff).
// ═════════════════════════════════════════════════════════════════════════════
class AF3_Mosstomper : public UnitCard {
public:
    AF3_Mosstomper() : UnitCard(609) {}

    // [Hunt 2]
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MOSSTOMPER: Hunt 2 -> +2 XP (now " +
                             std::to_string(ctx.state.player(ctx.controller).xp) + ")");
    }

    // [Level 3] static: +1 [M] and [Deflect] while controller has 3+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 3) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            GameObject::AuraEffect might;
            might.source = id;
            might.might_bonus = 1;
            obj.aura_effects.push_back(might);
            GameObject::AuraEffect deflect;
            deflect.source = id;
            deflect.keyword = Keyword::Deflect;
            obj.aura_effects.push_back(deflect);
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [642] Hwei, Brooding Painter — When I move, draw 1, then discard 1. Then,
// based on the discarded card's type:
//   Spell — Draw 1. Gear — Ready up to 2 runes. Unit — Give me +3 [M] this turn.
// ═════════════════════════════════════════════════════════════════════════════
class AF3_HweiBroodingPainter : public UnitCard {
public:
    AF3_HweiBroodingPainter() : UnitCard(642) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);

        auto build_choices = [&]() {
            std::vector<Intent> out;
            for (auto cid : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {cid};
                out.push_back(std::move(c));
            }
            return out;
        };

        switch (ri.resume_point) {
        case 0: {
            // Draw 1, then prompt the discard.
            ctx.executor.drawCards(ctx.controller, 1);
            if (ps.hand.empty()) return;  // nothing to discard, no branch
            ctx.executor.requestChoice(ctx.controller, build_choices(),
                                        "Hwei: discard 1");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) return;
            auto cid = choice->chosen_objects[0];
            if (!ctx.state.objectExists(cid)) return;

            // Capture the discarded card's type BEFORE it leaves hand.
            CardType dtype = ctx.state.getObject(cid).card_type;
            ctx.executor.applyDiscard(ctx.controller, cid);

            // Branch on the discarded card's type.
            if (dtype == CardType::Spell) {
                ctx.executor.drawCards(ctx.controller, 1);
                ctx.events.logTrace("HWEI: discarded Spell -> draw 1");
            } else if (dtype == CardType::Gear) {
                // Ready up to 2 runes (channelRunes brings runes in ready;
                // here we ready up to 2 already-exhausted runes belonging to
                // the controller).
                int readied = 0;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (readied >= 2) break;
                    if (obj.isRune() && obj.controller == ctx.controller &&
                        obj.is_exhausted) {
                        ctx.executor.readyObject(id);
                        ++readied;
                    }
                }
                ctx.events.logTrace("HWEI: discarded Gear -> readied " +
                                     std::to_string(readied) + " rune(s)");
            } else if (dtype == CardType::Unit) {
                if (ctx.state.objectExists(ctx.source)) {
                    ctx.executor.giveTemporaryMight(ctx.source, 3);
                    ctx.events.logTrace("HWEI: discarded Unit -> +3 [M] this turn");
                }
            }
            return;
        }
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [675] Master Yi, Tempered — [Hunt 2] + [Level 6][>] I have [Deflect] and
// [Ganking]. Fix: Hunt 2 (gain 2 XP) + ONGOING static Level-6 keywords via
// applyPassiveAura (was a one-shot on-play set).
// ═════════════════════════════════════════════════════════════════════════════
class AF3_MasterYi : public UnitCard {
public:
    AF3_MasterYi() : UnitCard(675) {}

    // [Hunt 2]
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MASTER YI: Hunt 2 -> +2 XP (now " +
                             std::to_string(ctx.state.player(ctx.controller).xp) + ")");
    }

    // [Level 6] static: [Deflect] and [Ganking] while controller has 6+ XP.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (state.player(controller).xp < 6) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            GameObject::AuraEffect deflect;
            deflect.source = id;
            deflect.keyword = Keyword::Deflect;
            obj.aura_effects.push_back(deflect);
            GameObject::AuraEffect ganking;
            ganking.source = id;
            ganking.keyword = Keyword::Ganking;
            obj.aura_effects.push_back(ganking);
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// [765] Dusk Rose Lab — At the start of your Beginning Phase, you may kill a
// unit you control here to draw 1.
// Fix: "you may" optionality + agent choice of which unit + the draw 1.
// ═════════════════════════════════════════════════════════════════════════════
class AF3_DuskRoseLab : public BattlefieldCard {
public:
    AF3_DuskRoseLab() : BattlefieldCard(765) {}
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }

    // Units the controller controls AT this battlefield.
    std::vector<GameObjectId> unitsHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        // Find this BF's id.
        BattlefieldId my_bf = kInvalidId;
        bool found = false;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { my_bf = bf.id; found = true; break; }
        }
        if (!found) return out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto ubf = obj.battlefieldId();
            if (ubf && *ubf == my_bf) out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() { return !unitsHere(ctx).empty(); };
        int conf = confirmOptional(ctx,
            "Dusk Rose Lab: kill a unit here to draw 1?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / no units here

        auto units = unitsHere(ctx);
        if (units.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Dusk Rose Lab (unit to kill)", units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.killObject(picked);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("DUSK ROSE LAB: killed a unit here -> draw 1");
    }
};

// [735] Sacrifice — "As an additional cost to play this, kill a friendly
// [Mighty] unit (5+ M). Draw 2 and channel 1 rune exhausted." The additional
// cost can't be paid at the engine's play-time cost cursor, so we approximate:
// the spell is only legal when a friendly Mighty unit exists (needsPlayTimeTarget
// + custom legal-target enumeration), and the agent chooses WHICH Mighty unit
// to kill at resolve. If no Mighty unit remains the spell fizzles (the
// mandatory cost is unpaid) rather than granting its upside for free.
class AF3_Sacrifice : public SpellCard {
public:
    AF3_Sacrifice() : SpellCard(735) {}
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
            const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.controller == controller &&
                obj.location.has_value() && obj.current_might >= 5) {
                out.push_back(id);
            }
        }
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !enumerateLegalTargets(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId victim = pickTarget(ctx, "Sacrifice: kill a Mighty unit", legal);
        if (victim == kInvalidId) return;  // no Mighty unit / suspended
        if (!ctx.state.objectExists(victim)) return;
        ctx.executor.killObject(victim);  // additional cost paid
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("SACRIFICE: killed a Mighty unit -> draw 2 + channel 1");
    }
};

// ─── Registration ─────────────────────────────────────────────────────────────

void registerAuditFixes3(CardRegistry& registry) {
    registry.registerCard(45,  std::make_unique<AF3_Defy>());
    registry.registerCard(735, std::make_unique<AF3_Sacrifice>());
    registry.registerCard(209, std::make_unique<AF3_CullTheWeak>());
    registry.registerCard(366, std::make_unique<AF3_EmperorsDivide>());
    registry.registerCard(440, std::make_unique<AF3_JaxUnrelenting>());
    registry.registerCard(486, std::make_unique<AF3_GlascMixologist>());
    registry.registerCard(534, std::make_unique<AF3_TreasureHoard>());
    registry.registerCard(609, std::make_unique<AF3_Mosstomper>());
    registry.registerCard(642, std::make_unique<AF3_HweiBroodingPainter>());
    registry.registerCard(675, std::make_unique<AF3_MasterYi>());
    registry.registerCard(765, std::make_unique<AF3_DuskRoseLab>());
}

} // namespace riftbound
