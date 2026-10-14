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

class ScrapyardChampion : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Legion] — the ability is Active only if another card was played this
    // turn, and only an Active ability triggers (CR 727.1.c.1, 812.1.c):
    // judged as I am played. Off, nothing goes on the chain (finding G9).
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return legionActive(state, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 2, "Scrapyard Champion: discard 2 then draw 2",
            [](CardContext& c) { c.executor.drawCards(c.controller, 2); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 20;
        d.def_id = R"RB(ogn-020-298)RB";
        d.name = R"RB(Scrapyard Champion)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-020/298)RB";
        d.collector_number = 20;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, discard 2, then draw 2. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/10f096d2b469fd73329386e5efe88c9bec667d7c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_20(CardRegistry& r) {
    r.registerCard(20, std::make_unique<ScrapyardChampion>());
}

} // namespace riftbound
