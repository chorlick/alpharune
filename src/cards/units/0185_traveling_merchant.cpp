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

class TravelingMerchant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Traveling Merchant: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 185;
        d.def_id = R"RB(ogn-185-298)RB";
        d.name = R"RB(Traveling Merchant)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-185/298)RB";
        d.collector_number = 185;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, discard 1, then draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b7dbebe2bf5691391c8e4146e478b8bd2ac40aef-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_185(CardRegistry& r) {
    r.registerCard(185, std::make_unique<TravelingMerchant>());
}

} // namespace riftbound
