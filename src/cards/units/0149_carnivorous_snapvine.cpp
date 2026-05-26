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

class CarnivorousSnapvine : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, choose an enemy unit at a battlefield. We deal damage
    // equal to our Mights to each other." Triggers get empty targets — pick at
    // resolution.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.isAtBattlefield()) continue;
            legal.push_back(id);
        }
        GameObjectId enemy = pickTarget(ctx, "Carnivorous Snapvine (fight)", legal);
        if (enemy == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (enemy == kInvalidId || !ctx.state.objectExists(enemy)) return;
        // Collect both Mights BEFORE dealing damage (collect-then-kill).
        int my_might = ctx.state.getObject(ctx.source).current_might;
        int their_might = ctx.state.getObject(enemy).current_might;
        ctx.executor.dealDamage(enemy, my_might, ctx.source);
        ctx.executor.dealDamage(ctx.source, their_might, enemy);
        if (ctx.state.objectExists(enemy) &&
            ctx.state.getObject(enemy).hasLethalDamage())
            ctx.executor.killObject(enemy);
        if (ctx.state.objectExists(ctx.source) &&
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
