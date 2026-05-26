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

class LeeSinAscetic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.buffUnit(ctx.source);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 78;
        d.def_id = R"RB(ogn-078-298)RB";
        d.name = R"RB(Lee Sin, Ascetic)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-078/298)RB";
        d.collector_number = 78;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Lee Sin)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
[E]: Buff me. (I get a +1 [M] buff.)
I can have any number of buffs.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/70734e8833bfbbdb2736407c449f418553e3cf7c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_78(CardRegistry& r) {
    r.registerCard(78, std::make_unique<LeeSinAscetic>());
}

} // namespace riftbound
