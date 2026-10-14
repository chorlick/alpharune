#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class HeraldOfTheArcane : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }
    // "Play a 1 [M] Recruit unit token." No location is printed (unlike
    // Viktor, Leader's "into your base"), and a token is played (CR 350.2):
    // its controller's base or a battlefield they control, their choice (CR
    // 355.2.a), made as the ability is activated (CR 355).
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        const int dest = pickTokenDest(*this, ctx, "Herald of the Arcane: play the Recruit to");
        if (dest == -1) return;               // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (dest < 0) return;                 // nowhere to play it
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 /*might=*/1, /*tags=*/{"Recruit"}, KeywordSet{},
                                 destLocation(ctx.controller, dest), /*enter_ready=*/false,
                                 tokenPlayLocations(ctx));
        RIFT_TRACE(ctx.events, "HERALD OF THE ARCANE: [1],[E] -> 1[M] Recruit token");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 298;
        d.def_id = R"RB(ogn-308-298)RB";
        d.name = R"RB(Herald of the Arcane)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-308/298)RB";
        d.collector_number = 308;
        d.artist = R"RB(Rudy Siswanto)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Viktor)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB([1], [E]: Play a 1 [M] Recruit unit token.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a39902058db800a4318f3333dddb8a62a8751d7c-1488x2078.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_298(CardRegistry& r) {
    r.registerCard(298, std::make_unique<HeraldOfTheArcane>());
}

} // namespace riftbound
