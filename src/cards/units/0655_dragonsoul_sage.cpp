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

class DragonsoulSage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.events.logTrace("ACTIVATE: Dragonsoul Sage adds [1] energy");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 655;
        d.def_id = R"RB(unl-093-219)RB";
        d.name = R"RB(Dragonsoul Sage)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-093/219)RB";
        d.collector_number = 93;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction][>] [E]: [Add] [1]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2065a3b0fef9779fed8a3d42202606a31acf59ff-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_655(CardRegistry& r) {
    r.registerCard(655, std::make_unique<DragonsoulSage>());
}

} // namespace riftbound
