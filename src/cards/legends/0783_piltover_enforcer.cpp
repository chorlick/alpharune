#include "cards/card.h"
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

class PiltoverEnforcer : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "if you assigned 3 or more excess damage" — not surfaced to triggers;
        // treated as satisfied (documented approximation).
        if (!ctx.state.objectExists(ctx.source)) return;
        auto findTargets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.controller != ctx.controller) continue;
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            if (ctx.state.getObject(ctx.source).is_exhausted) return false;  // need to exhaust me
            return !findTargets().empty();
        };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx,
            "Piltover Enforcer: exhaust me to ready a unit?", still_legal);
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / illegal
        GameObjectId target = pickTarget(ctx, "Piltover Enforcer: ready a unit",
                                         findTargets());
        if (target == kInvalidId || !ctx.state.objectExists(target)) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.readyObject(target);
        ctx.events.logTrace("PILTOVER ENFORCER: exhaust me -> ready " +
                            ctx.state.getObject(target).name +
                            " (excess-damage condition not engine-checked)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 783;
        d.def_id = R"RB(unl-229-219)RB";
        d.name = R"RB(Piltover Enforcer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-229/219)RB";
        d.collector_number = 229;
        d.artist = R"RB(Jonathan Santoro)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Vi)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you conquer, if you assigned 3 or more excess damage, you may exhaust me to ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/08c48ad82381bb5830a0b413d7a2f25dd2b20d76-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_783(CardRegistry& r) {
    r.registerCard(783, std::make_unique<PiltoverEnforcer>());
}

} // namespace riftbound
