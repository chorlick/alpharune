// Audit-fix override implementations (batch 0).
//
// One class per card, prefixed AF0_ for global uniqueness. Registered LAST
// via registerAuditFixes0(), so these override both the generated stubs and
// any earlier manual implementation for the same card id.
//
// Each card's printed text is implemented as faithfully as the engine API
// allows; gaps that require engine support are documented inline.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {

// ═══════════════════════════════════════════════════════════════════════════
// Local resume-pattern helper: discard 1 (agent-chosen), then run post_fn with
// the discarded card id available. Mirrors deck_cards.cpp::discardThenAct but
// surfaces the chosen card so the post-fn can read its CardDef (Get Excited!
// needs the discarded card's printed Energy cost). Uses resume_point 0/1 and
// resume_data[0] to stash the chosen card id across the agent decision.
// ═══════════════════════════════════════════════════════════════════════════
template <typename PostFn>
static void af0_discardOneThenAct(CardContext& ctx, const std::string& label,
                                  PostFn&& post_fn) {
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
        if (ps.hand.empty()) {
            // Nothing to discard — effect has no card to read; fizzle.
            post_fn(ctx, kInvalidId);
            return;
        }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        ri.resume_point = 1;
        return;
    }
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

// ─── [8] Get Excited! ─────────────────────────────────────────────────────
// "[Action] Discard 1. Deal its Energy cost as damage to a unit at a
//  battlefield. (Ignore its Power cost.)"
// Gap fixed: deal the DISCARDED card's printed Energy cost, not a hardcoded 1.
class AF0_GetExcited : public SpellCard {
public:
    AF0_GetExcited() : SpellCard(8) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId target = targets.empty() ? kInvalidId : targets[0];
        GameObjectId source = ctx.source;
        af0_discardOneThenAct(ctx, "Get Excited!: discard 1",
            [target, source](CardContext& c, GameObjectId discarded) {
                if (target == kInvalidId || !c.state.objectExists(target)) return;
                if (discarded == kInvalidId) return;
                // Read the discarded card's printed Energy cost from its def.
                const auto& obj = c.state.getObject(discarded);
                int dmg = 0;
                if (obj.card_def_id != kInvalidId) {
                    dmg = c.executor.cardDB().get(obj.card_def_id).energy_cost;
                }
                if (dmg <= 0) return;
                c.executor.dealDamage(target, dmg, source);
                c.events.logTrace("GET EXCITED!: dealt " + std::to_string(dmg) +
                                  " (discarded card Energy cost)");
            });
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
};

// ─── [116] Thousand-Tailed Watcher ─────────────────────────────────────────
// "[Accelerate] When you play me, give enemy units -3 [M] this turn, to a
//  minimum of 1 [M]."
// Gap fixed: AoE on trigger (empty targets) — self-select ALL enemy units and
// apply -3 with a floor of 1. Accelerate is engine-handled.
class AF0_ThousandTailedWatcher : public UnitCard {
public:
    AF0_ThousandTailedWatcher() : UnitCard(116) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            enemies.push_back(id);
        }
        for (auto id : enemies) {
            // -3 might this turn, minimum 1.
            ctx.executor.giveTemporaryMight(id, -3, /*minimum=*/1);
        }
        ctx.events.logTrace("THOUSAND-TAILED WATCHER: enemy units -3M (min 1) this turn");
    }
};

