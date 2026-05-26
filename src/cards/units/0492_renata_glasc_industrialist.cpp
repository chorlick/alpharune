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

// "Your tokens enter ready."
// ESCALATE(tokens_enter_ready_replacement): this is a continuous replacement
// effect on token creation, but
// EffectExecutor::createToken decides ready/exhausted from its own enter_ready
// arg and does NOT consult a per-player "tokens enter ready" flag or any aura.
// Modeling it via applyPassiveAura is impossible because the aura pass can't
// distinguish a token that just ENTERED exhausted from one exhausted by use
// (readying the latter would be wrong). Left unimplemented pending a
// createToken replacement hook / per-player flag the executor reads.

class RenataGlascIndustrialist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 492;
        d.def_id = R"RB(sfd-171-221)RB";
        d.name = R"RB(Renata Glasc, Industrialist)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-171/221)RB";
        d.collector_number = 171;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Your tokens enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/edacb8a82d840f2fe46233de9157319a1dc6361a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_492(CardRegistry& r) {
    r.registerCard(492, std::make_unique<RenataGlascIndustrialist>());
}

} // namespace riftbound
