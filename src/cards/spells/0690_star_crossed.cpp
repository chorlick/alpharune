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

class StarCrossed : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    // Phase 6q+ — defer pair selection so the policy head gets
    // distinct slots per (friendly, enemy) pair instead of
    // collapsing all O(friends × enemies) variants into one Play
    // slot. Sequential MakeChoice: first picks friendly, second
    // picks enemy. Each pick is policy-head-visible.
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal_friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value()) continue;
            if (!obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_friendly.push_back(id);
        }
        auto enemy_fn = [&](GameObjectId /*picked_a*/) {
            std::vector<GameObjectId> legal_enemy;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.untargetable_by_enemy) continue;
                legal_enemy.push_back(id);
            }
            return legal_enemy;
        };
        auto [friendly, enemy] = pickTargetPair(ctx, "Star-Crossed",
                                                  legal_friendly,
                                                  enemy_fn);
        // Suspend detection: if either is kInvalidId AND we're at
        // a prompt-publish resume_point (10 or 12), return now —
        // chain re-enters after agent picks.
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly != kInvalidId && ctx.state.objectExists(friendly)) {
            ctx.executor.bounceToHand(friendly);
        }
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            ctx.executor.bounceToHand(enemy);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 690;
        d.def_id = R"RB(unl-128-219)RB";
        d.name = R"RB(Star-Crossed)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-128/219)RB";
        d.collector_number = 128;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Return a friendly unit and an enemy unit to their owners' hands.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0bfa45e6e86dfc256de0163f96273b761e9592fb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_690(CardRegistry& r) {
    r.registerCard(690, std::make_unique<StarCrossed>());
}

} // namespace riftbound
