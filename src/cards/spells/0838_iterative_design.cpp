#include "cards/card.h"
#include "cards/card_helpers.h"
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

class IterativeDesign : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play a 3 [M] Mech unit token." / "[Flow] [2][B]"
    // No location is printed, so the token is played like any unit: to your
    // base or to a battlefield you control that units may be played to
    // (pickTokenDest; testing phase R5). The places are passed on, so an
    // additional copy of the token (Zilean, Time Mage) has the same choice.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        const int dest = pickTokenDest(*this, ctx, "Iterative Design: where to play the Mech");
        if (dest < 0) return;   // waiting for the agent / nowhere to play it
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", /*might=*/3,
                                 {"Mech"}, KeywordSet{}, destLocation(ctx.controller, dest),
                                 /*enter_ready=*/false, tokenPlayLocations(ctx));
    }
    FlowCost flowCost() const override { return flow(2, 1, Domain::Mind); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 838;
        d.def_id = R"RB(ven-051-166)RB";
        d.name = R"RB(Iterative Design)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-051/166)RB";
        d.collector_number = 51;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Play a 3 [M] Mech unit token.
[Flow] [2][B] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8d85a05075f95b63832ed7aa15620041eacb49d3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_838(CardRegistry& r) {
    r.registerCard(838, std::make_unique<IterativeDesign>());
}

}  // namespace riftbound
