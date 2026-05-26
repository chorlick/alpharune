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

class RengarPouncing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool playableAsReactionToAttack() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 348;
        d.def_id = R"RB(sfd-025-221)RB";
        d.name = R"RB(Rengar, Pouncing)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-025/221)RB";
        d.collector_number = 25;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Cat)RB", R"RB(Rengar)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.assault_value = 2;
        d.keywords.set(Keyword::Assault);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve, including to a battlefield you control.)
[Assault 2] (+2 [M] while I'm an attacker.)
I can be played to a battlefield you're attacking.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e24eb3fb835c4762479da5608b018785ef22e02c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_348(CardRegistry& r) {
    r.registerCard(348, std::make_unique<RengarPouncing>());
}

} // namespace riftbound
