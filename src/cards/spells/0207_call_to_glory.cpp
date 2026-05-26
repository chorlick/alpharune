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

class CallToGlory : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.giveTemporaryMight(targets[0], 3);
        ctx.events.logTrace("CALL TO GLORY: +3 [M] this turn "
                            "(buff-spend-for-free opt unmodeled)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 207;
        d.def_id = R"RB(ogn-207-298)RB";
        d.name = R"RB(Call to Glory)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-207/298)RB";
        d.collector_number = 207;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As you play this, you may spend a buff as an additional cost. If you do, ignore this spell's cost.
Give a unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ced53bc3fb15f263471067fc3868295b09e62a07-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_207(CardRegistry& r) {
    r.registerCard(207, std::make_unique<CallToGlory>());
}

} // namespace riftbound
