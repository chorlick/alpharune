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

class TiannaCrownguard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Clause 1: "[Deflect]" — engine keyword, set in def.
    // Clause 2: "While I'm at a battlefield, opponents can't gain points." While
    //   a Tianna I control is at a battlefield, set my opponent's
    //   cannot_gain_points (reset each recalc; consulted by scoreHold/scoreConquer).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.location.has_value() ||
                !std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            state.player(opponent(controller)).cannot_gain_points = true;
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 383;
        d.def_id = R"RB(sfd-060-221)RB";
        d.name = R"RB(Tianna Crownguard)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-060/221)RB";
        d.collector_number = 60;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
While I'm at a battlefield, opponents can't gain points.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2a1876aeedae7310409d0eaaf453507188b0c82b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_383(CardRegistry& r) {
    r.registerCard(383, std::make_unique<TiannaCrownguard>());
}

} // namespace riftbound
