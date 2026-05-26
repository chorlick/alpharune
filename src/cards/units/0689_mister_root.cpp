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

class MisterRoot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MISTER ROOT: +2 XP on move to BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 689;
        d.def_id = R"RB(unl-127-219)RB";
        d.name = R"RB(Mister Root)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-127/219)RB";
        d.collector_number = 127;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][P] as an additional cost to have me enter ready.)
When I move to a battlefield, gain 2 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c9bb7a8f5a1426a4af9843f2473ee6cc37dd24bd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_689(CardRegistry& r) {
    r.registerCard(689, std::make_unique<MisterRoot>());
}

} // namespace riftbound
