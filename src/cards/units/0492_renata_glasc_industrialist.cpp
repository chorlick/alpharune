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

// "Your tokens enter ready." Wired via a per-player flag
// (PlayerState::tokens_enter_ready) set in applyPassiveAura and consulted by
// EffectExecutor::createToken at creation time — so it forces the entering
// (exhausted-by-default) token to ready WITHOUT touching tokens exhausted later
// by use. The flag is reset+recomputed each cleanup, so it lapses when Renata
// leaves the board.

class RenataGlascIndustrialist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        state.player(controller).tokens_enter_ready = true;
    }
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
