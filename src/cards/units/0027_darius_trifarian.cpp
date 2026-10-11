#include "cards/card.h"
#include "cards/card_helpers.h"
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
    // "your second card" may be Darius himself (WhenYouPlayMe) or any other
    // card played while he is on the board.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe,
                TriggerType::WhenYouPlayAUnit,
                TriggerType::WhenYouPlayASpell,
                TriggerType::WhenYouPlayAGear};
    }
    // "Your second card in a turn" is which play the triggering card was
    // (playOrdinalThisTurn, stamped on the card as it is played), not the
    // count when this is checked: a spell's play triggers fire as it resolves,
    // after any cards played in response. A token is not a card.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType trigger, GameObjectId subject) const override {
        const GameObjectId played = trigger == TriggerType::WhenYouPlayMe ? source : subject;
        if (played == kInvalidId || !state.objectExists(played))
            return state.player(controller).cards_played_this_turn == 2;
        if (state.getObject(played).isToken()) return false;
        return playOrdinalThisTurn(state, played) == 2;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // left the board
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
