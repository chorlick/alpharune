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

class AhriInquisitive : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I attack or defend, give an enemy unit here -2 [M] this turn, to
    // a minimum of 1 [M]." Combat triggers receive EMPTY targets, so pick at
    // resolution from enemy units at my battlefield.
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != my_bf) continue;
            legal.push_back(id);
        }
        GameObjectId picked = pickTarget(ctx, "Ahri, Inquisitive (-2 M)", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, -2, /*minimum=*/1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 119;
        d.def_id = R"RB(ogn-119-298)RB";
        d.name = R"RB(Ahri, Inquisitive)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-119/298)RB";
        d.collector_number = 119;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ahri)RB", R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When I attack or defend, give an enemy unit here -2 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cfa28e1abcac1db780d11e82985e13ee5978290d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_119(CardRegistry& r) {
    r.registerCard(119, std::make_unique<AhriInquisitive>());
}

} // namespace riftbound
