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

class Wallop : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Ready a unit." (main effect — fully implemented)
        if (!targets.empty())
            ctx.executor.readyObject(targets[0]);
    }
    // "As you play this, you may spend a buff as an additional cost. If you do,
    // ignore this spell's cost."
    // ENGINE LIMITATION: optionalAdditionalCost() only pays energy/power and
    // does not grant a cost waiver, and selfCostReduction() has no agent
    // decision point to model the optional "spend a buff" choice. A
    // spend-a-buff-to-ignore-cost replacement requires engine support (out of
    // scope). The optional discount is left unimplemented; the spell still
    // costs its printed amount.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 146;
        d.def_id = R"RB(ogn-146-298)RB";
        d.name = R"RB(Wallop)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-146/298)RB";
        d.collector_number = 146;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
As you play this, you may spend a buff as an additional cost. If you do, ignore this spell's cost.
Ready a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7bbd2eb8ce224e1872a1c920d7c64d796b2355cd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_146(CardRegistry& r) {
    r.registerCard(146, std::make_unique<Wallop>());
}

} // namespace riftbound