// ─── [331] Sentinel Adept ──────────────────────────────────────────────────
// "[Weaponmaster] (When you play me, you may [Equip] one of your Equipment to
//  me for [A] less, even if it's already attached.)"
// Gap fixed: offer an agent choice of WHICH equipment to attach (previously
// auto-picked the first). "for [A] less" remains modeled as a free attach: the
// engine's cost-payment cursor can't be driven from inside a resolving trigger
// (same standing limitation as the WeaponmasterUnit base / Last Rites).
class AF0_SentinelAdept : public UnitCard {
public:
    AF0_SentinelAdept() : UnitCard(331) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // Collect the controller's Equipment gear on board (attached or not).
        std::vector<GameObjectId> equipment;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isGear() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            bool is_equipment = false;
            for (auto& tag : obj.tags) {
                if (tag == "Equipment") { is_equipment = true; break; }
            }
            if (!is_equipment) continue;
            equipment.push_back(id);
        }
        auto still_legal = [&equipment]() { return !equipment.empty(); };
        if (!still_legal()) return;

        // "you may" — optional.
        int conf = confirmOptional(ctx, "Sentinel Adept: equip an Equipment to me?",
                                    still_legal);
        if (conf < 1) return;

        GameObjectId gear_id = pickTarget(ctx, "Sentinel Adept: pick Equipment", equipment);
        if (gear_id == kInvalidId || !ctx.state.objectExists(gear_id)) return;

        auto& gear = ctx.state.getObject(gear_id);

        // "even if it's already attached" — detach from current unit first.
        if (gear.attached_to.has_value()) {
            auto old_unit = *gear.attached_to;
            if (ctx.state.objectExists(old_unit)) {
                auto& old = ctx.state.getObject(old_unit);
                old.attachment_might_bonus -= gear.might_bonus;
                auto it = std::find(old.attachments.begin(), old.attachments.end(), gear_id);
                if (it != old.attachments.end()) old.attachments.erase(it);
                old.recomputeMight();
            }
            gear.attached_to = std::nullopt;
        }

        auto& self = ctx.state.getObject(ctx.source);
        gear.attached_to = ctx.source;
        self.attachments.push_back(gear_id);
        gear.location = self.location;
        gear.zone = self.zone;
        self.attachment_might_bonus += gear.might_bonus;
        self.recomputeMight();

        ctx.events.emit(ObjectStateChangedEvent{gear_id, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "equipped"});
        ctx.events.logTrace("SENTINEL ADEPT: equipped " + gear.name +
                            " (free / [A] less)");
    }
};

// ─── [412] The Zero Drive ──────────────────────────────────────────────────
// "[Equip] [1][B] ([1][B]: Attach this to a unit you control.)
//  [3][B], Banish this: Play all units banished with this, ignoring their
//  costs. (Use only if unattached.)
//  effect_text: [Deathknell] — Banish me. (When I die, get the effect.)"
//
// Implementation:
//  • Equip: standard pay [1][B] power + attach (handled below via the same
//    rune-payment pattern as equip_cards.cpp::standardEquip).
//  • Deathknell — Banish me: structured replacement. When the equipped unit
//    would die, banish it INSTEAD of trashing, record it in this gear's
//    tracked_objects ("units banished with this"). The gear detaches and stays
//    on board (now unattached), enabling the replay ability.
//  • Replay ability ([3][B], Banish this): single activated ability, legality
//    gated to "only if unattached" via hasLegalTargets(). On activate: play
//    every recorded banished unit ignoring costs, then banish this gear.
//
// Gaps: the [B] power portion of both costs isn't separately enforced (the
// activation cost path only pays/checks energy — same limitation across the
// codebase); modeled as energy 1 / energy 3 respectively.
class AF0_ZeroDrive : public GearCard {
public:
    AF0_ZeroDrive() : GearCard(412) {}

