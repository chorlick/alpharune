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

class OceanDrake : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may play me to an open battlefield."
    unsigned extraPlayLocations() const override { return kPlayToOpenBattlefield; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "When you play me, you may return a non-Dragon unit to its owner's hand."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    std::vector<GameObjectId> nonDragons(CardContext& ctx) const {
        return chooseableUnits(ctx.state, ctx.controller,
                               [](const GameObject& obj) { return !hasTag(obj, "Dragon"); });
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Ocean Drake: return a non-Dragon unit to hand?",
                                   [&]() { return !nonDragons(ctx).empty(); });
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Ocean Drake: return to hand", nonDragons(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.bounceToHand(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 902;
        d.def_id = R"RB(ven-115-166)RB";
        d.name = R"RB(Ocean Drake)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-115/166)RB";
        d.collector_number = 115;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Dragon)RB", R"RB(Ionia)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(You may play me to an open battlefield.
When you play me, you may return a non-Dragon unit to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bd93cc3c9165204cd3e830fc2022384bad7517be-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_902(CardRegistry& r) {
    r.registerCard(902, std::make_unique<OceanDrake>());
}

}  // namespace riftbound
