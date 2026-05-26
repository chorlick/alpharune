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

class UndyingLegion : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool requiresLegion() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 587;
        d.def_id = R"RB(unl-025-219)RB";
        d.name = R"RB(Undying Legion)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-025/219)RB";
        d.collector_number = 25;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion][>] You may play me from your trash for [3][R]. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3a6955daff2982e0ba4417533052d5ea334232fb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_587(CardRegistry& r) {
    r.registerCard(587, std::make_unique<UndyingLegion>());
}

} // namespace riftbound
