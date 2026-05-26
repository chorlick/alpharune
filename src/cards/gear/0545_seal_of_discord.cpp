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

class SealOfDiscord : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.addFloatingPower(ctx.controller, Domain::Chaos, 1);
        ctx.events.logTrace("SEAL OF DISCORD: [E] -> [Add] [P] (Chaos)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 545;
        d.def_id = R"RB(sfd-234-221)RB";
        d.name = R"RB(Seal of Discord)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-234/221)RB";
        d.collector_number = 234;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.power_cost = 1;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [P]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9263b8f36725a4cec320c1049608824ae72f6237-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_545(CardRegistry& r) {
    r.registerCard(545, std::make_unique<SealOfDiscord>());
}

} // namespace riftbound
