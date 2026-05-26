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

class TeemoScout : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.giveTemporaryMight(ctx.source, 3);
        ctx.events.logTrace("TEEMO, SCOUT: +3M this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 197;
        d.def_id = R"RB(ogn-197-298)RB";
        d.name = R"RB(Teemo, Scout)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-197/298)RB";
        d.collector_number = 197;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Teemo)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
When you play me, give me +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca8a2e11dd78dd09ad1c9ad3a23e5699254b947e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_197(CardRegistry& r) {
    r.registerCard(197, std::make_unique<TeemoScout>());
}

} // namespace riftbound
