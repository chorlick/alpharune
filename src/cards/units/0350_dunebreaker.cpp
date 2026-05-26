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

class Dunebreaker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "If you have two or fewer cards in your hand, I enter ready."
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        return state.player(controller).hand.size() <= 2;
    }

    // "When I hold, draw 2."
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.events.logTrace("DUNEBREAKER: hold -> draw 2");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 350;
        d.def_id = R"RB(sfd-027-221)RB";
        d.name = R"RB(Dunebreaker)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-027/221)RB";
        d.collector_number = 27;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(If you have two or fewer cards in your hand, I enter ready.
When I hold, draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fa17369233dfc68e011d16b7b98971a6f8743c3e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_350(CardRegistry& r) {
    r.registerCard(350, std::make_unique<Dunebreaker>());
}

} // namespace riftbound
