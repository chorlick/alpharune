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

class Scrapheap : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When this is played, discarded, or killed, draw 1."
    // Played -> WhenYouPlayThis; killed -> WhenIDie. The "discarded" branch
    // cannot fire: the TriggerManager does not subscribe to CardsDiscardedEvent
    // (no WhenYouDiscard dispatch), and a discard moves the card from hand
    // directly to trash without an on-board trigger. Wiring that needs an
    // engine edit (out of scope); the played/killed branches are implemented.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SCRAPHEAP: played/killed -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 182;
        d.def_id = R"RB(ogn-182-298)RB";
        d.name = R"RB(Scrapheap)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-182/298)RB";
        d.collector_number = 182;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When this is played, discarded, or killed, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c059786e89a2443221b7c0b8ca39779dcc9c755-744x1038.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_182(CardRegistry& r) {
    r.registerCard(182, std::make_unique<Scrapheap>());
}

} // namespace riftbound
