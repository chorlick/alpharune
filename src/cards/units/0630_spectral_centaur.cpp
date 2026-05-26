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

class SpectralCentaur : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When another friendly unit dies, give me +2 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.events.logTrace("SPECTRAL CENTAUR: another friendly unit died -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 630;
        d.def_id = R"RB(unl-068-219)RB";
        d.name = R"RB(Spectral Centaur)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-068/219)RB";
        d.collector_number = 68;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.ability_text = R"RB(When another friendly unit dies, give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/60fb267f2d0cd16714d35a0eefd0d5864304102b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_630(CardRegistry& r) {
    r.registerCard(630, std::make_unique<SpectralCentaur>());
}

} // namespace riftbound
