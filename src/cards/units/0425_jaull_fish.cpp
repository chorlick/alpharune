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

class JaullFish : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I cost [2] less for each of your [Mighty] units." ([Accelerate] is
    // engine-handled.) Mighty = current_might >= 5.
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        int mighty = 0;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            if (isMighty(obj)) ++mighty;
        }
        return mighty * 2;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 425;
        d.def_id = R"RB(sfd-103-221)RB";
        d.name = R"RB(Jaull-Fish)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-103/221)RB";
        d.collector_number = 103;
        d.artist = R"RB(MAR Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 6;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
I cost [2] less for each of your [Mighty] units. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/56a31630910179a1fb2f2ddf3e6e5c9627bddf5e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_425(CardRegistry& r) {
    r.registerCard(425, std::make_unique<JaullFish>());
}

} // namespace riftbound
