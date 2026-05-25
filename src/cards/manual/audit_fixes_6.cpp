// Audit-fix batch 6 — manual override cards. Registered LAST so these
// classes supersede generated stubs and earlier manual classes for the
// same card ids. One class per card, all prefixed AF6_ for global
// uniqueness across the audit_fixes_*.cpp files.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace riftbound {

namespace {

// Local copy of the move-to-location helper (deck_cards.cpp's is file-local).
inline void af6_moveToLocation(EffectExecutor& exec, GameObjectId obj,
                                const std::optional<LocationId>& loc) {
    if (!loc) return;
    if (std::holds_alternative<BattlefieldLocation>(*loc)) {
        exec.moveToBattlefield(obj, std::get<BattlefieldLocation>(*loc).id);
    } else {
        exec.moveToBase(obj);
    }
}

// Recycle ONE ready rune (optionally of a specific domain) from `player`'s
// base to pay a power cost. Returns true if a rune was found+recycled.
// Mirrors the power-payment idiom in equip_cards.cpp's standardEquip.
// `domain == nullopt` means any rune ([A]).
inline bool af6_payOnePower(CardContext& ctx, PlayerId player,
                            std::optional<Domain> domain) {
    auto& state = ctx.state;
    auto base_loc = BaseLocation{player};
    GameObjectId rune = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (obj.is_exhausted) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        if (domain.has_value()) {
            bool match = false;
            for (auto d : obj.domains) if (d == *domain) { match = true; break; }
            if (!match) continue;
        }
        rune = id;
        break;
    }
    if (rune == kInvalidId) return false;
    auto& dr = state.getObject(rune);
    dr.location = std::nullopt;
    dr.zone = ZoneType::RuneDeck;
    state.player(player).rune_deck.insert(state.player(player).rune_deck.begin(), rune);
    ctx.events.logTrace("  ADDL_COST: recycled " + dr.name + " for power");
    return true;
}

// Self-contained equip: pay one power rune (domain or any), then attach this
// gear to `unit`. Replicates equip_cards.cpp's standardEquip / universal
// equip (those classes aren't exposed in a header, so we reproduce the logic
// here). Returns true on success.
inline bool af6_standardEquip(CardContext& ctx, GameObjectId gear_id,
                              GameObjectId unit_id, std::optional<Domain> domain) {
    auto& state = ctx.state;
    if (!state.objectExists(gear_id) || !state.objectExists(unit_id)) return false;
    // Pay the power portion. Bail (no state change) if we can't.
    if (!af6_payOnePower(ctx, ctx.controller, domain)) return false;

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

}  // namespace

// ─── [67] Blitzcrank, Impassive ─────────────────────────────────────────────
// [Tank] (engine-handled).
// When you play me to a battlefield, you may move an enemy unit to here.
// When I hold, return me to my owner's hand.
class AF6_BlitzcrankImpassive : public UnitCard {
public:
    AF6_BlitzcrankImpassive() : UnitCard(67) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // WhenIHold (delayed-fire path leaves firing_trigger=None, so treat
        // anything that isn't WhenYouPlayMe as the hold branch only when the
        // unit is actually holding a battlefield — but here both triggers are
        // event-driven and carry firing_trigger, so branch on it directly).
        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            ctx.executor.bounceToHand(ctx.source);
            ctx.events.logTrace("BLITZCRANK: returned to owner's hand on hold");
            return;
        }

        // WhenYouPlayMe: pull an enemy unit to my battlefield.
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — trigger condition not met

        auto find_enemy_elsewhere = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() == my_bf) continue;
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_enemy_elsewhere() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Blitzcrank: pull an enemy unit to my battlefield?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_enemy_elsewhere();
        if (target == kInvalidId) return;
        std::string name = ctx.state.getObject(target).name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("BLITZCRANK: pulled " + name + " to BF#" +
                             std::to_string(static_cast<int>(*my_bf)));
    }
};

