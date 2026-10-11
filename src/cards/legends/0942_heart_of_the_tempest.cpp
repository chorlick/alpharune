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

class HeartOfTheTempest : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a card from anywhere other than your hand, empower me."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACardNotFromHand; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.empower(ctx.source);
    }
    // "[Action][>] Disempower me, [E]: Give a unit [Assault 2] this turn."
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .disempower_self = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = true,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int /*ability_index*/) const override {
        return chooseableUnits(state, controller, [](const GameObject&) { return true; });
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = !targets.empty() ? targets[0]
            : pickTarget(ctx, "Heart of the Tempest",
                         enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryKeyword(picked, Keyword::Assault, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 942;
        d.def_id = R"RB(ven-155-166)RB";
        d.name = R"RB(Heart of the Tempest)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-155/166)RB";
        d.collector_number = 155;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Order, Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Kennen)RB"};
        d.champion_tag = R"RB(Kennen)RB";  // Yordle is a plain tag (CR 103.3.a.2)
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a card from anywhere other than your hand, empower me.
[Action][>] Disempower me, [E]: Give a unit [Assault 2] this turn. (+2 [M] while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0eab83392b310417d2630d50a3bfee3dd02b31c4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_942(CardRegistry& r) {
    r.registerCard(942, std::make_unique<HeartOfTheTempest>());
}

}  // namespace riftbound
