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

class ShadowOrderDisciple : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When I move, you may [Burn 1] to give me +1 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto on_board = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        auto legal = [&]() {
            return on_board() && (costAlreadyPaid(ctx) ||
                                  !ctx.state.player(ctx.controller).main_deck.empty());
        };
        int conf = confirmOptional(ctx, "Shadow Order Disciple: Burn 1 for +1 [M]?", legal);
        if (conf == -1) return;                       // waiting for the agent
        // [Burn 1] is the cost: paid as the ability goes on the chain (CR 383.3).
        if (conf == 1 && costDueNow(ctx) && ctx.executor.burn(ctx.controller, 1).empty())
            return;                                   // cost not paid
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (conf != 1 || !on_board()) return;         // declined / I left the board
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "SHADOW ORDER DISCIPLE: Burn 1 -> +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 882;
        d.def_id = R"RB(ven-095-166)RB";
        d.name = R"RB(Shadow Order Disciple)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-095/166)RB";
        d.collector_number = 95;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When I move, you may [Burn 1] to give me +1 [M] this turn. (To Burn 1, put the top card of your Main Deck into your trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f85fb8b46a29360f9850826df11202ff8221fec8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_882(CardRegistry& r) {
    r.registerCard(882, std::make_unique<ShadowOrderDisciple>());
}

}  // namespace riftbound
