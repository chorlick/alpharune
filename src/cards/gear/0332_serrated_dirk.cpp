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

class SerratedDirk : public SimpleEquipGear {
public:
    SerratedDirk() : SimpleEquipGear(Domain::Fury) {}
    const CardDef& def() const override { return def_; }
    int equippedAssault() const override { return 2; }
    KeywordSet equippedKeywords() const override { KeywordSet k; k.set(Keyword::Assault); return k; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 332;
        d.def_id = R"RB(sfd-009-221)RB";
        d.name = R"RB(Serrated Dirk)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-009/221)RB";
        d.collector_number = 9;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [R] ([R]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB([Assault 2] (+2 [M] while I'm an attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9498c8146745124fc45a7b55184854a9c9da21f5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_332(CardRegistry& r) {
    r.registerCard(332, std::make_unique<SerratedDirk>());
}

} // namespace riftbound
