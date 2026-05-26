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

class Hexdrinker : public SimpleEquipGear {
public:
    Hexdrinker() : SimpleEquipGear(Domain::Body) {}
    const CardDef& def() const override { return def_; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Deflect); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 424;
        d.def_id = R"RB(sfd-102-221)RB";
        d.name = R"RB(Hexdrinker)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-102/221)RB";
        d.collector_number = 102;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.might_bonus = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [O] ([O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7fdd555cbf9ef8c4ff8bd3ba38d187842de563d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_424(CardRegistry& r) {
    r.registerCard(424, std::make_unique<Hexdrinker>());
}

} // namespace riftbound
