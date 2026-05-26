#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BladeOfTheRuinedKing : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        // PRE-CHECK both portions of the additional cost before committing
        // anything (mirror standardEquip's all-or-nothing discipline):
        //   1) an Order ([Y]) power rune to recycle, and
        //   2) a killable friendly unit OTHER than the equip target.
        GameObjectId order_rune = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() ||
                *obj.location != LocationId{base_loc}) continue;
            for (auto d : obj.domains) {
                if (d == Domain::Order) { order_rune = id; break; }
            }
            if (order_rune != kInvalidId) break;
        }
        if (order_rune == kInvalidId) return false;

        std::vector<GameObjectId> killable;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            if (id == unit) continue;  // can't kill the unit we're equipping
            killable.push_back(id);
        }
        if (killable.empty()) return false;

        // Commit cost 1: kill a friendly unit (agent choice).
        GameObjectId victim = pickTarget(ctx, "Blade of the Ruined King: "
                                              "kill a friendly unit", killable);
        if (victim == kInvalidId) {
            // Suspended for the agent choice — re-entry will resume.
            return false;
        }
        if (!state.objectExists(victim)) return false;
        ctx.executor.killObject(victim);

        // Commit cost 2: recycle the Order power rune for [Y].
        {
            auto& dr = state.getObject(order_rune);
            ctx.events.logTrace("  EQUIP_COST: recycled " + dr.name + " for [Y]");
            dr.location = std::nullopt;
            dr.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), order_rune);
        }

        // Attach.
        if (!state.objectExists(unit)) return false;
        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit_obj.name +
                             " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
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
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 498;
        d.def_id = R"RB(sfd-178-221)RB";
        d.name = R"RB(Blade of the Ruined King)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-178/221)RB";
        d.collector_number = 178;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might_bonus = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] — [Y], Kill a friendly unit (Pay the cost: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/26ab126258a15afd380c313e973f7469808ce55f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_498(CardRegistry& r) {
    r.registerCard(498, std::make_unique<BladeOfTheRuinedKing>());
}

} // namespace riftbound
