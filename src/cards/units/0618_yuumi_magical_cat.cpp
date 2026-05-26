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

class YuumiMagicalCat : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.giveTemporaryMight(targets[0], 3);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 618;
        d.def_id = R"RB(unl-056-219)RB";
        d.name = R"RB(Yuumi, Magical Cat)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-056/219)RB";
        d.collector_number = 56;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Fae)RB", R"RB(Cat)RB", R"RB(Bandle City)RB", R"RB(Yuumi)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB(When I attack or defend, give one of your other units here +3 [M] and [Tank] this turn. (It must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/588ddfeb01ab7b37110d8a7e656cf9f35530ce4e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_618(CardRegistry& r) {
    r.registerCard(618, std::make_unique<YuumiMagicalCat>());
}

} // namespace riftbound
