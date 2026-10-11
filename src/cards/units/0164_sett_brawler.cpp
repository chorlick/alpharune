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

class SettBrawler_164 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I'm played and when I conquer, buff me. (If I don't have a buff, I
    // get a +1 [M] buff.)"
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Nothing happens to a Sett that left the board or already has a buff
        // (EffectExecutor::buffUnit).
        if (ctx.executor.buffUnit(ctx.source))
            RIFT_TRACE(ctx.events, "SETT, BRAWLER: buff me (+1M buff)");
    }
    // "Spend my buff: Give me +4 [M] this turn." Spending the buff is the
    // activation cost: paid as the ability is activated (CR 355), by the Sett
    // that activates it, so only a buffed Sett is offered the ability.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        return Card::canActivateAbilityAt(state, controller, source, ability_index) &&
               state.getObject(source).buff_count > 0;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& self = ctx.state.getObject(ctx.source);
        if (costDueNow(ctx)) {
            if (!self.location.has_value() || self.buff_count <= 0) return;
            ctx.executor.spendBuff(ctx.source, ctx.controller);   // CR 427.2; "When you spend a buff"
        }
        if (finalizingChoices(ctx)) return;   // costs end here (CR 355)
        if (!self.location.has_value()) return;   // left the board
        ctx.executor.giveTemporaryMight(ctx.source, 4);
        RIFT_TRACE(ctx.events, "SETT, BRAWLER: spent a buff for +4M this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 164;
        d.def_id = R"RB(ogn-164-298)RB";
        d.name = R"RB(Sett, Brawler)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-164/298)RB";
        d.collector_number = 164;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Sett)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When I'm played and when I conquer, buff me. (If I don't have a buff, I get a +1 [M] buff.)
Spend my buff: Give me +4 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/00ddd4d478f2e49e18a60ed67f4d1452041b7da3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_164(CardRegistry& r) {
    r.registerCard(164, std::make_unique<SettBrawler_164>());
}

} // namespace riftbound
