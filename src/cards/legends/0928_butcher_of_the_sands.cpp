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

class ButcherOfTheSands : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Reaction][>] [A][A], [E]: [Add] [2]. Spend this Energy only to play units
    // or activated abilities of units."
    bool hasActivatedAbility() const override { return true; }
    // [Add]: it resolves as it is activated — no chain item, no priority,
    // Focus stays (CR 430.2, 401.2; testing phase D2).
    bool abilityAddsResources() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        c.power = 2;
        c.power_any_domain = true;
        return c;
    }
    bool isReactionAbility() const override { return true; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.addUnitOnlyEnergy(ctx.controller, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 928;
        d.def_id = R"RB(ven-141-166)RB";
        d.name = R"RB(Butcher of the Sands)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-141/166)RB";
        d.collector_number = 141;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Body};
        d.tags = {R"RB(Renekton)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([Reaction][>] [A][A], [E]: [Add] [2]. Spend this Energy only to play units or activated abilities of units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7620595b36b40a0c3d05c4c5469b016d1c18c6f2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_928(CardRegistry& r) {
    r.registerCard(928, std::make_unique<ButcherOfTheSands>());
}

}  // namespace riftbound
