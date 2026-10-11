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

// "When you hold here, you may pay [A][A][A][A] to score 1 point."

class PowerNexus : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // CR 445.2 / 164.2.b: the [A][A][A][A] may be paid with floating power
        // or by recycling runes; the payment is only offered when it can be
        // made, and it is made exactly once (after the yes/no is answered).
        int conf = confirmOptional(ctx, "Power Nexus: pay [A][A][A][A] to score 1?",
            [&]() { return ctx.executor.payableAnyPower(ctx.controller) >= 4; });
        if (conf != 1) return;
        if (costDueNow(ctx) && !ctx.executor.payAnyPower(ctx.controller, 4)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.scorePoints(ctx.controller, 1);   // CR 194.1.c
        RIFT_TRACE(ctx.events, "POWER NEXUS: paid [A][A][A][A] -> score 1 point");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 528;
        d.def_id = R"RB(sfd-214-221)RB";
        d.name = R"RB(Power Nexus)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-214/221)RB";
        d.collector_number = 214;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may pay [A][A][A][A] to score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/653db9a5f2aead0191c8d61da3d1876fb894b545-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_528(CardRegistry& r) {
    r.registerCard(528, std::make_unique<PowerNexus>());
}

} // namespace riftbound
