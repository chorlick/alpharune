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

class Possession : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Take control of it and recall it. (Send it to your base.)"
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.takeControl(targets[0], ctx.controller, /*until_end_of_turn=*/false);
        ctx.executor.moveToBase(targets[0]);
        ctx.events.logTrace("POSSESSION: take control + recall to base");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 203;
        d.def_id = R"RB(ogn-203-298)RB";
        d.name = R"RB(Possession)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-203/298)RB";
        d.collector_number = 203;
        d.artist = R"RB(League of Legends)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 8;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose an enemy unit at a battlefield. Take control of it and recall it. (Send it to your base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f26462174fd01407f25f7d49a70862d2b9af35cc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_203(CardRegistry& r) {
    r.registerCard(203, std::make_unique<Possession>());
}

} // namespace riftbound
