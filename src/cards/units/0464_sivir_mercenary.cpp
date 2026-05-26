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

class SivirMercenary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you've spent at least [A][A] this turn, I have +2 [M] and [Ganking]."
    // ([Accelerate] engine-handled.)
    // ENGINE GAP: there is no per-turn cumulative power-spent counter on
    // PlayerState (only last_spell_energy_spent / total_energy_spent, which are
    // per-play energy, not cumulative power). The "[A][A] spent this turn"
    // condition cannot be evaluated, so this conditional static is left
    // unimplemented pending a power-spent-this-turn tracker in the cost system.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 464;
        d.def_id = R"RB(sfd-143-221)RB";
        d.name = R"RB(Sivir, Mercenary)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-143/221)RB";
        d.collector_number = 143;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Sivir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Accelerate] (You may pay [1][P] as an additional cost to have me enter ready.)
If you've spent at least [A][A] this turn, I have +2 [M] and [Ganking]. (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb07b4adc6e301fae973ab13156028390facbad2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_464(CardRegistry& r) {
    r.registerCard(464, std::make_unique<SivirMercenary>());
}

} // namespace riftbound
