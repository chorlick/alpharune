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

class BlackMarketBroker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayFromFacedown; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When you play a card from face down, play a Gold gear token exhausted."
        createGoldExhausted(ctx);
        ctx.events.logTrace("BLACK MARKET BROKER: facedown play -> Gold gear token exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 442;
        d.def_id = R"RB(sfd-121-221)RB";
        d.name = R"RB(Black Market Broker)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-121/221)RB";
        d.collector_number = 121;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play a card from face down, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f167c95cea17981f20b767ff81c180aab8a383e2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_442(CardRegistry& r) {
    r.registerCard(442, std::make_unique<BlackMarketBroker>());
}

} // namespace riftbound
