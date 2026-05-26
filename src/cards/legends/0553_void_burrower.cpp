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

class VoidBurrower : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 553;
        d.def_id = R"RB(sfd-243-221)RB";
        d.name = R"RB(Void Burrower)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-243/221)RB";
        d.collector_number = 243;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you conquer, you may exhaust me to reveal the top 2 cards of your Main Deck. You may banish one, then play it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/968eb5c484a25bbebd162f31736024b4ff3b0d07-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_553(CardRegistry& r) {
    r.registerCard(553, std::make_unique<VoidBurrower>());
}

} // namespace riftbound
