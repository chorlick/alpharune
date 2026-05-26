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

class ClothArmor : public SimpleEquipGear {
public:
    ClothArmor() : SimpleEquipGear(Domain::Mind) {}
    const CardDef& def() const override { return def_; }
    int equippedShield() const override { return 2; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Shield); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 387;
        d.def_id = R"RB(sfd-064-221)RB";
        d.name = R"RB(Cloth Armor)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-064/221)RB";
        d.collector_number = 64;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::QuickDraw);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.)
[Equip] [B] ([B]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Shield 2] (+2 [M] while I'm a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fe02bf01609ff2e6cc6fa8fbd578fd596ab5cb84-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_387(CardRegistry& r) {
    r.registerCard(387, std::make_unique<ClothArmor>());
}

} // namespace riftbound
