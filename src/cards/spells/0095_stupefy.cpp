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

class Stupefy : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isReactionAbility() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.giveTemporaryMight(targets[0], -1, /*minimum=*/1);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 95;
        d.def_id = R"RB(ogn-095-298)RB";
        d.name = R"RB(Stupefy)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-095/298)RB";
        d.collector_number = 95;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit -1 [M] this turn, to a minimum of 1 [M]. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/837a5976192bbf8bdb4086429802167548ecc119-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_95(CardRegistry& r) {
    r.registerCard(95, std::make_unique<Stupefy>());
}

} // namespace riftbound
