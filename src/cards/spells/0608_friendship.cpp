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

class Friendship : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Friendship", legal);
        if (picked == kInvalidId) return;
        auto presence = scanFriendlyTags(ctx.state, ctx.controller);
        int bonus = presence.count();  // 0..4
        if (bonus <= 0) return;
        ctx.events.logTrace("FRIENDSHIP: +" + std::to_string(bonus) +
                             "[M] this turn (tags present: B=" +
                             (presence.bird?"1":"0") + " C=" + (presence.cat?"1":"0") +
                             " D=" + (presence.dog?"1":"0") + " P=" + (presence.poro?"1":"0") + ")");
        ctx.executor.giveTemporaryMight(picked, bonus);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 608;
        d.def_id = R"RB(unl-046-219)RB";
        d.name = R"RB(Friendship)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-046/219)RB";
        d.collector_number = 46;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a unit. Give it +1 [M] this turn for each of the following tags among your units — Bird, Cat, Dog, and Poro.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4a1a08ec8c23a2663babd2fc683481888347407a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_608(CardRegistry& r) {
    r.registerCard(608, std::make_unique<Friendship>());
}

} // namespace riftbound
