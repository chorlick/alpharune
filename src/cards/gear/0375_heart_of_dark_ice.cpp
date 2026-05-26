#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class HeartOfDarkIce : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Heart of Dark Ice", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, 3);
        ctx.events.logTrace("HEART OF DARK ICE: +3M to " +
                             ctx.state.getObject(picked).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 375;
        d.def_id = R"RB(sfd-052-221)RB";
        d.name = R"RB(Heart of Dark Ice)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-052/221)RB";
        d.collector_number = 52;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([E]: Give a unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/73d8251c3bd8f7cd77010e9628515cd7fa462386-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_375(CardRegistry& r) {
    r.registerCard(375, std::make_unique<HeartOfDarkIce>());
}

} // namespace riftbound
