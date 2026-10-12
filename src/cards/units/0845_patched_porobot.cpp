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

class PatchedPorobot : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "(I enter exhausted.)" is the default for units (CR 143.4).
    // "When you play me, if you control 3 or more other gear, draw 1."
    // (This card is a unit, so every gear you control is an "other" gear.)
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // The "if" is part of the trigger condition (CR 383.2.a.1): counted as I
    // am played; with fewer gear nothing is put on the chain.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return gearControlled(state, controller) >= 3;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "PATCHED POROBOT: 3+ gear -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 845;
        d.def_id = R"RB(ven-058-166)RB";
        d.name = R"RB(Patched Porobot)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-058/166)RB";
        d.collector_number = 58;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Poro)RB", R"RB(Piltover)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB((I enter exhausted.)
When you play me, if you control 3 or more other gear, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7667d7e0c902782c38221e9326d1bb98181ec17f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_845(CardRegistry& r) {
    r.registerCard(845, std::make_unique<PatchedPorobot>());
}

}  // namespace riftbound
