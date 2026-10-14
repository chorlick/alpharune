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

class ProtectiveSands : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you conquer here, if you control 4 or fewer runes, you may pay [1]
    // to draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    // "if you control 4 or fewer runes" is part of the trigger condition (CR
    // 383.2.a.1): counted when the conquer happens; with more nothing is put
    // on the chain.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return runesControlled(state, controller) <= 4;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&ctx]() {
            return canPayEnergy(ctx, ctx.controller, 1);
        };
        // No return ahead of the "may": when it can't be offered
        // confirmOptional stores the "no" on the chain item, so the resolution
        // asks nothing if the [1] has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Protective Sands: pay [1] to draw 1?", still_legal);
        if (conf < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 949;
        d.def_id = R"RB(ven-162-166)RB";
        d.name = R"RB(Protective Sands)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-162/166)RB";
        d.collector_number = 162;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, if you control 4 or fewer runes, you may pay [1] to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a994ae712cdefcbdaf263fbb6ab0723d0f678e99-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_949(CardRegistry& r) {
    r.registerCard(949, std::make_unique<ProtectiveSands>());
}

}  // namespace riftbound
