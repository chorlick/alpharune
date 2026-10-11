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

class Profiteer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When you play me, you may disempower something you control to empower a
    // legend, unit, or gear."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Empowered things I control (board + my legend).
        auto mine_empowered = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.controller != ctx.controller || !obj.isEmpowered()) continue;
                if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        // A legend, unit, or gear that can become Empowered once `paid` has been
        // disempowered (so `paid` itself is a legal recipient).
        auto recipients = [&](GameObjectId paid) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                const bool legend = obj.isLegend() && obj.zone == ZoneType::LegendZone;
                const bool permanent = (obj.isUnit() || obj.isGear()) && obj.location.has_value();
                if (!legend && !permanent) continue;
                if (obj.isEmpowered() && id != paid) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int conf = confirmOptional(ctx, "Profiteer: disempower something to empower something?",
            [&]() { return costAlreadyPaid(ctx) || !mine_empowered().empty(); });
        if (conf != 1) return;                        // waiting, declined or nothing Empowered
        // What is disempowered is the cost (a pick, not a target); what is
        // empowered is the target. The cost is paid as the ability goes on
        // the chain (CR 383.3), after the target is chosen.
        const auto& ri = ctx.state.chain.resuming;
        auto paid = selectObject(ctx, "Profiteer: disempower which", mine_empowered());
        if (!paid.has_value()) return;                // waiting
        GameObjectId gain = kInvalidId;
        if (*paid != kInvalidId) {
            gain = pickTarget(ctx, "Profiteer: empower which", recipients(*paid));
            if (gain == kInvalidId && ri.has_value() && ri->resume_point == 7) return;   // waiting
        }
        if (costDueNow(ctx)) {
            if (*paid == kInvalidId || gain == kInvalidId) return;   // nothing to pay with / for
            if (!ctx.executor.disempower(*paid)) return;             // cost not paid -> no effect
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (gain == kInvalidId || !ctx.state.objectExists(gain)) return;   // mistarget
        ctx.executor.empower(gain);
        ctx.events.logTrace("PROFITEER: disempowered one thing to empower another");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 869;
        d.def_id = R"RB(ven-082-166)RB";
        d.name = R"RB(Profiteer)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-082/166)RB";
        d.collector_number = 82;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may disempower something you control to empower a legend, unit, or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/27955854d3f04ed95e87152f066f99671af2f9f1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_869(CardRegistry& r) {
    r.registerCard(869, std::make_unique<Profiteer>());
}

}  // namespace riftbound
