#include "cards/card.h"
#include "cards/card_helpers.h"
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

class CorruptEnforcer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIMoveToFB, TriggerType::WhenIWinCombat};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            ctx.executor.drawCards(ctx.controller, 1);
            RIFT_TRACE(ctx.events, "CORRUPT ENFORCER: win combat -> draw 1");
            return;
        }
        // WhenIMoveToFB: "discard 1" — the controller chooses the card.
        if (!ctx.state.chain.resuming.has_value()) {
            // No chain item to resume (called directly): no agent to ask.
            ctx.executor.discardCards(ctx.controller, 1);
            return;
        }
        discardThenAct(ctx, 1, "Corrupt Enforcer: discard 1", [](CardContext&) {});
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 444;
        d.def_id = R"RB(sfd-123-221)RB";
        d.name = R"RB(Corrupt Enforcer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-123/221)RB";
        d.collector_number = 123;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(When I move to a battlefield, discard 1.
When I win a combat, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c0a6bf40b629fa038299329c1d803fc9fc8e61e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_444(CardRegistry& r) {
    r.registerCard(444, std::make_unique<CorruptEnforcer>());
}

} // namespace riftbound
