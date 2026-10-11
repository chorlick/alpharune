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

class SquareUp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give a unit [Assault 4] this turn."
    // "[Repeat] — Discard 1": the discard is the Repeat's cost (CR 820),
    // never part of the base effect.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.discard_cards = 1}};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Assault, 4);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 579;
        d.def_id = R"RB(unl-017-219)RB";
        d.name = R"RB(Square Up)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-017/219)RB";
        d.collector_number = 17;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] — Discard 1 (You may pay the additional cost to repeat this spell's effect.)
Give a unit [Assault 4] this turn. (+4 [M] while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5e14c09db2f064e5f6986f500a04335d73d459dd-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_579(CardRegistry& r) {
    r.registerCard(579, std::make_unique<SquareUp>());
}

} // namespace riftbound
