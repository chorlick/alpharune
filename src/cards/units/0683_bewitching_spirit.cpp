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

class BewitchingSpirit : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        static const std::vector<std::string> kModes = {"You discard",
                                                         "Opponent discards"};
        int mode = pickMode(ctx, "Bewitching Spirit: choose a player to discard",
                            /*num_modes=*/2, kModes);
        if (mode == -1) return;        // waiting on agent
        if (mode < 0) mode = 0;        // no-legal fallback -> self
        PlayerId target = (mode == 0) ? ctx.controller : opponent(ctx.controller);
        ctx.executor.discardCards(target, 1);
        ctx.events.logTrace("BEWITCHING SPIRIT: " + std::string(toString(target))
                            + " discards 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 683;
        d.def_id = R"RB(unl-121-219)RB";
        d.name = R"RB(Bewitching Spirit)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-121/219)RB";
        d.collector_number = 121;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.ability_text = R"RB(When you play me, choose a player. They discard 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a771917dea98b137c3f4a13e4967dccf9ee445e5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_683(CardRegistry& r) {
    r.registerCard(683, std::make_unique<BewitchingSpirit>());
}

} // namespace riftbound
