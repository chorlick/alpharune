#include "cards/card.h"
#include "cards/card_helpers.h"
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
    // "The first time ... each turn" is the trigger condition itself (CR
    // 383.1): judged when the unit dies, and it is the turn's first friendly
    // death — counted for the player whether or not this card was on the
    // board for it, and whether or not an earlier ability of it resolved
    // (PlayerState::unit_deaths_this_turn, counted before the death is
    // announced; testing phase B6).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) &&
               state.player(controller).unit_deaths_this_turn == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "WRAITH OF ECHOES: first friendly death this turn -> draw 1");
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
