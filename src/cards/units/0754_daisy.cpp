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

class Daisy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return scanFriendlyTags(state, player).count();
    }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto pres = scanFriendlyTags(ctx.state, ctx.controller);
        if (!pres.allFour()) {
            ctx.events.logTrace("DAISY: union not satisfied (count=" +
                                 std::to_string(pres.count()) + "/4) — no stun");
            return;
        }
        // Stun an enemy unit at my location.
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [oid, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location != *self.location) continue;
            if (obj.is_stunned) continue;
            ctx.events.logTrace("DAISY: stunning " + obj.name + " (id=" +
                                 std::to_string(oid) + ")");
            ctx.executor.stunUnit(oid);
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 754;
        d.def_id = R"RB(unl-196-219)RB";
        d.name = R"RB(Daisy!)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-196/219)RB";
        d.collector_number = 196;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(I enter ready.
Reduce my cost by [1] for each of the following tags among your units — Bird, Cat, Dog, and Poro.
When I attack while your units have all 4 tags, [Stun] an enemy unit here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ada85b2a6076b60933831454d02a21da3654362c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_754(CardRegistry& r) {
    r.registerCard(754, std::make_unique<Daisy>());
}

} // namespace riftbound
