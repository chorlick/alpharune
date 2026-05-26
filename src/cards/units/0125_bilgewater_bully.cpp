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

class BilgewaterBully : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 125;
        d.def_id = R"RB(ogn-125-298)RB";
        d.name = R"RB(Bilgewater Bully)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-125/298)RB";
        d.collector_number = 125;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(While I'm buffed, I have [Ganking]. (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/474f66ffa1ecebd9e0341d644cb82a3b8135eece-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_125(CardRegistry& r) {
    r.registerCard(125, std::make_unique<BilgewaterBully>());
}

} // namespace riftbound
