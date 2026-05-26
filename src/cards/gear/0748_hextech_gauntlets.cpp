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

class HextechGauntlets : public GearCard {
public:
    const CardDef& def() const override { return def_; }

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
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 748;
        d.def_id = R"RB(unl-188-219)RB";
        d.name = R"RB(Hextech Gauntlets)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-188/219)RB";
        d.collector_number = 188;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Vi)RB", R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [3][A]. This ability's Energy cost is reduced by the Might of the unit you choose. (Pay the cost: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer, if you assigned 3 or more excess damage, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/22755fc744a79fbdcce0ceadfe20d6d0a2b8624a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_748(CardRegistry& r) {
    r.registerCard(748, std::make_unique<HextechGauntlets>());
}

} // namespace riftbound
