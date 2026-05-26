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

class Boneshiver : public SimpleEquipGear {
public:
    Boneshiver() : SimpleEquipGear(Domain::Body, 1) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 439;
        d.def_id = R"RB(sfd-118-221)RB";
        d.name = R"RB(Boneshiver)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-118/221)RB";
        d.collector_number = 118;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][O] ([1][O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c3e4d8c3f3975a7429428ac90e5ebcffa1f8e5a9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_439(CardRegistry& r) {
    r.registerCard(439, std::make_unique<Boneshiver>());
}

} // namespace riftbound
