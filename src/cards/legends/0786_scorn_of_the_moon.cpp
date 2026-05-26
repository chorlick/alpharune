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

class ScornOfTheMoon : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.events.logTrace("ACTIVATE: Scorn of the Moon adds [1] energy "
                            "(showdown-spend earmark not engine-enforced)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 786;
        d.def_id = R"RB(unl-234-219)RB";
        d.name = R"RB(Scorn of the Moon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-234/219)RB";
        d.collector_number = 234;
        d.artist = R"RB(Lisha Du)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Diana)RB"};
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction][>] [E]: [Add] [1]. Spend this Energy only during showdowns.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8bd4006c34aa020211e501e3cb7ee14ab5b4c41f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_786(CardRegistry& r) {
    r.registerCard(786, std::make_unique<ScornOfTheMoon>());
}

} // namespace riftbound
