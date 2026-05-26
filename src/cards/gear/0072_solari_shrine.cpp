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

class SolariShrine : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you kill a stunned enemy unit, you may exhaust this to draw 1."
    // The dying unit's prior stunned state is not surfaced to the trigger
    // (cleared/gone by resolve), so the "stunned" condition is treated as
    // satisfied (documented approximation, as with Piltover Enforcer/Gauntlets).
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Solari Shrine: exhaust to draw 1?",
                                   still_legal);
        if (conf < 1) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SOLARI SHRINE: exhaust -> draw 1 "
                            "(stunned condition not engine-checked)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 72;
        d.def_id = R"RB(ogn-072-298)RB";
        d.name = R"RB(Solari Shrine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-072/298)RB";
        d.collector_number = 72;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you kill a stunned enemy unit, you may exhaust this to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/139a6d1c62a717bab4cdd75926e7f4aba6efb15d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_72(CardRegistry& r) {
    r.registerCard(72, std::make_unique<SolariShrine>());
}

} // namespace riftbound
