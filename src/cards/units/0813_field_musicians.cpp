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

class FieldMusicians : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you play me, give a unit +3 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId tgt = kInvalidId;
        {   // the engine never hands a trigger targets: the card picks
            tgt = pickTarget(ctx, "Field Musicians: give a unit +3 [M]",
                             chooseableUnits(ctx.state, ctx.controller,
                                             [](const GameObject&) { return true; }));
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.giveTemporaryMight(tgt, 3);
        ctx.events.logTrace("FIELD MUSICIANS: +3 [M] this turn to " +
                            ctx.state.getObject(tgt).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 813;
        d.def_id = R"RB(ven-026-166)RB";
        d.name = R"RB(Field Musicians)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-026/166)RB";
        d.collector_number = 26;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When you play me, give a unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e880946b6c99023d9fb1e9aa7d23a0c673b0f6ac-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_813(CardRegistry& r) {
    r.registerCard(813, std::make_unique<FieldMusicians>());
}

}  // namespace riftbound
