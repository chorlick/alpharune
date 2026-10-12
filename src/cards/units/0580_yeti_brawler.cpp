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

// "When I conquer, if you assigned 3 or more excess damage, play two Gold gear
//  tokens exhausted."

class YetiBrawler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    // "if you assigned 3 or more excess damage" is part of the trigger
    // condition (CR 383.2.a.1), asked when the conquer happens: in the combat
    // for the battlefield I conquered (the one I am at).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        BattlefieldId bf = state.player(controller).last_combat_bf;
        if (state.objectExists(source))
            if (auto here = state.getObject(source).battlefieldId()) bf = *here;
        return excessCombatDamageAt(state, controller, bf) >= 3;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldExhausted(ctx);
        createGoldExhausted(ctx);
        RIFT_TRACE(ctx.events, "YETI BRAWLER: conquer with 3+ excess damage -> two Gold tokens, exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 580;
        d.def_id = R"RB(unl-018-219)RB";
        d.name = R"RB(Yeti Brawler)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-018/219)RB";
        d.collector_number = 18;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I conquer, if you assigned 3 or more excess damage, play two Gold gear tokens exhausted. (They have "[Reaction][>] Kill this, [E]: [Add] [A]."))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/38b59ae384ef6df8c94d5c51418101b2630b43a7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_580(CardRegistry& r) {
    r.registerCard(580, std::make_unique<YetiBrawler>());
}

} // namespace riftbound
