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

class MorganaVindictive : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // [Ambush] is engine-handled (keyword).
    // "When you play me, deal damage to a unit equal to the damage marked on it."
    // The unit is chosen at resolution (pickTarget) so the amount reflects the
    // damage marked at that moment.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId tgt = kInvalidId;
        {   // the engine never hands a trigger targets: the card picks
            tgt = pickTarget(ctx, "Morgana, Vindictive: choose a unit",
                             chooseableUnits(ctx.state, ctx.controller,
                                             [](const GameObject&) { return true; }));
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        const auto& unit = ctx.state.getObject(tgt);
        if (!unit.isUnit() || !unit.location.has_value()) return;
        const int amount = unit.damage_marked;
        if (amount <= 0) return;
        ctx.executor.dealDamage(tgt, amount, ctx.source);
        ctx.events.logTrace("MORGANA: dealt " + std::to_string(amount) +
                            " (damage already marked) to a unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 804;
        d.def_id = R"RB(ven-017-166)RB";
        d.name = R"RB(Morgana, Vindictive)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-017/166)RB";
        d.collector_number = 17;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Demacia)RB", R"RB(Morgana)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When you play me, deal damage to a unit equal to the damage marked on it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/04f3ff3a25b08c9af33090e7a533396583f5e433-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_804(CardRegistry& r) {
    r.registerCard(804, std::make_unique<MorganaVindictive>());
}

}  // namespace riftbound
