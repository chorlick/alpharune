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

class BladeTwirler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "The first time I move each turn, choose a player. They [Burn 1]."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    // "The first time I move each turn": judged when the move happens. Every
    // move bumps GameObject::moves_this_turn before its event is announced,
    // so the first move of the turn is the one that leaves it at 1. No flag
    // is written, so nothing has to be undone if the ability is re-chosen.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).moves_this_turn == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Mode 0 = the opponent, mode 1 = yourself.
        int mode = pickMode(ctx, "Blade Twirler: choose a player to [Burn 1]", 2,
                            {"Opponent burns 1", "You burn 1"});
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (mode < 0) return;  // waiting for agent
        PlayerId who = (mode == 0) ? opponent(ctx.controller) : ctx.controller;
        ctx.executor.burn(who, 1);
        RIFT_TRACE(ctx.events, std::string("BLADE TWIRLER: first move -> ") +
                            toString(who) + " burns 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 789;
        d.def_id = R"RB(ven-002-166)RB";
        d.name = R"RB(Blade Twirler)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-002/166)RB";
        d.collector_number = 2;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Yordle)RB", R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(The first time I move each turn, choose a player. They [Burn 1]. (They put the top card of their Main Deck into their trash.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b222082d5d38630c4d2542ed24c0442881e3de18-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_789(CardRegistry& r) {
    r.registerCard(789, std::make_unique<BladeTwirler>());
}

}  // namespace riftbound
