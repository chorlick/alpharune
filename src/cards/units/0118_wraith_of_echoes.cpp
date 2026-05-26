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

class WraithOfEchoes : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "The first time a friendly unit dies each turn, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenAFriendlyUnitDies; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__wraith_draw_turn");
        if (it != self.card_counters.end() && it->second == ctx.state.turn.turn_number)
            return;  // already drew this turn
        self.card_counters["__wraith_draw_turn"] = ctx.state.turn.turn_number;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("WRAITH OF ECHOES: first friendly death this turn -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 118;
        d.def_id = R"RB(ogn-118-298)RB";
        d.name = R"RB(Wraith of Echoes)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-118/298)RB";
        d.collector_number = 118;
        d.artist = R"RB(Michal Ivan)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(The first time a friendly unit dies each turn, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/10785ab5267ca3ff2dea081d8d18ba47c3858517-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_118(CardRegistry& r) {
    r.registerCard(118, std::make_unique<WraithOfEchoes>());
}

} // namespace riftbound
