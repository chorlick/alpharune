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

class TreasureHunter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("TREASURE HUNTER: Gold gear token created");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 451;
        d.def_id = R"RB(sfd-130-221)RB";
        d.name = R"RB(Treasure Hunter)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-130/221)RB";
        d.collector_number = 130;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.ability_text = R"RB(When I move, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c4e1bf257379b4612a1c58f0480480d9b698196c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_451(CardRegistry& r) {
    r.registerCard(451, std::make_unique<TreasureHunter>());
}

} // namespace riftbound
