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

class TheGrandPlaza : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, if you have 7+ units here, you win the game."
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Locate this BF (its card object is ctx.source).
        BattlefieldId my_bf = kInvalidId;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { my_bf = bf.id; break; }
        }
        if (my_bf == kInvalidId) return;
        int units_here = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() == my_bf) units_here++;
        }
        if (units_here >= 7) {
            ctx.state.game_over = true;
            ctx.state.winner = ctx.controller;
            ctx.state.game_over_reason = "The Grand Plaza win condition (7+ units here)";
            ctx.events.logTrace("THE GRAND PLAZA: 7+ units here -> win the game");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 288;
        d.def_id = R"RB(ogn-293-298)RB";
        d.name = R"RB(The Grand Plaza)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-293/298)RB";
        d.collector_number = 293;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, if you have 7+ units here, you win the game.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f9ca8d04cb269ad99bca7e6dae874cad1bec336a-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_288(CardRegistry& r) {
    r.registerCard(288, std::make_unique<TheGrandPlaza>());
}

} // namespace riftbound
