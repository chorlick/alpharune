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

class ViciousSnapjaws : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When another friendly unit dies, gain 1 XP."
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("VICIOUS SNAPJAWS: another friendly unit died -> +1 XP");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 691;
        d.def_id = R"RB(unl-129-219)RB";
        d.name = R"RB(Vicious Snapjaws)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-129/219)RB";
        d.collector_number = 129;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.ability_text = R"RB(When another friendly unit dies, gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/99845de971eb165c7a6bb424050cd9a29b45b548-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_691(CardRegistry& r) {
    r.registerCard(691, std::make_unique<ViciousSnapjaws>());
}

} // namespace riftbound
