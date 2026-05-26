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

class SuperMegaDeathRocket : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty()) {
            ctx.executor.dealDamage(targets[0], 5, ctx.source);
            if (ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).hasLethalDamage()) {
                ctx.executor.killObject(targets[0]);
            }
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // ENGINE GAP: "When you conquer, you may discard 1 to return this from your
    // trash to your hand." Conquer triggers fire only on units at the scoring
    // battlefield; the engine does not scan trash-resident spells for conquer
    // triggers, so this recursion clause cannot be wired from the card file.
    // The deal-5 effect above is correct.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 250;
        d.def_id = R"RB(ogn-252-298)RB";
        d.name = R"RB(Super Mega Death Rocket!)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-252/298)RB";
        d.collector_number = 252;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Jinx)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 5 to a unit.
When you conquer, you may discard 1 to return this from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/95d6fd4d7944e759cfe7ee5e208fa329b719333b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_250(CardRegistry& r) {
    r.registerCard(250, std::make_unique<SuperMegaDeathRocket>());
}

} // namespace riftbound