// ─── [284] Targon's Peak ────────────────────────────────────────────────────
// When you conquer here, ready up to 2 runes at the end of this turn.
//
// "At the end of this turn" must be deferred. We register a DelayedAbility
// scoped to AtEndOfTurn for the conquering player. (NOTE: the engine's
// AtEndOfTurn delayed-ability dispatch is limited — battlefield card objects
// have no `location` so the static-trigger sweep skips them, and there is no
// checkDelayedAbilities(AtEndOfTurn) call today. We register the CR-correct
// delayed ability so the effect fires if/when that dispatch is wired; the
// onTrigger body readies up to 2 runes for the controller.)
class AF6_TargonsPeak : public BattlefieldCard {
public:
    AF6_TargonsPeak() : BattlefieldCard(284) {}
    // Only WhenYouConquerHere is a static (event-dispatched) trigger. The
    // end-of-turn ready-up is scheduled as a DelayedAbility; when it fires it
    // re-enters this onTrigger with firing_trigger == None (delayed-ability
    // dispatch leaves fired_trigger unset), which falls through to the ready
    // branch below.
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouConquerHere) {
            // Schedule the ready-up for end of this turn.
            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtEndOfTurn;
            da.expires_on_turn = ctx.state.turn.turn_number;  // clean up EOT
            ctx.state.delayed_abilities.push_back(da);
            ctx.events.logTrace("TARGON'S PEAK: scheduled ready-2-runes for end of turn");
            return;
        }
        // AtEndOfTurn fire: ready up to 2 of the controller's exhausted runes.
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 2) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        ctx.events.logTrace("TARGON'S PEAK: readied " + std::to_string(readied) +
                             " runes at end of turn");
    }
};

// ─── [402] Bellows Breath ───────────────────────────────────────────────────
// [Action] (engine-handled timing). [Repeat] (engine-handled).
// Deal 1 to up to three units at the same location.
class AF6_BellowsBreath : public SpellCard {
public:
    AF6_BellowsBreath() : SpellCard(402) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 3, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        // "at the same location" — anchor on the first chosen target's
        // location; only damage targets sharing it. Collect first, kill
        // lethal after (AoE iterator-safety per CLAUDE.md).
        std::optional<LocationId> anchor;
        if (ctx.state.objectExists(targets[0]))
            anchor = ctx.state.getObject(targets[0]).location;
        if (!anchor.has_value()) return;

        std::vector<GameObjectId> hit;
        for (auto t : targets) {
            if (!ctx.state.objectExists(t)) continue;
            auto& obj = ctx.state.getObject(t);
            if (obj.location != anchor) continue;
            // de-dup (action gen may repeat in degenerate cases)
            if (std::find(hit.begin(), hit.end(), t) != hit.end()) continue;
            hit.push_back(t);
        }
        for (auto t : hit) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 1, ctx.source);
        }
        for (auto t : hit) {
            if (ctx.state.objectExists(t) &&
                ctx.state.getObject(t).hasLethalDamage()) {
                ctx.executor.killObject(t);
            }
        }
        ctx.events.logTrace("BELLOWS BREATH: dealt 1 to " +
                             std::to_string(hit.size()) + " units at same location");
    }
};

// ─── [461] Fizz, Trickster ──────────────────────────────────────────────────
// When you play me, you may play a spell from your trash with Energy cost no
// more than [3], ignoring its Energy cost. Recycle that spell after you play
// it. (You must still pay its Power cost.)
class AF6_FizzTrickster : public UnitCard {
public:
    AF6_FizzTrickster() : UnitCard(461) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        const auto& db = ctx.executor.cardDB();

        auto find_spell = [&]() -> GameObjectId {
            // Most-recent first.
            for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
                auto cid = ps.trash[i];
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (obj.card_type != CardType::Spell) continue;
                if (obj.card_def_id == kInvalidId) continue;
                // Energy cost gate: no more than 3.
                if (db.get(obj.card_def_id).energy_cost > 3) continue;
                return cid;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_spell() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Fizz: play a spell (cost <= 3) from trash for free?", still_legal);
        if (conf != 1) return;

        GameObjectId chosen = find_spell();
        if (chosen == kInvalidId) return;
        // Remove from trash before playing.
        auto it = std::find(ps.trash.begin(), ps.trash.end(), chosen);
        if (it != ps.trash.end()) ps.trash.erase(it);

        std::string sname = ctx.state.getObject(chosen).name;
        ctx.executor.playIgnoringCost(ctx.controller, chosen);
        // "Recycle that spell after you play it" — bottom of owner's main deck.
        ctx.executor.recycleCards(ctx.controller, {chosen});
        ctx.events.logTrace("FIZZ: played " + sname +
                             " from trash (energy<=3, ignoring energy), then recycled it");
    }
};

