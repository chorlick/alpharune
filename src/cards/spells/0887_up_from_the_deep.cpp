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

class UpFromTheDeep : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Play two 1 [M] Tentacle unit tokens from Bilgewater." CR 187.10: a
    // Tentacle token is a domainless 1 [M] unit token with the Bilgewater tag.
    // Each is played to the controller's base or a battlefield they control.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> made;
        auto make = [&](LocationId loc) {
            return ctx.executor.createToken(ctx.controller, CardType::Unit, "Tentacle", 1,
                                            {"Bilgewater"}, KeywordSet{}, loc,
                                            /*enter_ready=*/false);
        };
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        playTokensChoosingLocations(ctx, "Up from the Deep: play a Tentacle", 2, make, made);
    }
    // "[Flow] [3]"
    FlowCost flowCost() const override { return flow(3); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 887;
        d.def_id = R"RB(ven-100-166)RB";
        d.name = R"RB(Up from the Deep)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-100/166)RB";
        d.collector_number = 100;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Play two 1 [M] Tentacle unit tokens from Bilgewater.
[Flow] [3] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/245942e8b2bf073d5d6d57074474ec273557f4df-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_887(CardRegistry& r) {
    r.registerCard(887, std::make_unique<UpFromTheDeep>());
}

}  // namespace riftbound
