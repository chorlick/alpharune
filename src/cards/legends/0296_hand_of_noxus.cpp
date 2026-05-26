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

class HandOfNoxus : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool isReactionAbility() const override { return true; }
    bool requiresLegion() const override { return true; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[Legion] — [Add] [1]." Add 1 floating energy.
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.events.logTrace("HAND OF NOXUS: [E] -> [Add] [1]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 296;
        d.def_id = R"RB(ogn-302-298)RB";
        d.name = R"RB(Hand of Noxus)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-302/298)RB";
        d.collector_number = 302;
        d.artist = R"RB(Peter Kim)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Darius)RB"};
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Legion);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction], [Legion] — [Add] [1]. (Abilities that add resources can't be reacted to. Get the effect if you've played a card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/37d43efe09ccfe5d72ce37fdc27599bd5ef736af-1488x2078.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_296(CardRegistry& r) {
    r.registerCard(296, std::make_unique<HandOfNoxus>());
}

} // namespace riftbound
