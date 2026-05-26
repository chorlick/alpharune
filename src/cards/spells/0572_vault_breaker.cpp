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

class VaultBreaker : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // "Give a unit [Assault 2] and [Ganking] this turn."
        if (!targets.empty()) {
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Assault, 2);
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Ganking, 0);
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 572;
        d.def_id = R"RB(unl-010-219)RB";
        d.name = R"RB(Vault Breaker)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-010/219)RB";
        d.collector_number = 10;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.assault_value = 2;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give a unit [Assault 2] and [Ganking] this turn. (+2 [M] while it's an attacker. It can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8046c9f133be83268d7dc9788abea58f461914d8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_572(CardRegistry& r) {
    r.registerCard(572, std::make_unique<VaultBreaker>());
}

} // namespace riftbound
