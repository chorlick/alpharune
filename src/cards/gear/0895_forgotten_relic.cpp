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

class ForgottenRelic : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "When you play this or at the start of your Beginning Phase, [Burn 1].
    //  When you burn a unit this way, do this: Give a friendly unit +[M] equal
    //  to the burned card's Might this turn."
    // "When you burn a unit this way, do this: ..." is a reflexive trigger
    // (CR 388): when the burned card is a unit it goes on the chain as an
    // item of its own, carrying that card's Might; which friendly unit gets
    // it is chosen as that item is finalized.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::AtStartOfBeginning};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto burned = ctx.executor.burn(ctx.controller, 1);
        if (burned.empty() || !ctx.state.objectExists(burned[0])) return;
        const auto& card = ctx.state.getObject(burned[0]);
        if (!card.isUnit()) return;
        const int might = card.card_def_id != kInvalidId
            ? ctx.executor.cardDB().get(card.card_def_id).might : card.base_might;
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0, {might});
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data[0]`: the burned unit's printed Might.
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        const int might = data.empty() ? 0 : data[0];
        std::vector<GameObjectId> friendly;
        for (const auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        std::sort(friendly.begin(), friendly.end());
        GameObjectId picked = pickTarget(
            ctx, "Forgotten Relic: give a friendly unit +" + std::to_string(might) + " [M]",
            friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 388.4)
        if (picked == kInvalidId || !ctx.state.objectExists(picked) || might <= 0) return;
        ctx.executor.giveTemporaryMight(picked, might);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 895;
        d.def_id = R"RB(ven-108-166)RB";
        d.name = R"RB(Forgotten Relic)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-108/166)RB";
        d.collector_number = 108;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play this or at the start of your Beginning Phase, [Burn 1]. When you burn a unit this way, do this: Give a friendly unit +[M] equal to the burned card's Might this turn. (To Burn 1, put the top card of your Main Deck into your trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e249b3c2a7f652dfb7549604f600792df11835b4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_895(CardRegistry& r) {
    r.registerCard(895, std::make_unique<ForgottenRelic>());
}

}  // namespace riftbound