    // ── Equip [1][B] ──
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return payEquipAndAttach(ctx, ctx.source, unit, /*energy=*/1, Domain::Mind);
    }

    // ── Deathknell — Banish me (death replacement on the bearer) ──
    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        auto& gear = ctx.state.getObject(ctx.source);
        // Only intercept the death of the unit we're attached to.
        if (!gear.attached_to.has_value() || *gear.attached_to != dying_unit) {
            return false;
        }
        if (!ctx.state.objectExists(dying_unit)) return false;
        ctx.events.logTrace("THE ZERO DRIVE: Deathknell — banish bearer + record");

        // Detach the gear (refund might bonus, clear attachment), so the gear
        // remains on board UNATTACHED — enabling the replay ability.
        ctx.executor.unattachGear(ctx.source);

        // Banish the bearer instead of letting it trash. Record it so the
        // replay ability ("all units banished with this") can replay it.
        ctx.executor.banishObject(dying_unit);
        gear.tracked_objects.push_back(dying_unit);
        return true;  // replacement consumed — unit does not die normally
    }

    // ── Replay ability: [3][B], Banish this — only if unattached ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [3][B] energy/power. The [B] power isn't separately enforced by the
        // activation cost path (engine limitation); banish-this is performed
        // manually in onActivate (no banish_self cost flag exists).
        return {.exhaust = false, .energy = 3};
    }
    // "(Use only if unattached.)" — legality gate. For a single-ability card
    // with no targets, generateActivateAbilityActions consults hasLegalTargets.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        // Find this gear's on-board instance and require it to be unattached
        // with at least one recorded banished unit to replay.
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (!obj.location.has_value()) continue;
            if (obj.attached_to.has_value()) return false;  // attached → illegal
            if (obj.tracked_objects.empty()) return false;  // nothing to replay
            return true;
        }
        return false;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& gear = ctx.state.getObject(ctx.source);
        if (gear.attached_to.has_value()) return;  // guard: only if unattached

        // Snapshot the recorded units; play each ignoring costs.
        std::vector<GameObjectId> to_play = gear.tracked_objects;
        gear.tracked_objects.clear();
        for (auto uid : to_play) {
            if (!ctx.state.objectExists(uid)) continue;
            auto& u = ctx.state.getObject(uid);
            // Remove the duplicate banishment-vector entry before re-zoning,
            // mirroring Dazzling Aurora's banish→play handoff.
            auto& bz = ctx.state.player(u.owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), uid), bz.end());
            ctx.executor.playIgnoringCost(ctx.controller, uid);
            ctx.events.logTrace("THE ZERO DRIVE: replayed banished unit " + u.name);
        }

        // "Banish this" cost — route the gear itself to banishment.
        ctx.executor.banishObject(ctx.source);
    }

private:
    // Pay [energy] ready runes + recycle one matching-domain rune for power,
    // then attach. Mirrors equip_cards.cpp::standardEquip with an upfront
    // affordability pre-check so an unaffordable equip never half-pays.
    static bool payEquipAndAttach(CardContext& ctx, GameObjectId gear_id,
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
            int remaining = energy_cost;
            for (auto& [id, obj] : state.objects) {
                if (remaining <= 0) break;
                if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
                if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                obj.is_exhausted = true;
                remaining--;
            }
        }
        {
            auto& dr = state.getObject(domain_rune);
            dr.location = std::nullopt;
            dr.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), domain_rune);
        }

        auto& gear = state.getObject(gear_id);
        auto& unit = state.getObject(unit_id);
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
};

// ─── [467] Vex, Cheerless ──────────────────────────────────────────────────
// "While I'm in combat, friendly spells cost [1][A] less to a minimum of [1],
//  and enemy spells cost [1][A] more."
// Gap fixed: add the "to a minimum of [1]" floor (min_cost=1) on the friendly
// reduction. Preserves the existing two combat-active cost modifiers.
class AF0_VexCheerless : public UnitCard {
public:
    AF0_VexCheerless() : UnitCard(467) {}
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        PlayerState::CostModifier friendly_mod;
        friendly_mod.energy_reduction = 1;
        friendly_mod.min_cost = 1;            // "to a minimum of [1]"
        friendly_mod.this_turn_only = false;
        friendly_mod.combat_active_only = true;
        friendly_mod.affects_friendly_only = true;
        state.player(controller).cost_modifiers.push_back(friendly_mod);

        PlayerState::CostModifier enemy_mod;
        enemy_mod.energy_increase = 1;
        enemy_mod.this_turn_only = false;
        enemy_mod.combat_active_only = true;
        enemy_mod.affects_enemy_only = true;
        state.player(controller).cost_modifiers.push_back(enemy_mod);
    }
};

