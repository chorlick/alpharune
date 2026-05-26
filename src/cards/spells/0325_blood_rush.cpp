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

class BloodRush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Assault, 2);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 325;
        d.def_id = R"RB(sfd-003-221)RB";
        d.name = R"RB(Blood Rush)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-003/221)RB";
        d.collector_number = 3;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 1;
        d.assault_value = 2;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
[Repeat] [1] (You may pay the additional cost to repeat this spell's effect.)
Give a unit [Assault 2] this turn. (+2 [M] while it's an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/931b77b9ab56c3abc85686be4d2452c450f9b3e0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_325(CardRegistry& r) {
    r.registerCard(325, std::make_unique<BloodRush>());
}

} // namespace riftbound
