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

class LooseCannon : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 249;
        d.def_id = R"RB(ogn-251-298)RB";
        d.name = R"RB(Loose Cannon)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-251/298)RB";
        d.collector_number = 251;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Jinx)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(At start of your Beginning Phase, draw 1 if you have one or fewer cards in your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f57c14381b126e9f5a7b5bc4913151cb24c14fc3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_249(CardRegistry& r) {
    r.registerCard(249, std::make_unique<LooseCannon>());
}

} // namespace riftbound
