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

class OrnnBlacksmith : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    // Top (up to) 4 cards of the deck (back = top).
    std::vector<GameObjectId> topFour(CardContext& ctx) const {
        const auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> out;
        for (auto it = ps.main_deck.rbegin();
             it != ps.main_deck.rend() && out.size() < 4; ++it)
            if (ctx.state.objectExists(*it)) out.push_back(*it);
        return out;
    }
    std::vector<GameObjectId> gearAmong(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto id : topFour(ctx))
            if (ctx.state.getObject(id).isGear()) out.push_back(id);
        return out;
    }
    // "look at the top 4 cards of your Main Deck. You may reveal a gear from
    // among them and draw it. Then recycle the rest." (As Fate Weaver 626.)
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // The look is private; it tells the controller what is there to take.
        const auto looked = topFour(ctx);
        for (size_t i = 0; i < looked.size(); ++i)
            knowledge::seenInDeck(ctx.state, looked[i], knowledge::only(ctx.controller),
                                  DeckEnd::Top, static_cast<int>(i));
        int conf = confirmOptional(ctx, "Ornn, Blacksmith: reveal a gear from the top 4 and draw it?",
                                   [&]() { return !gearAmong(ctx).empty(); });
        if (conf == -1) return;  // waiting for agent
        GameObjectId drafted = kInvalidId;
        if (conf == 1) {
            drafted = pickTarget(ctx, "Ornn, Blacksmith: choose a gear to draw", gearAmong(ctx));
            if (drafted == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;  // suspended for the choice
        }
        // Snapshot the top 4 before drawing (drawing shifts the deck).
        const std::vector<GameObjectId> original_top = topFour(ctx);
        if (drafted != kInvalidId) {
            auto& drafted_obj = ctx.state.getObject(drafted);
            // Revealed, then drawn: the opponent sees it and knows it is in hand.
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/true,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            deck::take(ctx.state, drafted, deck::Place::Held, knowledge::everyone(), /*slot_public=*/false);
            // "draw it": a draw (draws_this_turn, "When you draw").
            ctx.executor.drawHeldCard(ctx.controller, drafted, /*seen_by_all=*/true);
            ctx.events.logTrace("ORNN, BLACKSMITH: drafted gear " + drafted_obj.name);
        }
        // Recycle the rest to the bottom of the deck.
        std::vector<GameObjectId> rest;
        for (auto id : original_top) {
            if (id == drafted) continue;
            deck::take(ctx.state, id, deck::Place::Held, knowledge::only(ctx.controller), /*slot_public=*/false);
            rest.push_back(id);
        }
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 381;
        d.def_id = R"RB(sfd-058-221)RB";
        d.name = R"RB(Ornn, Blacksmith)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-058/221)RB";
        d.collector_number = 58;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ornn)RB", R"RB(Freljord)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me or when I hold, look at the top 4 cards of your Main Deck. You may reveal a gear from among them and draw it. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/77b48a28f48f26714f4bcf860379945a2d7186aa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_381(CardRegistry& r) {
    r.registerCard(381, std::make_unique<OrnnBlacksmith>());
}

} // namespace riftbound
