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

class TricksyTentacles : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.moveToBase(targets[0]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true, .max_might = 8};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 616;
        d.def_id = R"RB(unl-054-219)RB";
        d.name = R"RB(Tricksy Tentacles)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-054/219)RB";
        d.collector_number = 54;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Move any number of enemy units with the same controller and a total Might of 8 or less to a single location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7e1e77df44c076599fa780f7fd01a4550f67af45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_616(CardRegistry& r) {
    r.registerCard(616, std::make_unique<TricksyTentacles>());
}

} // namespace riftbound
