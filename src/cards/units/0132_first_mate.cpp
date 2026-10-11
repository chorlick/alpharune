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

class FirstMate : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, ready another unit." Chosen as the ability goes on
    // the chain (CR 383.3); "another" excludes me.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            return o.id != ctx.source;
        });
        std::sort(units.begin(), units.end());
        const GameObjectId t = pickTarget(ctx, "First Mate: ready another unit", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.readyObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 132;
        d.def_id = R"RB(ogn-132-298)RB";
        d.name = R"RB(First Mate)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-132/298)RB";
        d.collector_number = 132;
        d.artist = R"RB(Slawomir Maniak)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you play me, ready another unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/85e2b436ea482d53563cc5fa955f2a867ee6c32f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_132(CardRegistry& r) {
    r.registerCard(132, std::make_unique<FirstMate>());
}

} // namespace riftbound
