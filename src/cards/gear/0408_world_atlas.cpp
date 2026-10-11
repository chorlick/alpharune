#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class WorldAtlas : public SimpleEquipGear {
public:
    WorldAtlas() : SimpleEquipGear(Domain::Mind) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& /*targets*/) override {
        // CR 149.2: gear is played to its controller's Base unless an effect
        // says otherwise — not to the battlefield the equipped unit holds.
        // createGoldExhausted: exhausted, in base, as the Gold card (CR 187.5).
        for (int i = 0; i < 2; ++i) createGoldExhausted(ctx);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 408;
        d.def_id = R"RB(sfd-086-221)RB";
        d.name = R"RB(World Atlas)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-086/221)RB";
        d.collector_number = 86;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [B] ([B]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I hold, play two Gold gear tokens exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b8d96e128c19c5ff368119a60759cffa644e942-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_408(CardRegistry& r) {
    r.registerCard(408, std::make_unique<WorldAtlas>());
}

} // namespace riftbound
