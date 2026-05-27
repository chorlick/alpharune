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

class MaraiSpire : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "While you control this battlefield, friendly [Repeat] costs cost [1] less."
    // Wired via PlayerState::repeat_cost_reduction (set in applyPassiveAura for
    // the BF controller; the spell-play path subtracts it from the Repeat tranche
    // energy, min 0). BF-card applyPassiveAura is called with the controlling
    // player, so "while you control this battlefield" is implicit.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (controller == PlayerId::None) return;  // uncontrolled
        state.player(controller).repeat_cost_reduction += 1;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 525;
        d.def_id = R"RB(sfd-211-221)RB";
        d.name = R"RB(Marai Spire)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-211/221)RB";
        d.collector_number = 211;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB(While you control this battlefield, friendly [Repeat] costs cost [1] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ff64dcbac9b392d4f6930a7359114aa68aa14ebe-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_525(CardRegistry& r) {
    r.registerCard(525, std::make_unique<MaraiSpire>());
}

} // namespace riftbound
