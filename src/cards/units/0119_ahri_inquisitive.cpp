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

class AhriInquisitive_119 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When I attack or defend, give an enemy unit here -2 [M] this turn, to
    // a minimum of 1 [M]." Combat triggers receive EMPTY targets, so pick at
    // resolution from enemy units at my battlefield.
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Enemy units at my battlefield, minus those that can't be chosen by
        // enemy spells and abilities.
        std::vector<GameObjectId> legal;
        if (const auto my_bf = ctx.state.getObject(ctx.source).battlefieldId())
            legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller && o.battlefieldId() == my_bf;
            });
        std::sort(legal.begin(), legal.end());
        GameObjectId picked = pickTarget(ctx, "Ahri, Inquisitive (-2 M)", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;     // none / mistarget / waiting
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
    r.registerCard(119, std::make_unique<AhriInquisitive_119>());
}

} // namespace riftbound
