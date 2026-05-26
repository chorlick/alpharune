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

class ProgressDay : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 4);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 114;
        d.def_id = R"RB(ogn-114-298)RB";
        d.name = R"RB(Progress Day)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-114/298)RB";
        d.collector_number = 114;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Draw 4.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b15f479a8f29e31b6e4c06cf2d7c8cb8630073ef-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_114(CardRegistry& r) {
    r.registerCard(114, std::make_unique<ProgressDay>());
}

} // namespace riftbound
