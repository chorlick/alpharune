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

class VayneHunter_35 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "If an opponent controls a battlefield, I enter ready."
    bool entersReadyOnPlay(const GameState& state,
                           PlayerId controller) const override {
        PlayerId opp = opponent(controller);
        for (const auto& bf : state.battlefields)
            if (bf.controller.has_value() && *bf.controller == opp) return true;
        return false;
    }

    // "When I conquer, you may pay [1] to return me to my owner's hand."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        auto still_legal = [&]() {
            return canPayEnergy(ctx, ctx.controller, 1) &&
                   ctx.state.getObject(ctx.source).location.has_value();
        };
        // No return ahead of the "may" (as the reprint, 537): with [1]
        // unpayable confirmOptional stores the "no" on the chain item, so the
        // resolution asks nothing if it has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Vayne: pay [1] to return me to hand?",
                                   still_legal);
        if (conf < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        // A Vayne that left the board in response stays where she went
        // (EffectExecutor::bounceToHand does nothing for her).
        ctx.executor.bounceToHand(ctx.source);
        RIFT_TRACE(ctx.events, "VAYNE: paid [1] -> returned to owner's hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 35;
        d.def_id = R"RB(ogn-035-298)RB";
        d.name = R"RB(Vayne, Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-035/298)RB";
        d.collector_number = 35;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Vayne)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.assault_value = 3;
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Assault 3] (+3 [M] while I'm an attacker.)
If an opponent controls a battlefield, I enter ready.
When I conquer, you may pay [1] to return me to my owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b38a22e6f4bfaa8589f32754a7466fe5214e81b5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_35(CardRegistry& r) {
    r.registerCard(35, std::make_unique<VayneHunter_35>());
}

} // namespace riftbound
