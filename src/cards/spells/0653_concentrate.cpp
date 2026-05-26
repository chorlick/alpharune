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

class Concentrate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 2);
    }
    bool requiresLevel() const override { return true; }
    int levelThreshold() const override { return 6; }
    // "[Level 6] This costs [2] less. [Level 11] This costs [4] less instead."
    // Level hooks are not consumed by the engine; implement the tiered cost
    // reduction inline via selfCostReduction (consulted by canAfford/payCardCost).
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int xp = state.player(player).xp;
        if (xp >= 11) return 4;
        if (xp >= 6)  return 2;
        return 0;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 653;
        d.def_id = R"RB(unl-091-219)RB";
        d.name = R"RB(Concentrate)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-091/219)RB";
        d.collector_number = 91;
        d.artist = R"RB(Kudos Productions & 黯荧岛Dark Glow)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 5;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB(Draw 2.
[Level 6][>] This costs [2] less. (While you have 6+ XP, get the effect.)
[Level 11][>] This costs [4] less instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5e032420b6dcbdd96dca122b4d7875868feb82ed-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_653(CardRegistry& r) {
    r.registerCard(653, std::make_unique<Concentrate>());
}

} // namespace riftbound
