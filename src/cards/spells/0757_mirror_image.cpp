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

class MirrorImage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        // Create a Reflection token at the controller's base. Full
        // 8-arg signature: (controller, type, name, might, tags,
        // keywords, location, enter_ready). copyUnit() below
        // overwrites might/tags/keywords from the source.
        GameObjectId token = ctx.executor.createToken(
            ctx.controller, CardType::Unit, "Reflection",
            /*might=*/0, /*tags=*/{}, /*keywords=*/KeywordSet{},
            BaseLocation{}, /*enter_ready=*/true);
        if (token != kInvalidId) {
            ctx.executor.copyUnit(token, targets[0]);
            ctx.executor.giveTemporaryKeyword(token, Keyword::Temporary, 0);
            ctx.events.logTrace("MIRROR IMAGE: spawned Reflection copy of " +
                                 ctx.state.getObject(targets[0]).name +
                                 " (Temporary)");
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 757;
        d.def_id = R"RB(unl-200-219)RB";
        d.name = R"RB(Mirror Image)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-200/219)RB";
        d.collector_number = 200;
        d.artist = R"RB(华锐)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(LeBlanc)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Choose a unit. Play a ready Reflection unit token to your base. Then do this: It becomes a copy of that unit. Give it [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7ef8a09c0297cd796c4e2dee15922745ede7417-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_757(CardRegistry& r) {
    r.registerCard(757, std::make_unique<MirrorImage>());
}

} // namespace riftbound
