#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class DaughterOfTheVoid : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool isReactionAbility() const override { return true; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[Add] [A]. Use only to play spells." Add 1 universal power.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("DAUGHTER OF THE VOID: [E] -> [Add] [A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 294;
        d.def_id = R"RB(ogn-299-298)RB";
        d.name = R"RB(Daughter of the Void)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-299/298)RB";
        d.collector_number = 299;
        d.artist = R"RB(Jason Chan)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB"};
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [A]. Use only to play spells. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/57e13462b8fea9a5cfa1b424d3ad3005f37b00ad-1488x2078.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_294(CardRegistry& r) {
    r.registerCard(294, std::make_unique<DaughterOfTheVoid>());
}

} // namespace riftbound
