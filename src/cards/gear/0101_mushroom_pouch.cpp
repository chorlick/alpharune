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

class MushroomPouch : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    // "if you control a facedown card at a battlefield": a hidden card sits in
    // a battlefield's facedown zone — it has no board location, hidden_at says
    // which battlefield.
    static bool controlsFacedownCard(const GameState& state, PlayerId player) {
        for (const auto& [id, obj] : state.objects)
            if (obj.controller == player && obj.is_hidden && obj.hidden_at != kInvalidId)
                return true;
        return false;
    }
    // An "if" in the trigger: checked when it would trigger and again as it
    // resolves.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return controlsFacedownCard(state, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!controlsFacedownCard(ctx.state, ctx.controller)) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "MUSHROOM POUCH: facedown card present -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 101;
        d.def_id = R"RB(ogn-101-298)RB";
        d.name = R"RB(Mushroom Pouch)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-101/298)RB";
        d.collector_number = 101;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of your Beginning Phase, if you control a facedown card at a battlefield, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ef5b3125fda91c0d0a02b7e6dfb7d3d5bafa8bd0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_101(CardRegistry& r) {
    r.registerCard(101, std::make_unique<MushroomPouch>());
}

} // namespace riftbound
