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

class GloriousExecutioner : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("GLORIOUS EXECUTIONER: win combat -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 552;
        d.def_id = R"RB(sfd-242-221)RB";
        d.name = R"RB(Glorious Executioner)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-242/221)RB";
        d.collector_number = 242;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Draven)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you win a combat, draw 1. (You win if only your units remain after combat.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5a06fcd2cadbdb574d34d210ca97441ec33c9277-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_552(CardRegistry& r) {
    r.registerCard(552, std::make_unique<GloriousExecutioner>());
}

} // namespace riftbound
