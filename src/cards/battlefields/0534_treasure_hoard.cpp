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

class TreasureHoard : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        auto still_legal = [&ctx]() {
            return payableEnergy(ctx.state, ctx.controller) >= 1;
        };
        // No return ahead of the "may": with [1] unpayable confirmOptional
        // stores the "no" on the chain item, so the resolution asks nothing
        // if the [1] has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx,
            "Treasure Hoard: pay [1] for a Gold token?", still_legal);
        if (conf == -1) return;
        if (conf < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)

        // CR 149.2: gear can only be played to a player's Base unless an effect
        // says otherwise, and "play a Gold gear token exhausted" names no
        // location ("here" only says where the conquer happened). The helper
        // plays it exhausted, in the controller's base, as the Gold card
        // (CR 187.5).
        createGoldExhausted(ctx);
        RIFT_TRACE(ctx.events, "TREASURE HOARD: paid [1] -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 534;
        d.def_id = R"RB(sfd-220-221)RB";
        d.name = R"RB(Treasure Hoard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-220/221)RB";
        d.collector_number = 220;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may pay [1] to play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb39bf9f8c30d1ed756ba1b83c975a89d3635159-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_534(CardRegistry& r) {
    r.registerCard(534, std::make_unique<TreasureHoard>());
}

} // namespace riftbound
