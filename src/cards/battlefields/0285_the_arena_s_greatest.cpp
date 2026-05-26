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
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // "first Beginning Phase" per player: gate by player index key.
        std::string key = "__arenas_greatest_fired_" +
                          std::to_string(playerIndex(ctx.controller));
        if (self.card_counters[key] != 0) return;  // already fired for this player
        self.card_counters[key] = 1;
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("THE ARENA'S GREATEST: first Beginning Phase -> +1 point ("
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
        return d;
    }();
};

}  // anonymous namespace

void register_card_285(CardRegistry& r) {
    r.registerCard(285, std::make_unique<TheArenaSGreatest>());
}

} // namespace riftbound
