#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class GetExcited : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId target = targets.empty() ? kInvalidId : targets[0];
        GameObjectId source = ctx.source;
        discardOneThenAct(ctx, "Get Excited!: discard 1",
            [target, source](CardContext& c, GameObjectId discarded) {
                if (target == kInvalidId || !c.state.objectExists(target)) return;
                if (discarded == kInvalidId) return;
                // Read the discarded card's printed Energy cost from its def.
                const auto& obj = c.state.getObject(discarded);
                int dmg = 0;
                if (obj.card_def_id != kInvalidId) {
                    dmg = c.executor.cardDB().get(obj.card_def_id).energy_cost;
                }
                if (dmg <= 0) return;
                c.executor.dealDamage(target, dmg, source);
                c.events.logTrace("GET EXCITED!: dealt " + std::to_string(dmg) +
                                  " (discarded card Energy cost)");
            });
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 8;
        d.def_id = R"RB(ogn-008-298)RB";
        d.name = R"RB(Get Excited!)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-008/298)RB";
        d.collector_number = 8;
        d.artist = R"RB(Original Force Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Discard 1. Deal its Energy cost as damage to a unit at a battlefield. (Ignore its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2906c932c482af17fbb2979a8c42a6992f95d6a6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_8(CardRegistry& r) {
    r.registerCard(8, std::make_unique<GetExcited>());
}

} // namespace riftbound
