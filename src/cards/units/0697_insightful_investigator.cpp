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

class InsightfulInvestigator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, choose an opponent. They reveal their hand. You may pay
    //  2 XP to choose a card from their hand. If you do, they discard that card
    //  and draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);  // 1v1: the single opponent
        auto& ops = ctx.state.player(opp);

        // "They reveal their hand." — reveal each card to the controller. Only
        // emit the reveal once (on first entry into this trigger).
        if (!ctx.state.chain.resuming.has_value() ||
            ctx.state.chain.resuming->resume_point == 0) {
            for (auto cid : ops.hand) {
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    cid, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller,
                    ZoneType::Hand});
            }
            ctx.events.logTrace("INSIGHTFUL INVESTIGATOR: opponent reveals hand");
        }

        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&]() { return ps.xp >= 2 && !ops.hand.empty(); };
        int conf = confirmOptional(ctx, "Insightful Investigator: pay 2 XP to choose a card to discard?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / can't pay

        std::vector<GameObjectId> hand(ops.hand.begin(), ops.hand.end());
        GameObjectId chosen = pickTarget(ctx, "Insightful Investigator: choose a card", hand);
        if (chosen == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (chosen == kInvalidId || !ctx.state.objectExists(chosen)) return;
        ps.xp -= 2;
        ctx.executor.applyDiscard(opp, chosen);   // "they discard that card"
        ctx.executor.drawCards(opp, 1);            // "and draw 1"
        ctx.events.logTrace("INSIGHTFUL INVESTIGATOR: paid 2 XP -> opponent discards a card and draws 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 697;
        d.def_id = R"RB(unl-135-219)RB";
        d.name = R"RB(Insightful Investigator)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-135/219)RB";
        d.collector_number = 135;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, choose an opponent. They reveal their hand. You may pay 2 XP to choose a card from their hand. If you do, they discard that card and draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3540a748211afd35b9ef4873dae0850c238fa964-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_697(CardRegistry& r) {
    r.registerCard(697, std::make_unique<InsightfulInvestigator>());
}

} // namespace riftbound