// ─── [527] Ornn's Forge (Battlefield) ──────────────────────────────────────
// "While you control this battlefield, the first friendly non-token gear
//  played each turn costs [1] less."
// Implemented as a controller-gated, non-token cost reduction pushed via
// applyPassiveAura when this BF's controller is identified.
//
// Gaps: the engine's CostModifier has no "gear-only" filter and no
// "first per turn" tracking, so this reduces non-token cards more broadly than
// the printed text. We restrict to non-token via affects_non_token_only and to
// the controller via affects_friendly_only; the "gear-only" and "first each
// turn" restrictions cannot be modeled without engine support. min_cost=0 so
// a [0] gear can't go negative.
class AF0_OrnnsForge : public BattlefieldCard {
public:
    AF0_OrnnsForge() : BattlefieldCard(527) {}
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Find the battlefield this card object backs, and apply the reduction
        // only while a player controls it.
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            const auto& bf_obj = state.getObject(bf.card_object_id);
            if (bf_obj.card_def_id != cardDefId()) continue;
            if (!bf.controller.has_value()) continue;
            PlayerId who = *bf.controller;
            PlayerState::CostModifier m;
            m.source = bf.card_object_id;
            m.energy_reduction = 1;
            m.affects_non_token_only = true;
            m.affects_friendly_only = true;
            m.this_turn_only = false;
            state.player(who).cost_modifiers.push_back(m);
            return;
        }
    }
};

// ─── [603] Allay, Eager Admirer ────────────────────────────────────────────
// "[Deflect] While I'm at a battlefield, your other units here have [Deflect]."
// Gap fixed: aura scope is now "your OTHER units at MY battlefield" (not all
// units at the BF). Self is excluded; only friendly units share the BF.
class AF0_AllayEager : public UnitCard {
public:
    AF0_AllayEager() : UnitCard(603) {}
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            if (!self.isAtBattlefield()) continue;     // "While I'm at a battlefield"
            auto bf = self.battlefieldId();
            if (!bf) continue;
            for (auto& [tid, tgt] : state.objects) {
                if (tid == sid) continue;               // "other"
                if (!tgt.isUnit() || tgt.controller != controller) continue; // "your units"
                auto tbf = tgt.battlefieldId();
                if (!tbf || *tbf != *bf) continue;      // "here"
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Deflect;
                tgt.aura_effects.push_back(ae);
            }
        }
    }
};

// ─── [617] Vex, Mocking ────────────────────────────────────────────────────
// "[Shield] [Tank] When you [Stun] an enemy unit at a battlefield, you may move
//  me to that battlefield."
// Gap fixed: replace the WhenIDefend stun-the-attacker approximation with the
// correct WhenYouStun → move-self-to-the-stunned-unit's-battlefield. The
// stunned unit id is captured by TriggerManager into card_counters
// ["__stunned_unit_id"]. Shield/Tank are engine-handled.
class AF0_VexMocking : public UnitCard {
public:
    AF0_VexMocking() : UnitCard(617) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouStun; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__stunned_unit_id");
        if (it == self.card_counters.end()) return;
        auto stunned = static_cast<GameObjectId>(it->second);
        if (!ctx.state.objectExists(stunned)) return;

        // Determine the battlefield of the stunned enemy unit ("at a
        // battlefield" / "that battlefield").
        auto target_bf_of = [&ctx, stunned]() -> std::optional<BattlefieldId> {
            if (!ctx.state.objectExists(stunned)) return std::nullopt;
            return ctx.state.getObject(stunned).battlefieldId();
        };
        auto still_legal = [&]() {
            auto bf = target_bf_of();
            if (!bf) return false;                       // stunned unit must be at a BF
            if (!ctx.state.objectExists(ctx.source)) return false;
            auto& s = ctx.state.getObject(ctx.source);
            // Already there? then there's nothing to move.
            auto cur = s.battlefieldId();
            return !(cur && *cur == *bf);
        };
        if (!still_legal()) return;

        int conf = confirmOptional(ctx, "Vex Mocking: move me to that battlefield?",
                                    still_legal);
        if (conf < 1) return;
        auto bf = target_bf_of();
        if (!bf) return;
        ctx.executor.moveToBattlefield(ctx.source, *bf);
        ctx.events.logTrace("VEX MOCKING: moved to stunned unit's battlefield BF" +
                            std::to_string(*bf));
    }
};

