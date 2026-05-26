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

class Disintegrate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        ctx.executor.dealDamage(targets[0], 3, ctx.source);
        bool killed = false;
        if (ctx.state.objectExists(targets[0]) &&
            ctx.state.getObject(targets[0]).hasLethalDamage()) {
            ctx.executor.killObject(targets[0]);
            killed = true;
        }
        // "If this kills it, do this: draw 1." Only draw on a kill.
        if (killed) ctx.executor.drawCards(ctx.controller, 1);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 5;
        d.def_id = R"RB(ogn-005-298)RB";
        d.name = R"RB(Disintegrate)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-005/298)RB";
        d.collector_number = 5;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 3 to a unit at a battlefield. If this kills it, do this: draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a27374ac3a81f3dfefb43c3c3237c23b4883cb5a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_5(CardRegistry& r) {
    r.registerCard(5, std::make_unique<Disintegrate>());
}

} // namespace riftbound
