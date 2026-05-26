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

class ObeliskOfPower : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "At the start of each player's first Beginning Phase, that player channels
    //  1 rune." Fires for the turn player; gate to their FIRST turn only.
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.player(ctx.controller).turns_taken != 1) return;  // first only
        ctx.executor.channelRunes(ctx.controller, 1);
        ctx.events.logTrace("OBELISK OF POWER: first Beginning Phase -> channel 1 rune");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 279;
        d.def_id = R"RB(ogn-284-298)RB";
        d.name = R"RB(Obelisk of Power)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-284/298)RB";
        d.collector_number = 284;
        d.artist = R"RB(Chris Kintner)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of each player's first Beginning Phase, that player channels 1 rune.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0ef4890cdc3145616fcf41290a13c63b1afe4e0d-1038x744.png)RB";
        d.banned = true;  // tournament ban (formerly cards/ban-list.csv)
        return d;
    }();
};

}  // anonymous namespace

void register_card_279(CardRegistry& r) {
    r.registerCard(279, std::make_unique<ObeliskOfPower>());
}

} // namespace riftbound
