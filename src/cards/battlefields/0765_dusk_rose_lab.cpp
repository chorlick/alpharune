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
        if (!mine) return;
        // "kill a unit you control here" is the cost: it is paid as the
        // ability is finalized (CR 383.3), with a unit the controller picks.
        auto still_legal = [&]() { return !unitsHere(ctx).empty(); };
        if (!costAlreadyPaid(ctx) && !still_legal()) return;
        int conf = confirmOptional(ctx,
            "Dusk Rose Lab: kill a unit here to draw 1?", still_legal);
        if (conf < 1) return;    // waiting for the agent, declined, or no unit here
        if (costDueNow(ctx)) {
            const auto picked = selectObject(ctx, "Dusk Rose Lab (unit to kill)", unitsHere(ctx));
            if (!picked.has_value() || *picked == kInvalidId) return;   // waiting / none
            const auto here = unitsHere(ctx);
            if (std::find(here.begin(), here.end(), *picked) == here.end()) return;
            ctx.executor.killObject(*picked);
            // A death that was replaced is not a kill: the cost was not paid.
            if (ctx.state.getObject(*picked).location.has_value()) return;
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("DUSK ROSE LAB: killed a unit here -> draw 1");
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
