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

class RekSaiBreacher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // All three clauses are engine-handled — no card code required:
    //   - "[Accelerate]" + "[Assault]": engine keywords (set in def below).
    //   - "Friendly units played from anywhere other than a player's hand have
    //     [Accelerate].": engine-handled in GameEngine cost path, which checks
    //     for a friendly on-board Rek'Sai (card_def_id 352) and auto-applies
    //     Accelerate when current_play_source != Hand.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 352;
        d.def_id = R"RB(sfd-029-221)RB";
        d.name = R"RB(Rek'Sai, Breacher)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-029/221)RB";
        d.collector_number = 29;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Rek'Sai)RB", R"RB(The Void)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.assault_value = 1;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Assault);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
[Assault] (+1 [M] while I'm an attacker.)
Friendly units played from anywhere other than a player's hand have [Accelerate].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/999e2d25ea32da971e7b99b6e75558286c17949e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_352(CardRegistry& r) {
    r.registerCard(352, std::make_unique<RekSaiBreacher>());
}

} // namespace riftbound
