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

class WhiteflameProtector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.giveTemporaryMight(targets[0], 8);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 82;
        d.def_id = R"RB(ogn-082-298)RB";
        d.name = R"RB(Whiteflame Protector)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-082/298)RB";
        d.collector_number = 82;
        d.artist = R"RB(Aron Elekes)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me, give a unit +8 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a5cdd793736f915fa1b4ea388af66de7b2bc00cb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_82(CardRegistry& r) {
    r.registerCard(82, std::make_unique<WhiteflameProtector>());
}

} // namespace riftbound