// ─── [652] LeBlanc, Everywhere at Once ─────────────────────────────────────
// "[Backline] Your [Temporary] effects at my battlefield don't trigger."
// Backline is engine-handled. The "Temporary effects don't trigger" passive
// CANNOT be implemented: the engine has no mechanism to suppress triggered
// abilities of Temporary objects at a location (no field / hook on
// GameState/GameObject, and trigger dispatch happens in TriggerManager which
// is a shared file we must not edit). Documented gap; shell preserves the
// card's identity so it's no longer a silent generated stub.
class AF0_LeblancEverywhere : public UnitCard {
public:
    AF0_LeblancEverywhere() : UnitCard(652) {}
};

// ─── [703] Evelynn, Entrancing ─────────────────────────────────────────────
// "[Hidden] [Backline] When you play me from face down on your turn, you may
//  move an enemy unit at a different location to my battlefield."
// Gap fixed: move the chosen enemy to EVELYNN'S battlefield (previously
// moved it to its own base). Self-select an eligible enemy via pickTarget
// (trigger targets are empty). Eligible = enemy unit at a DIFFERENT location
// than Evelynn. Hidden/Backline are engine-handled. The "from face down on
// your turn" gate isn't surfaced as a distinct trigger, so this fires on any
// play of Evelynn (documented approximation, carried over from prior impl).
class AF0_EvelynnEntrancing : public UnitCard {
public:
    AF0_EvelynnEntrancing() : UnitCard(703) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;      // need a destination battlefield
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;

        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            // "at a different location" — exclude enemies already at my BF.
            auto obf = obj.battlefieldId();
            if (obf && *obf == *my_bf) continue;
            legal.push_back(id);
        }
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) return;

        int conf = confirmOptional(ctx, "Evelynn: move an enemy unit to my battlefield?",
                                    still_legal);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Evelynn: pick enemy unit", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, *my_bf);
        ctx.events.logTrace("EVELYNN: pulled " + ctx.state.getObject(picked).name +
                            " to my battlefield BF" + std::to_string(*my_bf));
    }
};

// ─── [749] Bashful Bloom (Legend) ──────────────────────────────────────────
// "[4], [E]: Play a ready 3 [M] Sprite unit token with [Temporary]. This
//  ability costs [1] less for each friendly unit with [Temporary]."
// Token creation preserved. The "[E]" exhaust portion is added (the prior
// impl omitted it).
//
// Gap: the per-Temporary cost reduction CANNOT be implemented. The activated-
// ability cost path (game_engine.cpp ActivateAbility) reads ActivationCost
// ::energy directly with no state-aware reduction hook — selfCostReduction and
// PlayerState::CostModifier only apply to PLAYING cards (canAfford /
// payCardCost), not to activating abilities. getActivationCost() /
// activatedAbilities() are const with no GameState access, so the count of
// friendly [Temporary] units can't be folded into the cost without engine
// support (a state-aware activation-cost hook). Modeled at the flat [4][E].
class AF0_BashfulBloom : public LegendCard {
public:
    AF0_BashfulBloom() : LegendCard(749) {}
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 4};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, /*enter_ready=*/true);
        ctx.events.logTrace("BASHFUL BLOOM: plays ready 3M Sprite (Temporary)");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════════════
void registerAuditFixes0(CardRegistry& registry) {
    registry.registerCard(8,   std::make_unique<AF0_GetExcited>());
    registry.registerCard(116, std::make_unique<AF0_ThousandTailedWatcher>());
    registry.registerCard(331, std::make_unique<AF0_SentinelAdept>());
    registry.registerCard(412, std::make_unique<AF0_ZeroDrive>());
    registry.registerCard(467, std::make_unique<AF0_VexCheerless>());
    registry.registerCard(527, std::make_unique<AF0_OrnnsForge>());
    registry.registerCard(603, std::make_unique<AF0_AllayEager>());
    registry.registerCard(617, std::make_unique<AF0_VexMocking>());
    registry.registerCard(652, std::make_unique<AF0_LeblancEverywhere>());
    registry.registerCard(703, std::make_unique<AF0_EvelynnEntrancing>());
    registry.registerCard(749, std::make_unique<AF0_BashfulBloom>());
}

} // namespace riftbound
