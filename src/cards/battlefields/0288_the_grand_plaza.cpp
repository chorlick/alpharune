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
    // "if you have 7+ units here" is part of the trigger condition (CR
    // 383.2.a.1): counted when the hold happens. With fewer nothing is put on
    // the chain; with seven the ability resolves even if a unit has left.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        // Locate this BF (its card object is `source`).
        BattlefieldId my_bf = kInvalidId;
        for (const auto& bf : state.battlefields) {
            if (bf.card_object_id == source) { my_bf = bf.id; break; }
        }
        if (my_bf == kInvalidId) return false;
        int units_here = 0;
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (obj.battlefieldId() == my_bf) units_here++;
        }
        return units_here >= 7;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.game_over = true;
        ctx.state.winner = ctx.controller;
        ctx.state.game_over_reason = "The Grand Plaza win condition (7+ units here)";
        RIFT_TRACE(ctx.events, "THE GRAND PLAZA: 7+ units here -> win the game");
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
