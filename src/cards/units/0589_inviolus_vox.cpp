#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When I conquer, give a friendly unit +8 [M] this turn."
class InviolusVox : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "a friendly unit": any of mine on board
        const auto friendly = boardObjects(ctx.state, [&](const GameObject& o) {
            return o.isUnit() && o.controller == ctx.controller;
        });
        const GameObjectId t = pickTarget(ctx, "Inviolus Vox: give a friendly unit +8 [M]",
                                          friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(t, 8);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 589;
        d.def_id = R"RB(unl-027-219)RB";
        d.name = R"RB(Inviolus Vox)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-027/219)RB";
        d.collector_number = 27;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When I conquer, give a friendly unit +8 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9b742ef3dc17f5da03d41e3349839037183b5ea9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_589(CardRegistry& r) {
    r.registerCard(589, std::make_unique<InviolusVox>());
}

} // namespace riftbound
