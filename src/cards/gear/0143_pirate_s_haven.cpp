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

class PirateSHaven : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you ready a friendly unit, give it +1 [M] this turn."
    // Fires via WhenYouReadyAFriendlyUnit; the readied unit is the trigger's
    // subject (chain item's triggering_subject).
    TriggerType triggerType() const override {
        return TriggerType::WhenYouReadyAFriendlyUnit;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId subject = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (subject == kInvalidId || !ctx.state.objectExists(subject)) return;
        ctx.executor.giveTemporaryMight(subject, 1);
        ctx.events.logTrace("PIRATE'S HAVEN: readied friendly -> +1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 143;
        d.def_id = R"RB(ogn-143-298)RB";
        d.name = R"RB(Pirate's Haven)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-143/298)RB";
        d.collector_number = 143;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you ready a friendly unit, give it +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1991b64d58cbd3698574f44b404f7a88d6403134-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_143(CardRegistry& r) {
    r.registerCard(143, std::make_unique<PirateSHaven>());
}

} // namespace riftbound
