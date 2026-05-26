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

class SeatOfPower : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int count = 0;
        for (const auto& bf : ctx.state.battlefields) {
            if (!bf.controller.has_value() || *bf.controller != ctx.controller) continue;
            if (bf.card_object_id == ctx.source) continue;  // skip "here"
            count++;
        }
        if (count > 0) {
            ctx.executor.drawCards(ctx.controller, count);
        }
        ctx.events.logTrace("SEAT OF POWER: conquer -> draw " + std::to_string(count));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 531;
        d.def_id = R"RB(sfd-217-221)RB";
        d.name = R"RB(Seat of Power)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-217/221)RB";
        d.collector_number = 217;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, draw 1 for each other battlefield you or allies control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8b9db30f8eedd5e5463a0fceaeaf90069bce39ae-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_531(CardRegistry& r) {
    r.registerCard(531, std::make_unique<SeatOfPower>());
}

} // namespace riftbound
