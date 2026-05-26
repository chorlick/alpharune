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

class Highlander : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit. The next time it would die this turn, heal it,
    // exhaust it, and recall it instead."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto& unit = ctx.state.getObject(targets[0]);
        unit.death_replacement_recall_pending = true;
        ctx.events.logTrace("HIGHLANDER: " + unit.name +
                             " — next death this turn becomes heal/exhaust/recall");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 318;
        d.def_id = R"RB(ogs-020-024)RB";
        d.name = R"RB(Highlander)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-020/024)RB";
        d.collector_number = 20;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.energy_cost = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit. The next time it would die this turn, heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/18c0818cbbfdf26a1237b4f7703b1f035f47b014-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_318(CardRegistry& r) {
    r.registerCard(318, std::make_unique<Highlander>());
}

} // namespace riftbound
