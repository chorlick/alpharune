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

class LotusTrap : public SpellCard {
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
        GameObjectId picked = pickTarget(ctx, "Lotus Trap", legal);
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        auto& target = ctx.state.getObject(picked);
        target.damage_doubled_this_turn = true;
        ctx.events.logTrace("LOTUS TRAP: damage-doubling applied to " +
                             target.name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 575;
        d.def_id = R"RB(unl-013-219)RB";
        d.name = R"RB(Lotus Trap)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-013/219)RB";
        d.collector_number = 13;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Reaction] (Play any time, even before spells and abilities resolve.)
Choose a unit. Double all damage that would be dealt to it this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9490f8c80da1bf4467e14c39b66ae4262e5f7f7b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_575(CardRegistry& r) {
    r.registerCard(575, std::make_unique<LotusTrap>());
}

} // namespace riftbound
