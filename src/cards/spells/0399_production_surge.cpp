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

class ProductionSurge : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            // Match by name (no Mech keyword/tag in the card data — token
            // name is "Mech"). Real Mech-tag enforcement is a follow-up.
            if (obj.name.find("Mech") != std::string::npos) return 2;
        }
        return 0;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        KeywordSet kw;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech",
                                  /*might=*/3, /*tags=*/{}, kw,
                                  BaseLocation{ctx.controller},
                                  /*exhausted=*/false);
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 399;
        d.def_id = R"RB(sfd-076-221)RB";
        d.name = R"RB(Production Surge)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-076/221)RB";
        d.collector_number = 76;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(This costs [2] less if you control a Mech.
Play a 3 [M] Mech unit token to your base.
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/78783854dd9372138e599affbe96a269d8908c29-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_399(CardRegistry& r) {
    r.registerCard(399, std::make_unique<ProductionSurge>());
}

} // namespace riftbound
