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

class TrinityForce : public SimpleEquipGear {
public:
    TrinityForce() : SimpleEquipGear(Domain::Body) {}
    const CardDef& def() const override { return def_; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,
                            const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("EQUIP_TRIGGER: Trinity Force scores 1 point -> " +
                            std::to_string(ps.score));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 437;
        d.def_id = R"RB(sfd-115-221)RB";
        d.name = R"RB(Trinity Force)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-115/221)RB";
        d.collector_number = 115;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.might_bonus = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [O] ([O]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I hold, score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e8b64985389e5792afc2a2fe0dc23ae7590d7709-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_437(CardRegistry& r) {
    r.registerCard(437, std::make_unique<TrinityForce>());
}

} // namespace riftbound
