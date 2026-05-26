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

class SkySplitter : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int highest = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            if (obj.current_might > highest) highest = obj.current_might;
        }
        return highest;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.dealDamage(targets[0], 5, ctx.source);
        ctx.events.logTrace("SKY SPLITTER: deal 5 to a unit at a battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 14;
        d.def_id = R"RB(ogn-014-298)RB";
        d.name = R"RB(Sky Splitter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-014/298)RB";
        d.collector_number = 14;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
This spell's Energy cost is reduced by the highest Might among units you control.
Deal 5 to a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/01faae468720dd5bf5e3fe12ba56c01af70263be-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_14(CardRegistry& r) {
    r.registerCard(14, std::make_unique<SkySplitter>());
}

} // namespace riftbound
