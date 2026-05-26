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

class SealOfUnity : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // [Add] [Y] resolves immediately (CR 429.2) — not via the chain.
        ctx.executor.addFloatingPower(ctx.controller, Domain::Order, 1);
        ctx.events.logTrace("ACTIVATE: Seal of Unity adds [Y] (Order power)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 549;
        d.def_id = R"RB(sfd-238-221)RB";
        d.name = R"RB(Seal of Unity)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-238/221)RB";
        d.collector_number = 238;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.power_cost = 1;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([E]: [Reaction] — [Add] [Y]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/228dbde67ec37f9a0b28f81e40fce5a8fa7af56b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_549(CardRegistry& r) {
    r.registerCard(549, std::make_unique<SealOfUnity>());
}

} // namespace riftbound
