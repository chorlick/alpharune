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

class DuskRoseLab : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }

    // Units the controller controls AT this battlefield.
    std::vector<GameObjectId> unitsHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        // Find this BF's id.
        BattlefieldId my_bf = kInvalidId;
        bool found = false;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { my_bf = bf.id; found = true; break; }
        }
        if (!found) return out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto ubf = obj.battlefieldId();
            if (ubf && *ubf == my_bf) out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "your Beginning Phase": "you" is this battlefield's controller
        // (CR 190.6.d); uncontrolled, the ability does nothing.
        bool mine = false;
        for (const auto& bf : ctx.state.battlefields)
            if (bf.card_object_id == ctx.source)
                mine = bf.controller.has_value() && *bf.controller == ctx.controller;
        // "kill a unit you control here" is the cost: it is paid as the
        // ability is finalized (CR 383.3), with a unit the controller picks.
        // Both conditions are the "may"'s, with no return ahead of it: when
        // one fails confirmOptional stores the "no" on the chain item, so the
        // resolution asks nothing if a unit has arrived here since.
        auto still_legal = [&]() { return mine && !unitsHere(ctx).empty(); };
        int conf = confirmOptional(ctx,
            "Dusk Rose Lab: kill a unit here to draw 1?", still_legal);
        if (conf < 1) return;    // waiting for the agent, declined, or no unit here
        // The cost is attempted once. A death that was replaced is not a
        // kill: the cost was not paid, so there is no draw — and no second
        // attempt when the ability resolves (Card::killAsCost records the
        // failed attempt on the chain item, and the marker below is still
        // reached).
        bool paid = true;
        if (costDueNow(ctx) && !costUnpaid(ctx)) {
            const auto picked = selectObject(ctx, "Dusk Rose Lab (unit to kill)", unitsHere(ctx));
            if (!picked.has_value() || *picked == kInvalidId) return;   // waiting / none
            const auto here = unitsHere(ctx);
            if (std::find(here.begin(), here.end(), *picked) == here.end()) return;
            // "Killed" is the unit dying, not the unit leaving: a replacement
            // puts another event in the death's place and the death does not
            // occur (CR 370.1.b, 370.1.c.1; the Deathgrip example of CR
            // 359.3.e.14.b), wherever the unit ends up — still here, recalled,
            // or banished by "If it would die this turn, banish it instead."
            paid = killAsCost(ctx, *picked);
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (!paid || costUnpaid(ctx)) return; // the unit did not die: nothing was paid
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "DUSK ROSE LAB: killed a unit here -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 765;
        d.def_id = R"RB(unl-209-219)RB";
        d.name = R"RB(Dusk Rose Lab)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-209/219)RB";
        d.collector_number = 209;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of your Beginning Phase, you may kill a unit you control here to draw 1. (This happens before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fa81c474ff29b9d2c42dd4e64a5789bf793e5221-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_765(CardRegistry& r) {
    r.registerCard(765, std::make_unique<DuskRoseLab>());
}

} // namespace riftbound
