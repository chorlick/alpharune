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

class Daisy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay() const override { return true; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return scanFriendlyTags(state, player).count();
    }
    // "When I attack while your units have all 4 tags, [Stun] an enemy unit
    // here." The tag condition is judged when she attacks; the unit is her
    // controller's choice, made as the ability goes on the chain (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return scanFriendlyTags(state, controller).allFour();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
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
        const GameObjectId t = pickTarget(ctx, "Daisy!: stun an enemy unit here", enemies_here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.stunUnitBy(t, ctx.source);   // attributed, for "when you stun"
        RIFT_TRACE(ctx.events, "DAISY: stunned " + ctx.state.getObject(t).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 754;
        d.def_id = R"RB(unl-196-219)RB";
        d.name = R"RB(Daisy!)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-196/219)RB";
        d.collector_number = 196;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(I enter ready.
Reduce my cost by [1] for each of the following tags among your units — Bird, Cat, Dog, and Poro.
When I attack while your units have all 4 tags, [Stun] an enemy unit here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ada85b2a6076b60933831454d02a21da3654362c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_754(CardRegistry& r) {
    r.registerCard(754, std::make_unique<Daisy>());
}

} // namespace riftbound
