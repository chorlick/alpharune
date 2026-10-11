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

class MonasteryOfHirana : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override {
        return TriggerType::WhenYouConquerHere;
    }
    // "When you conquer here, you may spend a buff to draw 1." The buff is a
    // cost: it is spent as the ability is finalized (CR 383.3), from a buffed
    // unit the controller picks.
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto buffed = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value() && obj.buff_count > 0) out.push_back(id);
            return out;
        };
        auto still_legal = [&]() { return !buffed().empty(); };
        if (!costAlreadyPaid(ctx) && !still_legal()) return;
        int conf = confirmOptional(ctx,
            "Monastery of Hirana: spend a buff to draw 1?", still_legal);
        if (conf < 1) return;   // waiting on the agent, declined or no buff to spend
        if (costDueNow(ctx)) {
            auto unit = selectObject(ctx, "Monastery of Hirana: spend which unit's buff",
                                     buffed());
            if (!unit.has_value() || *unit == kInvalidId) return;   // waiting / none left
            auto& obj = ctx.state.getObject(*unit);
            ctx.executor.spendBuff(*unit, ctx.controller);   // CR 427.2; "When you spend a buff"
            ctx.events.logTrace("MONASTERY OF HIRANA: spent a buff on " + obj.name);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MONASTERY OF HIRANA: buff spent -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 277;
        d.def_id = R"RB(ogn-282-298)RB";
        d.name = R"RB(Monastery of Hirana)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-282/298)RB";
        d.collector_number = 282;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may spend a buff to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8767d9ed30e2873d3fa1045f973be229da56175d-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_277(CardRegistry& r) {
    r.registerCard(277, std::make_unique<MonasteryOfHirana>());
}

} // namespace riftbound
