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

// [Vision] is engine-handled.
// "[Action][>] Kill this, [E]: Give a unit +2 [M] this turn."

class DiviningShells : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = true, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Divining Shells: give a unit +2 [M]",
                                 enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // Additional cost: "Kill this."
        if (ctx.state.objectExists(ctx.source)) ctx.executor.killObject(ctx.source);
        ctx.executor.giveTemporaryMight(picked, 2);
        ctx.events.logTrace("DIVINING SHELLS: killed self -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 723;
        d.def_id = R"RB(unl-161-219)RB";
        d.name = R"RB(Divining Shells)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-161/219)RB";
        d.collector_number = 161;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play this, look at the top card of your Main Deck. You may recycle it.)
[Action][>] Kill this, [E]: Give a unit +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f2b626602f5db047b96cf9112474d42bf5b483e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_723(CardRegistry& r) {
    r.registerCard(723, std::make_unique<DiviningShells>());
}

} // namespace riftbound
