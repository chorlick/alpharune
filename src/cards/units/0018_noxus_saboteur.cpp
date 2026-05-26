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

// "Your opponents' [Hidden] cards can't be revealed here."
// [Hidden] (the printed keyword) is engine-handled (units hide facedown at
// controlled battlefields). But the printed ability — preventing an opponent
// from REVEALING their Hidden cards at this unit's battlefield — has no engine
// primitive. The engine models hiding (executeHideCard) and playing a card
// from facedown (executePlayFromHidden), but there is no "reveal an opponent's
// Hidden card" action/event for any restriction to hook. No IntentType, event,
// or per-BF flag covers it.
// ESCALATE(reveal_hidden_restriction): the engine needs a reveal-facedown
// mechanic (action + event) AND a per-location restriction the reveal path
// consults to forbid revealing an opponent's Hidden card at a battlefield
// where this unit is present.

class NoxusSaboteur : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 18;
        d.def_id = R"RB(ogn-018-298)RB";
        d.name = R"RB(Noxus Saboteur)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-018/298)RB";
        d.collector_number = 18;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(Your opponents' [Hidden] cards can't be revealed here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b78f0c822cb984db24ac3f1956cc8c10f8f88b22-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_18(CardRegistry& r) {
    r.registerCard(18, std::make_unique<NoxusSaboteur>());
}

} // namespace riftbound
