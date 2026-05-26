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

class SealOfStrength : public GearCard {
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
        ctx.executor.addFloatingPower(ctx.controller, Domain::Order, 1);
        ctx.events.logTrace("SEAL OF STRENGTH: Add [O]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 542;
        d.def_id = R"RB(sfd-231-221)RB";
        d.name = R"RB(Seal of Strength)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-231/221)RB";
        d.collector_number = 231;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.power_cost = 1;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [O]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dd5c24995d5c6d327b5101da3d6426f1a75a49ef-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_542(CardRegistry& r) {
    r.registerCard(542, std::make_unique<SealOfStrength>());
}

} // namespace riftbound
