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

class TastyFaefolk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 2, true);
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 75;
        d.def_id = R"RB(ogn-075-298)RB";
        d.name = R"RB(Tasty Faefolk)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-075/298)RB";
        d.collector_number = 75;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Fae)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Accelerate] (You may pay [1][G] as an additional cost to have me enter ready.)
[Deathknell] — Channel 2 runes exhausted and draw 1. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65f69ca9a1087deb12e91fb6fdee7b6efd0c088f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_75(CardRegistry& r) {
    r.registerCard(75, std::make_unique<TastyFaefolk>());
}

} // namespace riftbound
