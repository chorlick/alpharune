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

class RiftHerald : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIMoveToFB, TriggerType::WhenIDie};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenIMoveToFB) {
            onMove(ctx);
        } else if (ctx.firing_trigger == TriggerType::WhenIDie) {
            onDeath(ctx);
        }
    }

private:
    // Top (up to) 3 cards of the controller's Main Deck (back = top).
    static std::vector<GameObjectId> topThree(const CardContext& ctx) {
        const auto& deck = ctx.state.player(ctx.controller).main_deck;
        std::vector<GameObjectId> out;
        for (auto it = deck.rbegin(); it != deck.rend() && out.size() < 3; ++it)
            if (ctx.state.objectExists(*it)) out.push_back(*it);
        return out;
    }
    static std::vector<GameObjectId> unitsAmong(const CardContext& ctx,
                                                const std::vector<GameObjectId>& cards) {
        std::vector<GameObjectId> out;
        for (auto id : cards)
            if (ctx.state.getObject(id).isUnit()) out.push_back(id);
        return out;
    }

    // "When I move to a battlefield, look at the top 3 cards of your Main Deck.
    // You may reveal a unit from among them and draw it. Recycle the rest."
    // The three cards stay on the deck while the controller decides; only the
    // unit they choose is revealed to the opponent.
    void onMove(CardContext& ctx) {
        const auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point == 0) {
            // "Look at": private to the controller, once.
            int depth = 0;
            for (auto cid : topThree(ctx)) {
                const auto& obj = ctx.state.getObject(cid);
                knowledge::seenInDeck(ctx.state, cid, knowledge::only(ctx.controller),
                                      DeckEnd::Top, depth++);
                ctx.events.emit(CardRevealedEvent{
                    cid, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller,
                    ZoneType::MainDeck});
            }
        }
        int conf = confirmOptional(ctx, "Rift Herald: reveal a unit from the top 3 and draw it?",
                                   [&]() { return !unitsAmong(ctx, topThree(ctx)).empty(); });
        if (conf == -1) return;  // waiting for agent

        const std::vector<GameObjectId> looked = topThree(ctx);
        GameObjectId drawn = kInvalidId;
        if (conf == 1) {
            drawn = pickTarget(ctx, "Rift Herald: choose a unit to draw",
                               unitsAmong(ctx, looked));
            if (drawn == kInvalidId && ri.has_value() && ri->resume_point == 7)
                return;  // suspended for the choice
        }
        if (looked.empty()) return;

        // Take the looked-at cards off the deck, recycle the ones not drawn,
        // then reveal the chosen unit, put it back on top and draw it, so the
        // draw is a real draw (draw count, "when you draw" triggers).
        std::vector<GameObjectId> rest;
        for (auto cid : looked) {
            deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller),
                       /*slot_public=*/false);
            if (cid != drawn) rest.push_back(cid);
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
        if (drawn != kInvalidId) {
            const auto& obj = ctx.state.getObject(drawn);
            // No EffectExecutor::lookBeforeReveal (Void Hatchling 341) here: the
            // card revealed was picked from cards already looked at, so a look at
            // the top card shows nothing new, and recycling it first changes no
            // zone — "Recycle the rest" puts it on the bottom anyway, and if it is
            // the picked card it is drawn anyway.
            ctx.events.emit(CardRevealedEvent{
                drawn, obj.card_def_id, obj.owner,
                /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
                ZoneType::MainDeck});
            deck::putOnTop(ctx.state, ctx.controller, drawn, deck::Place::Held,
                           knowledge::everyone());
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("RIFT HERALD: revealed and drew unit " + obj.name);
        }
    }

    // [Deathknell]: "Play a unit from your hand to your base, ignoring its
    // Energy cost. (You must still pay its Power cost.)" Only units whose
    // Power cost can be paid are offered.
    std::vector<GameObjectId> playableHandUnits(CardContext& ctx) const {
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        std::vector<GameObjectId> out;
        for (auto cid : ctx.state.player(ctx.controller).hand) {
            if (!ctx.state.objectExists(cid) || !ctx.state.getObject(cid).isUnit()) continue;
            if (ctx.executor.canPlayPayingCost(ctx.controller, cid, power_only))
                out.push_back(cid);
        }
        return out;
    }
    void onDeath(CardContext& ctx) {
        GameObjectId picked = pickTarget(ctx, "Rift Herald (deathknell: unit from hand)",
                                         playableHandUnits(ctx));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;  // none / waiting
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        // playPayingCost takes the unit out of the hand once it is paid for.
        if (!ctx.executor.playPayingCost(ctx.controller, picked,
                                         LocationId{BaseLocation{ctx.controller}},
                                         power_only)) {
            ctx.events.logTrace("RIFT HERALD: can't pay that unit's Power cost");
            return;
        }
        ctx.events.logTrace("RIFT HERALD: deathknell -> played a unit from hand to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 741;
        d.def_id = R"RB(unl-179-219)RB";
        d.name = R"RB(Rift Herald)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-179/219)RB";
        d.collector_number = 179;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(The Void)RB"};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB(When I move to a battlefield, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest.
[Deathknell][>] Play a unit from your hand to your base, ignoring its Energy cost. (When I die, get the effect. You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4da94f3af92222bebe7be49197ef8d840772d73d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_741(CardRegistry& r) {
    r.registerCard(741, std::make_unique<RiftHerald>());
}

} // namespace riftbound
