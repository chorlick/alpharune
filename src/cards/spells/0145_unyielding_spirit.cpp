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

class UnyieldingSpirit : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Prevent ALL spell and ability damage this turn": whoever deals it,
        // to whoever's units — an opponent's spell hitting the opponent's own
        // units included. The executor checks the flag of the target's and of
        // the source's controller, so both players carry it.
        ctx.state.player(ctx.controller).prevent_spell_ability_damage_this_turn = true;
        ctx.state.player(opponent(ctx.controller)).prevent_spell_ability_damage_this_turn = true;
        RIFT_TRACE(ctx.events, "UNYIELDING SPIRIT: all spell/ability damage is "
                            "prevented this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 145;
        d.def_id = R"RB(ogn-145-298)RB";
        d.name = R"RB(Unyielding Spirit)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-145/298)RB";
        d.collector_number = 145;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Prevent all spell and ability damage this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9afea96ec04ce0c2ee76c4affbf4e6df470e7647-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_145(CardRegistry& r) {
    r.registerCard(145, std::make_unique<UnyieldingSpirit>());
}

} // namespace riftbound
