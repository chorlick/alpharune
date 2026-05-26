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

class TurnToDust : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Temporary, 0);
            ctx.events.logTrace("TURN TO DUST: gave gear [Temporary]");
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 632;
        d.def_id = R"RB(unl-070-219)RB";
        d.name = R"RB(Turn to Dust)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-070/219)RB";
        d.collector_number = 70;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Give a gear [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4986c0c2fea96d9a88d736789d604374a64d03fb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_632(CardRegistry& r) {
    r.registerCard(632, std::make_unique<TurnToDust>());
}

} // namespace riftbound
