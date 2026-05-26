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

class ViktorInnovator : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play a card on an opponent's turn, play a 1 [M] Recruit unit
    // token in your base." No single "play a card" trigger exists, so register
    // for spell / unit / gear plays and gate on it being the opponent's turn.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayASpell,
                TriggerType::WhenYouPlayAUnit,
                TriggerType::WhenYouPlayAGear};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Only on an opponent's turn (the turn player is not the controller).
        if (ctx.state.turn.turn_player == ctx.controller) return;
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("VIKTOR, INNOVATOR: off-turn play -> 1[M] Recruit in base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 117;
        d.def_id = R"RB(ogn-117-298)RB";
        d.name = R"RB(Viktor, Innovator)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-117/298)RB";
        d.collector_number = 117;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Viktor)RB", R"RB(Zaun)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play a card on an opponent's turn, play a 1 [M] Recruit unit token in your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/35be0b10f13611b4f5d828f2a79001036ab57ba1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_117(CardRegistry& r) {
    r.registerCard(117, std::make_unique<ViktorInnovator>());
}

} // namespace riftbound
