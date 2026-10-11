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
    // Resumable: the burn happens exactly once per resolution — a flag and the
    // burned unit's Might are kept on the resolving chain item, in resume_data
    // slots past the ones the choice helpers own — while pickTarget asks which
    // friendly unit gets the Might.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayThis, TriggerType::AtStartOfBeginning};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto friendly = [&]() {
            std::vector<GameObjectId> out;
            for (const auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        // Burn 1; returns the burned card's printed Might if it is a unit, else 0.
        auto burn_one = [&]() {
            auto burned = ctx.executor.burn(ctx.controller, 1);
            if (burned.empty() || !ctx.state.objectExists(burned[0])) return 0;
            const auto& card = ctx.state.getObject(burned[0]);
            if (!card.isUnit()) return 0;
            return card.card_def_id != kInvalidId
                ? ctx.executor.cardDB().get(card.card_def_id).might : card.base_might;
        };
        if (!ctx.state.chain.resuming.has_value()) {
            // Direct invocation (no chain to suspend on): first friendly unit.
            const int might = burn_one();
            auto mine = friendly();
            if (might > 0 && !mine.empty()) ctx.executor.giveTemporaryMight(mine.front(), might);
            return;
        }
        {
            auto& rd = ctx.state.chain.resuming->resume_data;
            while (rd.size() <= kMightSlot) rd.push_back(0);
            if (rd[kBurnedSlot] == 0) {
                rd[kBurnedSlot] = 1;
                rd[kMightSlot] = burn_one();
            }
        }
        const int might = ctx.state.chain.resuming->resume_data[kMightSlot];
        if (might <= 0) return;
        GameObjectId picked = pickTarget(
            ctx, "Forgotten Relic: give a friendly unit +" + std::to_string(might) + " [M]",
            friendly());
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, might);
    }
private:
    static constexpr size_t kBurnedSlot = 7;   // resume_data slots 0..6 belong to the choice helpers
    static constexpr size_t kMightSlot = 8;
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
