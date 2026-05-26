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

class TreasureTrove : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // PARTIAL: modeled on WhenIDie (no "leaves the board" trigger exists, so a
    // bounce-to-hand departure does not fire the draw + channel).
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("TREASURE TROVE: left board -> draw 1 + channel 1 "
                            "rune exhausted");
    }

    // "[P], [E]: Kill this."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .power = 1,
                              .power_domain = Domain::Chaos};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.killObject(ctx.source);
        ctx.events.logTrace("TREASURE TROVE: [P],[E] -> kill this");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 186;
        d.def_id = R"RB(ogn-186-298)RB";
        d.name = R"RB(Treasure Trove)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-186/298)RB";
        d.collector_number = 186;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When this leaves the board, draw 1 and channel 1 rune exhausted.
[P], [E]: Kill this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/682f5ee828ede4dbaf428cd0666db64ee8bae722-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_186(CardRegistry& r) {
    r.registerCard(186, std::make_unique<TreasureTrove>());
}

} // namespace riftbound
