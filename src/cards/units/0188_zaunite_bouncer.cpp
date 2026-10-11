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

class ZauniteBouncer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, return another unit at a battlefield to its owner's
    // hand." Chosen as the ability goes on the chain (CR 383.3); "another"
    // excludes me.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.id != ctx.source && o.isAtBattlefield();
        });
        std::sort(units.begin(), units.end());
        const GameObjectId t = pickTarget(ctx, "Zaunite Bouncer: return another unit to hand",
                                          units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.bounceToHand(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 188;
        d.def_id = R"RB(ogn-188-298)RB";
        d.name = R"RB(Zaunite Bouncer)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-188/298)RB";
        d.collector_number = 188;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, return another unit at a battlefield to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0f4e8258e3920e4a4654cfe74faf8c266f2aa5dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_188(CardRegistry& r) {
    r.registerCard(188, std::make_unique<ZauniteBouncer>());
}

} // namespace riftbound
