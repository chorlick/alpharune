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

class WagesOfPain : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.dealDamage(targets[0], 3, ctx.source);
            ctx.events.logTrace("WAGES OF PAIN: dealt 3 to a unit at a battlefield");
        }
        createGoldExhausted(ctx);
        ctx.events.logTrace("WAGES OF PAIN: played a Gold gear token exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 393;
        d.def_id = R"RB(sfd-070-221)RB";
        d.name = R"RB(Wages of Pain)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-070/221)RB";
        d.collector_number = 70;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Deal 3 to a unit at a battlefield. Play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/aef5c891bd6f7cbc3a97b1e01688868316599929-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_393(CardRegistry& r) {
    r.registerCard(393, std::make_unique<WagesOfPain>());
}

} // namespace riftbound
