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

class VoidSeeker : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty()) {
            ctx.executor.dealDamage(targets[0], 4, ctx.source);
            if (ctx.state.objectExists(targets[0]) &&
                ctx.state.getObject(targets[0]).hasLethalDamage()) {
                ctx.executor.killObject(targets[0]);
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 24;
        d.def_id = R"RB(ogn-024-298)RB";
        d.name = R"RB(Void Seeker)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-024/298)RB";
        d.collector_number = 24;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Deal 4 to a unit at a battlefield. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5b64b63c6be58ff3c33c7fb3c2dcb168d64e287e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_24(CardRegistry& r) {
    r.registerCard(24, std::make_unique<VoidSeeker>());
}

} // namespace riftbound
