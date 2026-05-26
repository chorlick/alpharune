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

class GreenFather : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        auto find_target = [&]() -> BattlefieldId {
            for (auto& bf : ctx.state.battlefields) {
                if (!bf.controller.has_value() ||
                    *bf.controller != ctx.controller) continue;
                if (ctx.state.objectExists(bf.card_object_id) &&
                    ctx.state.getObject(bf.card_object_id).name == "Brush")
                    continue;
                return bf.id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            if (ctx.state.getObject(ctx.source).is_exhausted) return false;
            return find_target() != kInvalidId;
        };

        int conf = confirmOptional(ctx,
            "Green Father: exhaust to replace controlled BF with Brush?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        BattlefieldId target = find_target();
        if (target == kInvalidId) return;
        auto& legend = ctx.state.getObject(ctx.source);
        legend.is_exhausted = true;
        ctx.executor.replaceBattlefieldWithToken(target, "Brush", ctx.controller);
        ctx.events.logTrace("GREEN FATHER: exhausted to replace BF#" +
                             std::to_string(target) + " with Brush token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 753;
        d.def_id = R"RB(unl-195-219)RB";
        d.name = R"RB(Green Father)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-195/219)RB";
        d.collector_number = 195;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Ivern)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you conquer or hold, you may exhaust me to replace that battlefield with a Brush battlefield token. (Bird, Cat, Dog, Poro, and Ivern units have +1 [M] in Brush. It can be swapped back when scored.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a2ca02d26c1e95db03690729bd31113bda7e4140-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_753(CardRegistry& r) {
    r.registerCard(753, std::make_unique<GreenFather>());
}

} // namespace riftbound
