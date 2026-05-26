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

class NilahJoyfulAscetic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.state.player(ctx.controller).xp += 1;
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 677;
        d.def_id = R"RB(unl-115-219)RB";
        d.name = R"RB(Nilah, Joyful Ascetic)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-115/219)RB";
        d.collector_number = 115;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Nilah)RB", R"RB(Kathkan)RB", R"RB(Demon)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
[Ganking] (I can move from battlefield to battlefield.)
When I move, gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6138c519310f917461e09d90fe3f2a9480914947-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_677(CardRegistry& r) {
    r.registerCard(677, std::make_unique<NilahJoyfulAscetic>());
}

} // namespace riftbound
