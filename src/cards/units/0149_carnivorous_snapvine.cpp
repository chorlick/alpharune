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

class CarnivorousSnapvine : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, choose an enemy unit at a battlefield. We deal damage
    // equal to our Mights to each other." Triggers get empty targets — pick at
    // resolution.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Enemy units at a battlefield, minus those that can't be chosen by
        // enemy spells and abilities.
        auto legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.controller != ctx.controller && o.isAtBattlefield();
        });
        std::sort(legal.begin(), legal.end());
        GameObjectId enemy = pickTarget(ctx, "Carnivorous Snapvine (fight)", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (enemy == kInvalidId) return;      // none / mistarget / waiting
        // "We deal damage ... to each other": nothing happens once I've left
        // the board (bounced or killed in response).
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        // Collect both Mights BEFORE dealing damage (collect-then-kill).
        int my_might = ctx.state.getObject(ctx.source).current_might;
        int their_might = ctx.state.getObject(enemy).current_might;
        ctx.executor.dealDamage(enemy, my_might, ctx.source);
        ctx.executor.dealDamage(ctx.source, their_might, enemy);
        if (ctx.state.getObject(enemy).location.has_value() &&
            ctx.state.getObject(enemy).hasLethalDamage())
            ctx.executor.killObject(enemy);
        if (ctx.state.getObject(ctx.source).location.has_value() &&
            ctx.state.getObject(ctx.source).hasLethalDamage())
            ctx.executor.killObject(ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 149;
        d.def_id = R"RB(ogn-149-298)RB";
        d.name = R"RB(Carnivorous Snapvine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-149/298)RB";
        d.collector_number = 149;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, choose an enemy unit at a battlefield. We deal damage equal to our Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5ceaec3452d623931b5ac93d0af26b180b9646de-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_149(CardRegistry& r) {
    r.registerCard(149, std::make_unique<CarnivorousSnapvine>());
}

} // namespace riftbound
