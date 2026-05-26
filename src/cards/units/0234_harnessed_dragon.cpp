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

class HarnessedDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.killObject(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 234;
        d.def_id = R"RB(ogn-234-298)RB";
        d.name = R"RB(Harnessed Dragon)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-234/298)RB";
        d.collector_number = 234;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Dragon)RB", R"RB(Demacia)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, kill an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/00bc15bc1c2f5b8e6d1713819998df1c04864dcb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_234(CardRegistry& r) {
    r.registerCard(234, std::make_unique<HarnessedDragon>());
}

} // namespace riftbound
