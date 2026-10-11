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

class IvernNurturer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // The top 3 (top = back of the vector). Nothing is taken off the deck
        // until every answer is in: the callback is re-entered after each one.
        std::vector<GameObjectId> top3;
        for (auto it = ps.main_deck.rbegin(); it != ps.main_deck.rend() && top3.size() < 3; ++it)
            top3.push_back(*it);
        if (top3.empty()) return;
        std::vector<GameObjectId> units;
        for (auto id : top3)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).isUnit())
                units.push_back(id);
        // "You may reveal a unit from among them and draw it." — which one, or
        // none. Asked as this resolves: the cards are only looked at now.
        const auto answer = selectObject(ctx, "Ivern, Nurturer: reveal a unit and draw it",
                                         units, 7, /*may_decline=*/true);
        if (!answer.has_value()) return;       // waiting for the agent
        GameObjectId drafted = *answer;
        if (std::find(units.begin(), units.end(), drafted) == units.end()) drafted = kInvalidId;

        bool themed = false;
        if (drafted != kInvalidId)
            for (const auto& t : ctx.state.getObject(drafted).tags)
                if (t == "Bird" || t == "Cat" || t == "Dog" || t == "Poro") themed = true;

        for (size_t i = 0; i < top3.size(); ++i)
            deck::takeTop(ctx.state, ctx.controller, deck::Place::Held,
                          knowledge::only(ctx.controller));
        std::vector<GameObjectId> rest;
        for (auto id : top3)
            if (id != drafted) rest.push_back(id);
        if (drafted != kInvalidId) {
            auto& obj = ctx.state.getObject(drafted);
            // No EffectExecutor::lookBeforeReveal (Void Hatchling 341) here: the
            // card revealed was picked from cards already looked at, so a look at
            // the top card shows nothing new, and recycling it first changes no
            // zone — "Recycle the rest" puts it on the bottom anyway, and if it is
            // the picked card it is drawn anyway.
            // Only the drawn card is revealed; the rest were just looked at.
            ctx.events.emit(CardRevealedEvent{
                drafted, obj.card_def_id, obj.owner,
                /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
                ZoneType::MainDeck});
            knowledge::seen(ctx.state, drafted, deck::Place::Held, knowledge::everyone());
            // "draw it": a draw (draws_this_turn, "When you draw").
            ctx.executor.drawHeldCard(ctx.controller, drafted, /*seen_by_all=*/true);
            RIFT_TRACE(ctx.events, "IVERN NURTURER: revealed and drew " + obj.name);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        // "Then if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff] a
        // friendly unit." — a reflexive trigger (CR 388): it goes on the chain
        // as an item of its own, and the unit is chosen as that item is
        // finalized.
        if (themed) ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0);
    }
    bool hasReflexiveTrigger() const override { return true; }
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& /*data*/) override {
        std::vector<GameObjectId> friendly;
        for (auto& [oid, u] : ctx.state.objects)
            if (u.isUnit() && u.controller == ctx.controller && u.location.has_value())
                friendly.push_back(oid);
        std::sort(friendly.begin(), friendly.end());
        const GameObjectId to_buff =
            pickTarget(ctx, "Ivern, Nurturer: buff a friendly unit", friendly);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 388.4)
        if (to_buff == kInvalidId || !ctx.state.objectExists(to_buff)) return;
        ctx.executor.buffUnit(to_buff);
        RIFT_TRACE(ctx.events, "IVERN NURTURER: themed reveal -> buff " +
                            ctx.state.getObject(to_buff).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 613;
        d.def_id = R"RB(unl-051-219)RB";
        d.name = R"RB(Ivern, Nurturer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-051/219)RB";
        d.collector_number = 51;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me or when I hold, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest. Then if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a72b08dd57422d3441df88eb61c000cdb4ac688a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_613(CardRegistry& r) {
    r.registerCard(613, std::make_unique<IvernNurturer>());
}

} // namespace riftbound