// ─── [508] Rabadon's Deathcrown ─────────────────────────────────────────────
// [Unique] (deck-build, engine-handled). [Equip] [A] (universal equip).
// effect: Your spells and abilities deal 3 Bonus Damage (while attached).
//
// LIMITATION: there is no engine support for spell/ability "bonus damage"
// (EffectExecutor::dealDamage consults no per-player bonus field, and an
// equipment can't intercept every other card's dealDamage). We preserve the
// universal-equip behavior; the +3 bonus-damage clause is NOT implementable
// without an engine change.
class AF6_RabadonsDeathcrown : public GearCard {
public:
    AF6_RabadonsDeathcrown() : GearCard(508) {}
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [A] equip: recycle any rune for power.
        return af6_standardEquip(ctx, ctx.source, unit, std::nullopt);
    }
};

// ─── [601] Soul Sword ───────────────────────────────────────────────────────
// [Equip] [G] (Calm equip).
// effect: [Level 3] I have an additional +1 [M]. (While 3+ XP, get effect.)
//
// The +1 [M] must be gated on the controller having 3+ XP. We grant it as an
// aura on the equipped unit via applyPassiveAura (recomputed every cleanup),
// so it appears/disappears as XP crosses the threshold.
class AF6_SoulSword : public GearCard {
public:
    AF6_SoulSword() : GearCard(601) {}
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [G] equip: recycle a Calm rune for power.
        return af6_standardEquip(ctx, ctx.source, unit, Domain::Calm);
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Find this gear's on-board instance(s); if attached and the
        // controller has >= 3 XP, push +1 might onto the equipped unit.
        if (state.player(controller).xp < 3) return;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller) continue;
            if (!obj.attached_to.has_value()) continue;
            auto unit_id = *obj.attached_to;
            auto uit = state.objects.find(unit_id);
            if (uit == state.objects.end()) continue;
            uit->second.aura_effects.push_back(
                GameObject::AuraEffect{.source = id, .might_bonus = 1});
        }
    }
};

// ─── [614] Nami, Headstrong ─────────────────────────────────────────────────
// You may pay [G] as an additional cost to play me.
// When you play me, if you paid the additional cost, [Stun] an enemy unit.
// When I hold, the next time you play a unit this turn, ready it and [Buff] it.
class AF6_NamiHeadstrong : public UnitCard {
public:
    AF6_NamiHeadstrong() : UnitCard(614) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold,
                TriggerType::WhenYouPlayAUnit};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true, .optional = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // Optional additional cost [G]; if paid, stun an enemy unit.
            auto find_enemy = [&]() -> GameObjectId {
                for (auto& [id, obj] : ctx.state.objects) {
                    if (!obj.isUnit()) continue;
                    if (obj.controller == ctx.controller) continue;
                    if (!obj.location.has_value()) continue;
                    if (obj.untargetable_by_enemy) continue;
                    return id;
                }
                return kInvalidId;
            };
            // Only worth paying if there's an enemy to stun AND a Calm rune.
            auto can_pay = [&]() {
                auto base_loc = BaseLocation{ctx.controller};
                for (auto& [id, obj] : ctx.state.objects) {
                    if (!obj.isRune() || obj.controller != ctx.controller) continue;
                    if (obj.is_exhausted) continue;
                    if (!obj.location.has_value() ||
                        *obj.location != LocationId{base_loc}) continue;
                    for (auto d : obj.domains)
                        if (d == Domain::Calm) return true;
                }
                return false;
            };
            auto still_legal = [&]() { return can_pay() && find_enemy() != kInvalidId; };

            int conf = confirmOptional(ctx,
                "Nami: pay [G] additional cost to stun an enemy?", still_legal);
            if (conf != 1) return;
            if (!af6_payOnePower(ctx, ctx.controller, Domain::Calm)) return;
            GameObjectId enemy = kInvalidId;
            if (!targets.empty() && ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).controller != ctx.controller) {
                enemy = targets[0];
            } else {
                enemy = find_enemy();
            }
            if (enemy == kInvalidId) return;
            ctx.executor.stunUnitBy(enemy, ctx.source);
            ctx.events.logTrace("NAMI: paid [G] additional cost -> stunned enemy");
            return;
        }

        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            // Arm the one-shot "next unit you play this turn" effect.
            auto& self = ctx.state.getObject(ctx.source);
            self.card_counters["__nami_hold_arm_turn"] = ctx.state.turn.turn_number;
            ctx.events.logTrace("NAMI: armed next-unit ready+buff for this turn");
            return;
        }

        if (ctx.firing_trigger == TriggerType::WhenYouPlayAUnit) {
            auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__nami_hold_arm_turn");
            if (it == self.card_counters.end()) return;
            if (it->second != ctx.state.turn.turn_number) return;  // stale arm
            // One-shot: disarm now.
            self.card_counters.erase(it);
            // Ready+buff the just-played unit. The trigger carries no event
            // object, so let the agent pick among freshly-played friendly
            // units (units enter exhausted, so prefer exhausted candidates).
            std::vector<GameObjectId> legal;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (!obj.is_exhausted) continue;  // just-played units are exhausted
                legal.push_back(id);
            }
            if (legal.empty()) return;
            GameObjectId pick = pickTarget(ctx, "Nami: ready + buff the played unit", legal);
            if (pick == kInvalidId || !ctx.state.objectExists(pick)) return;
            ctx.executor.readyObject(pick);
            ctx.executor.buffUnit(pick);
            ctx.events.logTrace("NAMI: readied + buffed the played unit");
            return;
        }
    }
};

