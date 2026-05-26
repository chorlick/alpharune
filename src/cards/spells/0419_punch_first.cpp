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

class PunchFirst : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Punch First", legal);
        if (picked == kInvalidId) return;
        ctx.executor.giveTemporaryMight(picked, 5);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 419;
        d.def_id = R"RB(sfd-097-221)RB";
        d.name = R"RB(Punch First)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-097/221)RB";
        d.collector_number = 97;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.power_cost = 2;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a unit +5 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d124efad90ff61d4ac29ee69fe71e48a7cd6ece2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_419(CardRegistry& r) {
    r.registerCard(419, std::make_unique<PunchFirst>());
}

} // namespace riftbound
