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
    // Wired via PlayerState::zilean_present (set here while a Zilean is at a BF)
    // + EffectExecutor::createToken, which spawns one extra copy of a friendly
    // token unit once per turn (PlayerState::zilean_double_token_turn stamps the
    // turn). Approximated as automatic (token creation is not an agent decision
    // point), and keyed on token CREATION (the engine's token-unit entry path).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.isAtBattlefield()) {
                state.player(controller).zilean_present = true;
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
