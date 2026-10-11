#include "cards/card.h"
#include "cards/card_helpers.h"
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

    // "[Equip] [3][A]. This ability's Energy cost is reduced by the Might of
    // the unit you choose." Paid through standardEquip: all-or-nothing, with
    // the engine's Power accounting ([A] = floating Power or any rune).
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        if (!equipEndsOnBoard(ctx.state, ctx.source, unit)) return false;
        const int reduction = ctx.state.getObject(unit).current_might;
        const int energy_cost = std::max(0, 3 - reduction);
        if (!standardEquip(ctx, ctx.source, unit, energy_cost, std::optional<Domain>{}))
            return false;
        ctx.events.logTrace("EQUIP: Hextech Gauntlets (energy paid=" +
                            std::to_string(energy_cost) + ", reduced by Might " +
                            std::to_string(reduction) + ")");
        return true;
    }

    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& /*targets*/) override {
        // "if you assigned 3 or more excess damage": in the combat at the
        // battlefield my unit just conquered.
        if (!ctx.state.objectExists(unit)) return;
        const auto bf = ctx.state.getObject(unit).battlefieldId();
        if (!bf || excessCombatDamageAt(ctx.state, ctx.controller, *bf) < 3) return;
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
