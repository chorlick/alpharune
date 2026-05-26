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

class TacticalRetreat : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto& unit = ctx.state.getObject(targets[0]);
        unit.death_replacement_recall_pending = true;
        ctx.events.logTrace("TACTICAL RETREAT: " + unit.name +
                             " — next death this turn becomes heal/exhaust/recall");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 737;
        d.def_id = R"RB(unl-175-219)RB";
        d.name = R"RB(Tactical Retreat)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-175/219)RB";
        d.collector_number = 175;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit. The next time it would die this turn, heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cd5abf6c27f5faac247f6044306db3d710fb9d61-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_737(CardRegistry& r) {
    r.registerCard(737, std::make_unique<TacticalRetreat>());
}

} // namespace riftbound
