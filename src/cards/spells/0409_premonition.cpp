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

class Premonition : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 3);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 409;
        d.def_id = R"RB(sfd-087-221)RB";
        d.name = R"RB(Premonition)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-087/221)RB";
        d.collector_number = 87;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Draw 3.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20ad2f42bcf9a0402e2a0d5a21d55eacee9ecf35-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_409(CardRegistry& r) {
    r.registerCard(409, std::make_unique<Premonition>());
}

} // namespace riftbound
