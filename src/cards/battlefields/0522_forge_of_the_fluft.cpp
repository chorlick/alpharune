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

class ForgeOfTheFluft : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "While you control this battlefield, friendly legends have
    //  '[E]: Attach an Equipment you control to a unit you control.'"
    // Wired via the aura-granted-ability pipeline: applyPassiveAura grants this
    // card's ability (index 0) to the controller's legend; activation routes
    // back here to onActivate, which attaches a chosen Equipment to a chosen
    // unit (both controller-owned). Exhausts the bearer (the legend).
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        a.needs_activation_time_target = true;  // pick equipment + unit at resolve
        return {a};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>&) override {
        // Equipment the controller owns and that isn't already attached.
        std::vector<GameObjectId> equips;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.controller != ctx.controller || !obj.location.has_value()) continue;
            if (obj.isGear() && !obj.attached_to.has_value() && isEquipment(obj))
                equips.push_back(id);
        }
        if (equips.empty()) return;
        auto units_fn = [&](GameObjectId) {
            std::vector<GameObjectId> units;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value())
                    units.push_back(id);
            return units;
        };
        auto [eq, unit] = pickTargetPair(ctx, "Forge of the Fluft: attach Equipment to a unit",
                                         equips, units_fn);
        if (eq == kInvalidId || unit == kInvalidId) return;  // suspended or fizzled
        attachFree(ctx, eq, unit);
        ctx.events.logTrace("FORGE OF THE FLUFT: attached an Equipment to a unit");
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (controller == PlayerId::None) return;  // uncontrolled BF
        auto legend = state.player(controller).legend_zone;
        if (legend != kInvalidId && state.objectExists(legend))
            state.getObject(legend).granted_abilities.push_back({cardDefId(), 0});
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 522;
        d.def_id = R"RB(sfd-208-221)RB";
        d.name = R"RB(Forge of the Fluft)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-208/221)RB";
        d.collector_number = 208;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While you control this battlefield, friendly legends have "[E]: Attach an Equipment you control to a unit you control.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6ad10353e7fc1337d8cc79086f8eaac1c75f5598-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_522(CardRegistry& r) {
    r.registerCard(522, std::make_unique<ForgeOfTheFluft>());
}

} // namespace riftbound