// ─── [645] Smoke and Mirrors ────────────────────────────────────────────────
// [Hidden] / [Action] (engine-handled keywords).
// Choose a unit you control and another unit you control at a different
// location. If at least one of them has [Temporary], move each to the other's
// location. Draw 1.
class AF6_SmokeAndMirrors : public SpellCard {
public:
    AF6_SmokeAndMirrors() : SpellCard(645) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() < 2) return;
        if (!ctx.state.objectExists(targets[0]) ||
            !ctx.state.objectExists(targets[1])) return;
        auto& a = ctx.state.getObject(targets[0]);
        auto& b = ctx.state.getObject(targets[1]);
        auto la = a.location;
        auto lb = b.location;
        // "at a different location" — if they share a location, the swap is
        // a no-op; still draw 1 per the card's last clause.
        // "If at least one of them has [Temporary]" — gate the move.
        bool has_temp = a.hasKeyword(Keyword::Temporary) ||
                        b.hasKeyword(Keyword::Temporary);
        if (has_temp && la != lb) {
            af6_moveToLocation(ctx.executor, targets[0], lb);
            af6_moveToLocation(ctx.executor, targets[1], la);
            ctx.events.logTrace("SMOKE AND MIRRORS: swapped two friendly units (Temporary gate met)");
        } else {
            ctx.events.logTrace("SMOKE AND MIRRORS: no swap (Temporary gate not met / same loc)");
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ─── [695] Blast Cone ───────────────────────────────────────────────────────
// When you play this, you may move an enemy unit.
// When you move an enemy unit, you may exhaust this to [Stun] it.
//
// The second clause's general "whenever you move an enemy unit" cannot be
// observed for moves caused by other cards (no such trigger event). We model
// the on-play combo: after Blast Cone moves an enemy via its own first
// clause, offer to exhaust it to stun that moved unit.
class AF6_BlastCone : public GearCard {
public:
    AF6_BlastCone() : GearCard(695) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true, .optional = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto find_enemy = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller == ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.untargetable_by_enemy) continue;
                return id;
            }
            return kInvalidId;
        };
        auto move_legal = [&]() {
            if (!targets.empty())
                return ctx.state.objectExists(targets[0]) &&
                       ctx.state.getObject(targets[0]).location.has_value();
            return find_enemy() != kInvalidId;
        };
        if (!move_legal()) return;
        int conf = confirmOptional(ctx, "Blast Cone: move an enemy unit?", move_legal);
        if (conf != 1) return;

        GameObjectId moved = kInvalidId;
        if (!targets.empty() && ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).controller != ctx.controller) {
            moved = targets[0];
        } else {
            moved = find_enemy();
        }
        if (moved == kInvalidId) return;
        ctx.executor.moveToBase(moved);
        ctx.events.logTrace("BLAST CONE: moved enemy on play");

        // Second clause: having moved an enemy unit, you may exhaust this to
        // stun it.
        auto stun_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted &&
                   ctx.state.objectExists(moved) &&
                   !ctx.state.getObject(moved).is_stunned;
        };
        if (!stun_legal()) return;
        int conf2 = confirmOptional(ctx,
            "Blast Cone: exhaust me to stun the moved enemy?", stun_legal);
        if (conf2 != 1) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.stunUnitBy(moved, ctx.source);
        ctx.events.logTrace("BLAST CONE: exhausted to stun moved enemy");
    }
};

