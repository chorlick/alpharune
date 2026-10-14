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

class TheArenaSGreatest : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "At the start of each player's first Beginning Phase, that player gains
    //  1 point." The ability is the battlefield controller's (CR 190.6.a; the
    //  turn player's while it is uncontrolled, 190.6.b); "that player" is the
    //  player whose Beginning Phase it is. "first" is part of the trigger
    //  condition (CR 383.1): a later Beginning Phase puts nothing on the
    //  chain (findings G9, G10).
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.turn.turn_player != PlayerId::None &&
               state.player(state.turn.turn_player).turns_taken == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId that_player = ctx.state.turn.turn_player;   // still their Beginning Phase
        ctx.executor.scorePoints(that_player, 1);   // CR 194.1.c
        const auto& ps = ctx.state.player(that_player);
        RIFT_TRACE(ctx.events, "THE ARENA'S GREATEST: first Beginning Phase -> +1 point ("
                            + std::to_string(ps.score) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 285;
        d.def_id = R"RB(ogn-290-298)RB";
        d.name = R"RB(The Arena's Greatest)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-290/298)RB";
        d.collector_number = 290;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of each player's first Beginning Phase, that player gains 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2afd8bdfdd42b1595af0746a6c6b92879ac770d6-1038x744.png)RB";
        d.banned = true;  // tournament ban (2026-07-24)
        return d;
    }();
};

}  // anonymous namespace

void register_card_285(CardRegistry& r) {
    r.registerCard(285, std::make_unique<TheArenaSGreatest>());
}

} // namespace riftbound
