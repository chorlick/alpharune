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

// "Give a friendly unit +1 [M] this turn and [Stun] an enemy unit at its
// location." Two targets: A = friendly unit (buffed), B = enemy unit at A's
// location (stunned). A is buffed even if no legal B exists.

class HeroicCharge : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller == controller) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal_friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_friendly.push_back(id);
        }
        auto enemy_fn = [&](GameObjectId picked_a) {
            std::vector<GameObjectId> legal_enemy;
            if (!ctx.state.objectExists(picked_a)) return legal_enemy;
            auto loc = ctx.state.getObject(picked_a).location;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value() || !obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.untargetable_by_enemy) continue;
                if (obj.location != loc) continue;  // same location as the friendly
                legal_enemy.push_back(id);
            }
            return legal_enemy;
        };
        auto [friendly, enemy] = pickTargetPair(ctx, "Heroic Charge",
                                                 legal_friendly, enemy_fn);
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        // Buff the friendly unit (even if there's no legal enemy to stun).
        if (friendly != kInvalidId && ctx.state.objectExists(friendly))
            ctx.executor.giveTemporaryMight(friendly, 1);
        // Stun the chosen enemy unit at the friendly's location.
        if (enemy != kInvalidId && ctx.state.objectExists(enemy))
            ctx.executor.stunUnitBy(enemy, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 717;
        d.def_id = R"RB(unl-155-219)RB";
        d.name = R"RB(Heroic Charge)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-155/219)RB";
        d.collector_number = 155;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a friendly unit +1 [M] this turn and [Stun] an enemy unit at its location. (A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/944b89db8a7f7961a28e66ec4fb7d30bb863f324-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_717(CardRegistry& r) {
    r.registerCard(717, std::make_unique<HeroicCharge>());
}

} // namespace riftbound
