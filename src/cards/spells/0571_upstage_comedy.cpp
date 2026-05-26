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

class UpstageComedy : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Upstage Comedy", legal);
        if (picked == kInvalidId) return;
        ctx.executor.readyObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 571;
        d.def_id = R"RB(unl-009-219)RB";
        d.name = R"RB(Upstage Comedy)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-009/219)RB";
        d.collector_number = 9;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4a636fcd682b32b41a4886c7383e781414139adb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_571(CardRegistry& r) {
    r.registerCard(571, std::make_unique<UpstageComedy>());
}

} // namespace riftbound
