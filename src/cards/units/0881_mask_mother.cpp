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

class MaskMother : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When you discard me, you may pay [1] to give a friendly unit +2 [M] this
    // turn." Fires from the trash. The [1] is paid with card_helpers.h
    // payEnergy: floating pool first, then ready runes.
    TriggerType triggerType() const override { return TriggerType::WhenIAmDiscarded; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly = [&]() {
            auto out = chooseableUnits(ctx.state, ctx.controller,
                [&](const GameObject& o) { return o.controller == ctx.controller; });
            std::sort(out.begin(), out.end());
            return out;
        };
        auto can_pay = [&]() {
            return canPayEnergy(ctx, ctx.controller, 1) && !friendly().empty();
        };
        int conf = confirmOptional(ctx, "Mask Mother: pay [1] to give a friendly unit +2 [M]?",
                                   can_pay);
        if (conf != 1) return;                        // waiting, declined or can't pay
        auto picked = pickTarget(ctx, "Mask Mother: +2 [M] this turn", friendly());
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.giveTemporaryMight(picked, 2);
        RIFT_TRACE(ctx.events, "MASK MOTHER: discarded, paid [1] -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 881;
        d.def_id = R"RB(ven-094-166)RB";
        d.name = R"RB(Mask Mother)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-094/166)RB";
        d.collector_number = 94;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you discard me, you may pay [1] to give a friendly unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/21cf578371a75f6e2661d44f4d7389d8732c133e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_881(CardRegistry& r) {
    r.registerCard(881, std::make_unique<MaskMother>());
}

}  // namespace riftbound
