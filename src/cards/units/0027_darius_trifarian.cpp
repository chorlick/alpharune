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

class DariusTrifarian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayAUnit,
                TriggerType::WhenYouPlayASpell,
                TriggerType::WhenYouPlayAGear};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Trigger fires AFTER the play, so cards_played_this_turn already
        // counts the just-played card. The second card => count == 2.
        if (ctx.state.player(ctx.controller).cards_played_this_turn != 2) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("DARIUS TRIFARIAN: second card played, +2M and ready");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 27;
        d.def_id = R"RB(ogn-027-298)RB";
        d.name = R"RB(Darius, Trifarian)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-027/298)RB";
        d.collector_number = 27;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Trifarian)RB", R"RB(Darius)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play your second card in a turn, give me +2 [M] this turn and ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bf7a4900fd2296972c1305a4707c23860bb0522e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_27(CardRegistry& r) {
    r.registerCard(27, std::make_unique<DariusTrifarian>());
}

} // namespace riftbound
