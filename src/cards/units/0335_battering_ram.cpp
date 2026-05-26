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

class BatteringRam : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "I cost [1] less for each card you've played this turn, to a minimum
    // of [1]." Printed energy cost is [5]; clamp the reduction so net energy
    // never drops below 1 (the engine only floors selfCostReduction results
    // at 0, so we enforce the printed minimum here).
    static constexpr int kPrintedEnergyCost = 5;
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int played = state.player(player).cards_played_this_turn;
        int max_reduction = kPrintedEnergyCost - 1;  // floor net cost at 1
        return std::min(std::max(0, played), max_reduction);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 335;
        d.def_id = R"RB(sfd-012-221)RB";
        d.name = R"RB(Battering Ram)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-012/221)RB";
        d.collector_number = 12;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(I cost [1] less for each card you've played this turn, to a minimum of [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5661f07778201b236b014a6078003cde9af57cdd-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_335(CardRegistry& r) {
    r.registerCard(335, std::make_unique<BatteringRam>());
}

} // namespace riftbound
