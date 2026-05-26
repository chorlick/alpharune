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

class GemJammer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Ganking, 0);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 330;
        d.def_id = R"RB(sfd-007-221)RB";
        d.name = R"RB(Gem Jammer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-007/221)RB";
        d.collector_number = 7;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(When you play me, give a unit [Ganking] this turn. (It can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9f487e8a76e6e6b9f2e32de0c9c147be42f49e59-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_330(CardRegistry& r) {
    r.registerCard(330, std::make_unique<GemJammer>());
}

} // namespace riftbound
