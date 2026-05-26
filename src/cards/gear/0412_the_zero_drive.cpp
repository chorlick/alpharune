#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class TheZeroDrive : public GearCard {
public:
    const CardDef& def() const override { return def_; }

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
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 412;
        d.def_id = R"RB(sfd-090-221)RB";
        d.name = R"RB(The Zero Drive)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-090/221)RB";
        d.collector_number = 90;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][B] ([1][B]: Attach this to a unit you control.)
[3][B], Banish this: Play all units banished with this, ignoring their costs. (Use only if unattached.))RB";
        d.effect_text = R"RB([Deathknell] — Banish me. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cc80cbfa1a64ad22dc5f7df870e9206f6938f7f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_412(CardRegistry& r) {
    r.registerCard(412, std::make_unique<TheZeroDrive>());
}

} // namespace riftbound
