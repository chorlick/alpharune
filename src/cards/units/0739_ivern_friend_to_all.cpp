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

class IvernFriendToAll : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::AsYouPlayMe, TriggerType::WhenIConquerOrHold};
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::AsYouPlayMe) {
            doChooseTag(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIConquerOrHold) {
            doScore(ctx);
        }
    }

private:
    void doChooseTag(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return;
        static const std::vector<std::string> kTags = {"Bird", "Cat", "Dog",
                                                        "Poro"};
        int mode = pickMode(ctx, "Ivern, Friend to All: choose a tag",
                            /*num_modes=*/4, kTags);
        if (mode == -1) return;  // waiting for agent choice
        if (mode < 0 || mode >= 4) mode = 0;
        const std::string& chosen = kTags[mode];
        auto& self = ctx.state.getObject(ctx.source);
        auto& tags = self.tags;
        if (std::find(tags.begin(), tags.end(), chosen) == tags.end()) {
            tags.push_back(chosen);
        }
        ctx.events.logTrace("IVERN FRIEND: chose tag [" + chosen + "]");
    }

    void doScore(CardContext& ctx) {
        auto pres = scanFriendlyTags(ctx.state, ctx.controller);
        if (!pres.allFour()) {
            ctx.events.logTrace("IVERN FRIEND: union not satisfied (count=" +
                                 std::to_string(pres.count()) + "/4) — no score");
            return;
        }
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("IVERN FRIEND: all-4-tag union satisfied — score 1 -> " +
                             std::to_string(ps.score));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 739;
        d.def_id = R"RB(unl-177-219)RB";
        d.name = R"RB(Ivern, Friend to All)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-177/219)RB";
        d.collector_number = 177;
        d.artist = R"RB(Zhongqi Li)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(As you play me, choose Bird, Cat, Dog, or Poro. I gain that tag.
When I conquer or hold, score 1 point if your units have all of the following tags among them — Bird, Cat, Dog, and Poro.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e450956e0561ddca36558d095b71d3b60dff8b03-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_739(CardRegistry& r) {
    r.registerCard(739, std::make_unique<IvernFriendToAll>());
}

} // namespace riftbound
