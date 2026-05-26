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

class Gust : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Gust", legal);
        if (picked == kInvalidId) return;
        ctx.executor.bounceToHand(picked);
    }
    // Filter to units with might <= 3.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            if (obj.current_might > 3) continue;
            targets.push_back(id);
        }
        return targets;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 169;
        d.def_id = R"RB(ogn-169-298)RB";
        d.name = R"RB(Gust)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-169/298)RB";
        d.collector_number = 169;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Return a unit at a battlefield with 3 [M] or less to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dfd3b161ab76ba0c5d503384f1289b3395434b10-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_169(CardRegistry& r) {
    r.registerCard(169, std::make_unique<Gust>());
}

} // namespace riftbound
