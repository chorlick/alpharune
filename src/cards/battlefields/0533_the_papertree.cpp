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

// "When you hold here, each player channels 1 rune exhausted."

class ThePapertree : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.channelRunes(PlayerId::Player1, 1, /*enter_exhausted=*/true);
        ctx.executor.channelRunes(PlayerId::Player2, 1, /*enter_exhausted=*/true);
        ctx.events.logTrace("THE PAPERTREE: hold -> each player channels 1 rune exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 533;
        d.def_id = R"RB(sfd-219-221)RB";
        d.name = R"RB(The Papertree)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-219/221)RB";
        d.collector_number = 219;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, each player channels 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c395b94a4f78b4e8b0590b56787c33600b18e358-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_533(CardRegistry& r) {
    r.registerCard(533, std::make_unique<ThePapertree>());
}

} // namespace riftbound
