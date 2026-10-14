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

class AstralHeron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play your first card each turn, if I'm at a battlefield, the
    //  next card you play this turn costs [2][A][A] less." (erratum,
    //  errata/vendetta-errata.txt: a discount nobody used ends with the turn.)
    // "Your first card each turn" is the card that was played first
    // (playOrdinalThisTurn, stamped on the card as it is played) — not the
    // first play this ability hears of: a spell's play triggers only once it
    // has resolved (CR 420.4.a), after any cards played in response, and a
    // response that was countered is never announced at all. The Heron itself
    // can be that first card (WhenYouPlayMe). A token is not a card.
    // If the first card is itself countered this does not trigger at all
    // (CR 420.4.a.1: an ability that triggers on playing a card does not
    // trigger for a countered card), and the next card is not "your first
    // card" either: the countered card was played (CR 420.4.b and its Defy
    // examples — it still counts for Legion and "no other cards this turn").
    // The [A][A] half is CostModifier::power_reduction, which the engine's
    // cost path subtracts from the card's Power cost (clamped at 0).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayACard, TriggerType::WhenYouPlayMe};
    }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId subject) const override {
        // "if I'm at a battlefield" is part of the trigger condition (CR
        // 383.2.a.1): asked when the card is played, not again as the ability
        // resolves. A Heron played straight to a battlefield is already there
        // when its own play is announced.
        if (!state.objectExists(source) || !state.getObject(source).isAtBattlefield())
            return false;
        const GameObjectId played = trigger == TriggerType::WhenYouPlayMe ? source : subject;
        if (played == kInvalidId || !state.objectExists(played)) return false;
        if (state.getObject(played).isToken()) return false;
        return playOrdinalThisTurn(state, played) == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "if I'm at a battlefield" was the trigger's condition
        // (triggerCondition): an ability that is on the chain resolves even
        // if the Heron has left the battlefield since (CR 383.2.a.1).
        auto& ps = ctx.state.player(ctx.controller);
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_reduction = 2;
        m.power_reduction = 2;
        m.next_card_only = true;
        m.this_turn_only = true;    // "the next card you play this turn" (erratum)
        m.affects_friendly_only = true;
        ps.cost_modifiers.push_back(m);
        RIFT_TRACE(ctx.events, "ASTRAL HERON: first card this turn -> next card costs [2][A][A] less");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 831;
        d.def_id = R"RB(ven-044-166)RB";
        d.name = R"RB(Astral Heron)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-044/166)RB";
        d.collector_number = 44;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bird)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 7;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play your first card each turn, if I'm at a battlefield, the next card you play this turn costs [2][A][A] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b1ce87efc9a1a7e2438c53e034a7f92caaa67ed6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_831(CardRegistry& r) {
    r.registerCard(831, std::make_unique<AstralHeron>());
}

}  // namespace riftbound
