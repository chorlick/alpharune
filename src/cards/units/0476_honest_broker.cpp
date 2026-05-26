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

class HonestBroker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("HONEST BROKER (Deathknell): Gold gear token created");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 476;
        d.def_id = R"RB(sfd-155-221)RB";
        d.name = R"RB(Honest Broker)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-155/221)RB";
        d.collector_number = 155;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Play a Gold gear token exhausted. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b3cc9bb93888f6f276f674baa16a0e53c724ae99-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_476(CardRegistry& r) {
    r.registerCard(476, std::make_unique<HonestBroker>());
}

} // namespace riftbound
