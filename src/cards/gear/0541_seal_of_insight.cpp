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

class SealOfInsight : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // "[E]: [Reaction] — [Add] [B]." Resolves immediately (CR 429.2),
        // not via the chain — adds 1 Mind power to the controller's pool.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Mind, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Insight adds [B] (Mind power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 541;
        d.def_id = R"RB(sfd-229-221)RB";
        d.name = R"RB(Seal of Insight)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-229/221)RB";
        d.collector_number = 229;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.power_cost = 1;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [B]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/eb4559c507f09cabc3735973a1b20d83688d2f9d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_541(CardRegistry& r) {
    r.registerCard(541, std::make_unique<SealOfInsight>());
}

} // namespace riftbound
