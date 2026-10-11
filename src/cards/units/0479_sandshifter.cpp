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

class Sandshifter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // "When you play me, kill an enemy unit with 3 [M] or less."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto small_enemies = chooseableUnits(ctx.state, me, [me](const GameObject& o) {
            return o.controller != me && o.current_might <= 3;
        });
        const GameObjectId t = pickTarget(ctx, "Sandshifter: kill an enemy unit with 3 [M] or less",
                                          small_enemies);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget (it grew) / waiting
        ctx.executor.killObject(t);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 479;
        d.def_id = R"RB(sfd-158-221)RB";
        d.name = R"RB(Sandshifter)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-158/221)RB";
        d.collector_number = 158;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.might = 6;
        d.ability_text = R"RB(When you play me, kill an enemy unit with 3 [M] or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/57f72b30862074bb5d1f302e86ee3cec024cd980-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_479(CardRegistry& r) {
    r.registerCard(479, std::make_unique<Sandshifter>());
}

} // namespace riftbound
