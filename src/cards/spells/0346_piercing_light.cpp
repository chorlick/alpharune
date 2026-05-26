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

class PiercingLight : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        // count=2 with optional=true — first must be a unit at a BF
        // (enforced); the second is "up to one other unit" so optional.
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true,
                                   .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        ctx.executor.dealDamage(targets[0], 2, ctx.source);
        // "then deal 2 to up to one OTHER unit" — the second hit must be a
        // different unit from the first.
        if (targets.size() >= 2 && targets[1] != targets[0] &&
            ctx.state.objectExists(targets[1])) {
            ctx.executor.dealDamage(targets[1], 2, ctx.source);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 346;
        d.def_id = R"RB(sfd-023-221)RB";
        d.name = R"RB(Piercing Light)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-023/221)RB";
        d.collector_number = 23;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2][R] (You may pay the additional cost to repeat this spell's effect.)
Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7fb3283b11fb8e5b5b08f9ead9b98c695b75bff0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_346(CardRegistry& r) {
    r.registerCard(346, std::make_unique<PiercingLight>());
}

} // namespace riftbound