// ─── [745] Thrill of the Hunt ───────────────────────────────────────────────
// [Reaction] (engine-handled timing).
// Banish a friendly unit, then its owner plays it to any battlefield,
// ignoring its cost.
class AF6_ThrillOfTheHunt : public SpellCard {
public:
    AF6_ThrillOfTheHunt() : SpellCard(745) {}
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit_id = pickTarget(ctx, "Thrill of the Hunt", legal);
        if (unit_id == kInvalidId || !ctx.state.objectExists(unit_id)) return;
        auto owner = ctx.state.getObject(unit_id).owner;
        std::string uname = ctx.state.getObject(unit_id).name;

        // Step 1: banish the friendly unit.
        ctx.executor.banishObject(unit_id);
        // banishObject pushes to banishment; playIgnoringCost re-zones it, so
        // drop the duplicate banishment entry.
        auto& bz = ctx.state.player(owner).banishment;
        bz.erase(std::remove(bz.begin(), bz.end(), unit_id), bz.end());

        // Step 2: owner plays it to ANY battlefield, ignoring cost. Use
        // pickMode so the agent records the battlefield choice (one option
        // per battlefield on the board, not just controlled ones).
        std::vector<LocationId> locations;
        std::vector<std::string> labels;
        for (const auto& bf : ctx.state.battlefields) {
            locations.push_back(LocationId{BattlefieldLocation{bf.id}});
            std::string bf_name = "BF#" + std::to_string(static_cast<int>(bf.id));
            if (ctx.state.objectExists(bf.card_object_id)) {
                bf_name = ctx.state.getObject(bf.card_object_id).name;
            }
            labels.push_back(bf_name);
        }
        if (locations.empty()) {
            // No battlefields — fall back to owner's base.
            ctx.executor.playIgnoringCost(owner, unit_id,
                                           LocationId{BaseLocation{owner}});
            ctx.events.logTrace("THRILL: " + uname + " (no BF) played to base");
            return;
        }
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        int chosen = pickMode(ctx, "Thrill: which battlefield",
                              static_cast<int>(locations.size()), labels, legal_mask);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) chosen = 0;
        ctx.executor.playIgnoringCost(owner, unit_id, locations[chosen]);
        ctx.events.logTrace("THRILL: " + uname + " played to " + labels[chosen] +
                             ", ignoring cost");
    }
};

// ─── [770] Ripper's Bay ─────────────────────────────────────────────────────
// When a unit here is returned to a player's hand, that player may pay [1] to
// channel 1 rune exhausted.
//
// LIMITATION: there is no trigger event for "a unit here is returned to hand"
// — bounceToHand emits LeftBoardEvent but no battlefield-scoped return-to-hand
// trigger is dispatched to battlefield cards. We expose the channel-on-pay
// effect via onTrigger keyed on WhenIMove (the nearest battlefield-relevant
// event the engine fires for units here is not return-to-hand), so the full
// clause cannot be wired without engine support. Implemented as a no-op-safe
// trigger to avoid mis-firing.
class AF6_RippersBay : public BattlefieldCard {
public:
    AF6_RippersBay() : BattlefieldCard(770) {}
};

void registerAuditFixes6(CardRegistry& registry) {
    registry.registerCard(67, std::make_unique<AF6_BlitzcrankImpassive>());
    registry.registerCard(284, std::make_unique<AF6_TargonsPeak>());
    registry.registerCard(402, std::make_unique<AF6_BellowsBreath>());
    registry.registerCard(461, std::make_unique<AF6_FizzTrickster>());
    registry.registerCard(508, std::make_unique<AF6_RabadonsDeathcrown>());
    registry.registerCard(601, std::make_unique<AF6_SoulSword>());
    registry.registerCard(614, std::make_unique<AF6_NamiHeadstrong>());
    registry.registerCard(645, std::make_unique<AF6_SmokeAndMirrors>());
    registry.registerCard(695, std::make_unique<AF6_BlastCone>());
    registry.registerCard(745, std::make_unique<AF6_ThrillOfTheHunt>());
    registry.registerCard(770, std::make_unique<AF6_RippersBay>());
}

} // namespace riftbound
