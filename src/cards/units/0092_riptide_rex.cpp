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

class RiptideRex : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, deal 6 to an enemy unit at a battlefield." Chosen as
    // the ability goes on the chain (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto enemies = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller != ctx.controller && o.isAtBattlefield();
        });
        std::sort(enemies.begin(), enemies.end());
        const GameObjectId t = pickTarget(ctx, "Riptide Rex: deal 6 to an enemy unit", enemies);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.dealDamage(t, 6, ctx.source);
        if (ctx.state.getObject(t).location.has_value() &&
            ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 92;
        d.def_id = R"RB(ogn-092-298)RB";
        d.name = R"RB(Riptide Rex)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-092/298)RB";
        d.collector_number = 92;
        d.artist = R"RB(MAR Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.might = 6;
        d.ability_text = R"RB(When you play me, deal 6 to an enemy unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e09bf1fb6d72da31d415ea31ae7413fe7f02d650-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_92(CardRegistry& r) {
    r.registerCard(92, std::make_unique<RiptideRex>());
}

} // namespace riftbound
