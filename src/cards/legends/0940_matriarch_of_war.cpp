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

class MatriarchOfWar : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you empower something else, empower me."
    TriggerType triggerType() const override { return TriggerType::WhenYouEmpower; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.empower(ctx.source);
    }
    // "Disempower me, [A], [E]: Ready a unit."
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .power = 1, .power_any_domain = true,
                     .disempower_self = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .needs_activation_time_target = true,
        }};
    }
    // "Ready a unit.": any unit, either player's — the text has no "friendly".
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int /*ability_index*/) const override {
        return chooseableUnits(state, controller, [](const GameObject&) { return true; });
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Matriarch of War",
                         enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.readyObject(picked);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 940;
        d.def_id = R"RB(ven-153-166)RB";
        d.name = R"RB(Matriarch of War)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-153/166)RB";
        d.collector_number = 153;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Ambessa)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you empower something else, empower me. (I become Empowered if I'm not already.)
Disempower me, [A], [E]: Ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/49e81916d7ae72b24de845223e1798652d26e789-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_940(CardRegistry& r) {
    r.registerCard(940, std::make_unique<MatriarchOfWar>());
}

}  // namespace riftbound
