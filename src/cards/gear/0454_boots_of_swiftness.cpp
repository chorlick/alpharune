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

class BootsOfSwiftness : public SimpleEquipGear {
public:
    BootsOfSwiftness() : SimpleEquipGear(Domain::Chaos) {}
    const CardDef& def() const override { return def_; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Ganking); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 454;
        d.def_id = R"RB(sfd-133-221)RB";
        d.name = R"RB(Boots of Swiftness)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-133/221)RB";
        d.collector_number = 133;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [P] ([P]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Ganking] (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a8a8e977f7427227adac69b5d2db07f643d0df09-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_454(CardRegistry& r) {
    r.registerCard(454, std::make_unique<BootsOfSwiftness>());
}

} // namespace riftbound
