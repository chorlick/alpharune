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

class RipperSBay : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a unit here is returned to a player's hand, that player may pay [1]
    // to channel 1 rune exhausted." Fired by TriggerManager::onUnitReturnedToHand
    // with ctx.controller = the bounced unit's owner ("that player").
    TriggerType triggerType() const override {
        return TriggerType::WhenAUnitReturnsToHandHere;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Ripper's Bay: pay [1] to channel 1 rune exhausted?",
                                   still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("RIPPER'S BAY: paid [1] -> channel 1 rune exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 770;
        d.def_id = R"RB(unl-214-219)RB";
        d.name = R"RB(Ripper's Bay)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-214/219)RB";
        d.collector_number = 214;
        d.artist = R"RB(MAR Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a unit here is returned to a player's hand, that player may pay [1] to channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6d0540edbabcdb7d5a6859ce2f820f744cb498c-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_770(CardRegistry& r) {
    r.registerCard(770, std::make_unique<RipperSBay>());
}

} // namespace riftbound
