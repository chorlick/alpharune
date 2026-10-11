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

class ZileanTimeMage : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Once each turn, if you would play a token unit while I'm at a battlefield,
    //  you may play that token and an additional copy of it instead."
    // PlayerState::plays_additional_token_copy is set here while a Zilean is at a
    // battlefield; EffectExecutor::createToken then asks the controller, once
    // each turn (PlayerState::additional_token_copy_used_turn stamps the turn it was
    // used), whether to play an additional copy. The copy is played by the
    // same instruction as the token: at the location that instruction names,
    // or at one the controller chooses again when the instruction let them
    // choose (createToken's `location_choices`). A card that then changes
    // "its" tokens reads EffectExecutor::lastTokensCreated (Keeper of Masks).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.isAtBattlefield()) {
                state.player(controller).plays_additional_token_copy = true;
                return;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 648;
        d.def_id = R"RB(unl-086-219)RB";
        d.name = R"RB(Zilean, Time Mage)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-086/219)RB";
        d.collector_number = 86;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Icathia)RB", R"RB(Zilean)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Once each turn, if you would play a token unit while I'm at a battlefield, you may play that token and an additional copy of it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2a3165b43bd9878c0eb519bc25d370064aacac35-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_648(CardRegistry& r) {
    r.registerCard(648, std::make_unique<ZileanTimeMage>());
}

} // namespace riftbound
