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

class Gloomist : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Optional: "you may exhaust me to draw 1". Routed through
        // confirmOptional so the agent picks yes/no (random ~50/50,
        // policy learns when to spend the exhaust). still_legal blocks
        // the prompt if the legend is already exhausted.
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            return !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx,
            "Gloomist: exhaust to draw 1?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;
        auto& legend = ctx.state.getObject(ctx.source);
        legend.is_exhausted = true;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("TRIGGER: Gloomist exhausts to draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 785;
        d.def_id = R"RB(unl-232-219)RB";
        d.name = R"RB(Gloomist)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-232/219)RB";
        d.collector_number = 232;
        d.artist = R"RB(Hozure)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Vex)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you or an ally hold, you may exhaust me to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d044ea46fa38cff80c39fdb0b890dd7226c22b89-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_785(CardRegistry& r) {
    r.registerCard(785, std::make_unique<Gloomist>());
}

} // namespace riftbound
