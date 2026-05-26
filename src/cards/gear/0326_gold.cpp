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

class Gold : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Pay the "kill this" part of the cost — the engine has already
        // exhausted us. CR 183.1: tokens cease to exist on leaving the
        // board, handled by killObject's super_type=Token branch.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("GOLD: kill+[E] -> +1 [A]");
        ctx.executor.killObject(ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 326;
        d.def_id = R"RB(sfd-t03)RB";
        d.name = R"RB(Gold)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-T03)RB";
        d.collector_number = 3;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB(Kill this, [E]: [Reaction] — [Add] [A]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e878c39b562a4e870e93b819c6d85cdf3fdc5238-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_326(CardRegistry& r) {
    r.registerCard(326, std::make_unique<Gold>());
}

} // namespace riftbound
