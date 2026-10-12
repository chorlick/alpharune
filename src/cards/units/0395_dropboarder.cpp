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

class Dropboarder : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, if you control two or more gear, ready me." The "if"
    // is part of the trigger condition (CR 383.2.a.1): counted as I am
    // played; with fewer nothing is put on the chain.
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return gearControlled(state, controller) >= 2;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "DROPBOARDER: 2+ gear controlled, ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 395;
        d.def_id = R"RB(sfd-072-221)RB";
        d.name = R"RB(Dropboarder)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-072/221)RB";
        d.collector_number = 72;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, if you control two or more gear, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8572e4cb5af60dd548e3b87374ff1b12840d981a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_395(CardRegistry& r) {
    r.registerCard(395, std::make_unique<Dropboarder>());
}

} // namespace riftbound
