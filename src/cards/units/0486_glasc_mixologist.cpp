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

class GlascMixologist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }

    // Eligible units in trash: card_type Unit with printed energy cost <= 3.
    std::vector<GameObjectId> eligibleTrashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            int cost = ctx.executor.cardDB().get(obj.card_def_id).energy_cost;
            if (cost > 3) continue;
            out.push_back(cid);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() {
            return !eligibleTrashUnits(ctx).empty();
        };
        int conf = confirmOptional(ctx,
            "Glasc Mixologist: play a <=3-cost unit from trash?", still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing eligible

        auto eligible = eligibleTrashUnits(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Glasc Mixologist (unit from trash)",
                                          eligible);
        // Suspended for agent decision.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Remove from trash, then play ignoring cost.
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("GLASC MIXOLOGIST: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 486;
        d.def_id = R"RB(sfd-165-221)RB";
        d.name = R"RB(Glasc Mixologist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-165/221)RB";
        d.collector_number = 165;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — You may play a unit with cost no more than [3] and no more than [A] from your trash, ignoring its cost. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7422fee71796aa61f8d9691d07654b901fc20f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_486(CardRegistry& r) {
    r.registerCard(486, std::make_unique<GlascMixologist>());
}

} // namespace riftbound
