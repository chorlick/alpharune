#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class TroveGolem : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "play four Gold gear tokens exhausted."
        for (int i = 0; i < 4; ++i) createGoldExhausted(ctx);
        ctx.events.logTrace("TROVE GOLEM: play four Gold gear tokens exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 494;
        d.def_id = R"RB(sfd-174-221)RB";
        d.name = R"RB(Trove Golem)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-174/221)RB";
        d.collector_number = 174;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 9;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, play four Gold gear tokens exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e5b0966cc00203db1036b6fa95360541f72ae131-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_494(CardRegistry& r) {
    r.registerCard(494, std::make_unique<TroveGolem>());
}

} // namespace riftbound
