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

class ViPeacekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I attack, [Stun] an enemy unit here." The unit is chosen by Vi's
    // controller as the ability goes on the chain (CR 383.3); the engine
    // hands a trigger no targets.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> enemies_here;
        if (ctx.state.objectExists(ctx.source)) {
            if (auto here = ctx.state.getObject(ctx.source).battlefieldId()) {
                const PlayerId me = ctx.controller;
                const BattlefieldId bf = *here;
                enemies_here = chooseableUnits(ctx.state, me, [me, bf](const GameObject& o) {
                    auto obf = o.battlefieldId();
                    return o.controller != me && obf && *obf == bf;
                });
                std::sort(enemies_here.begin(), enemies_here.end());
            }
        }
        const GameObjectId t = pickTarget(ctx, "Vi, Peacekeeper: stun an enemy unit here",
                                          enemies_here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.stunUnitBy(t, ctx.source);
        RIFT_TRACE(ctx.events, "VI: stun on attack");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 738;
        d.def_id = R"RB(unl-176-219)RB";
        d.name = R"RB(Vi, Peacekeeper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-176/219)RB";
        d.collector_number = 176;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Vi)RB", R"RB(Piltover)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When I attack, [Stun] an enemy unit here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/51610bbdecd77b15f58b9a968611e536ebdf445e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_738(CardRegistry& r) {
    r.registerCard(738, std::make_unique<ViPeacekeeper>());
}

} // namespace riftbound
