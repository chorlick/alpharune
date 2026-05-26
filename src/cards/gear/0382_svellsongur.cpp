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

class Svellsongur : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        return standardEquip(ctx, ctx.source, unit,
                                         /*energy_cost=*/1, Domain::Calm);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 382;
        d.def_id = R"RB(sfd-059-221)RB";
        d.name = R"RB(Svellsongur)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-059/221)RB";
        d.collector_number = 59;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][G] ([1][G]: Attach this to a unit you control.)
As this is attached to a unit, copy that unit's text to this Equipment's effect text for as long as this is attached to it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bc254398dfb5db217327b56862011a2fd6020789-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_382(CardRegistry& r) {
    r.registerCard(382, std::make_unique<Svellsongur>());
}

} // namespace riftbound
