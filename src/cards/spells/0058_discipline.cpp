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

class Discipline : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        bool suspended = false;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Discipline", legal);
            // Distinguish suspend vs no-targets via resume_point.
            // pickTarget reserves slots 6/7/8; 7 = suspended.
            if (picked == kInvalidId &&
                ctx.state.chain.resuming &&
                ctx.state.chain.resuming->resume_point == 7) {
                suspended = true;
            }
        }
        if (suspended) return;
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.giveTemporaryMight(picked, 2);
        }
        // Draw rider always runs (even if target fizzled).
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("DISCIPLINE: +2M to target + draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 58;
        d.def_id = R"RB(ogn-058-298)RB";
        d.name = R"RB(Discipline)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-058/298)RB";
        d.collector_number = 58;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit +2 [M] this turn. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8b9613970b505e3ad6abe2d51d091778314a7d48-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_58(CardRegistry& r) {
    r.registerCard(58, std::make_unique<Discipline>());
}

} // namespace riftbound
