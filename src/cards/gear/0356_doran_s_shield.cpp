#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class DoranSShield : public SimpleEquipGear {
public:
    DoranSShield() : SimpleEquipGear(Domain::Calm) {}
    const CardDef& def() const override { return def_; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Tank); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 356;
        d.def_id = R"RB(sfd-033-221)RB";
        d.name = R"RB(Doran's Shield)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-033/221)RB";
        d.collector_number = 33;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.might_bonus = 1;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [G] ([G]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Tank] (I must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/49a24d4c6d770ed233c4a69bf8d87385bfc997d8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_356(CardRegistry& r) {
    r.registerCard(356, std::make_unique<DoranSShield>());
}

} // namespace riftbound
