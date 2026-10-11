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

// "[Repeat] [2] ... Look at the top 3 cards of your Main Deck. You may reveal a
//  unit from among them and draw it. Recycle the rest." The [Repeat] cost loop
//  is engine-handled (resolves this onResolve once per repeat).

class DoubleTrouble : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [2]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 2}};
    }
    static std::vector<GameObjectId> topThree(const GameState& state, PlayerId p) {
        const auto& deck = state.player(p).main_deck;   // top = back
        std::vector<GameObjectId> out;
        for (size_t i = 0; i < 3 && i < deck.size(); ++i) out.push_back(deck[deck.size() - 1 - i]);
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto top = topThree(ctx.state, ctx.controller);
        if (top.empty()) return;
        // Look once — not again when re-entered with an answer.
        const bool first_pass = !ctx.state.chain.resuming.has_value() ||
                                ctx.state.chain.resuming->resume_point == 0;
        std::vector<GameObjectId> units;
        int rank = 0;
        for (auto cid : top) {
            const auto& c = ctx.state.getObject(cid);
            if (first_pass) {
                knowledge::seenInDeck(ctx.state, cid, knowledge::only(ctx.controller),
                                      DeckEnd::Top, rank);
                ctx.events.emit(CardRevealedEvent{cid, c.card_def_id, c.owner,
                                                  /*revealed_to_all=*/false, ctx.controller,
                                                  ZoneType::MainDeck});
            }
            ++rank;
            if (c.isUnit()) units.push_back(cid);
        }
        // "You may reveal a unit from among them and draw it": whether, and
        // which one. Both answers come before any card moves.
        const int go = confirmOptional(ctx, "Double Trouble: reveal a unit and draw it",
                                       [&]() { return !units.empty(); });
        if (go == -1) return;
        GameObjectId drawn = kInvalidId;
        if (go == 1) {
            drawn = pickTarget(ctx, "Double Trouble: unit to draw", units);
            if (drawn == kInvalidId) return;   // waiting
        }

        std::vector<GameObjectId> rest;
        for (auto cid : top) {
            deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(ctx.controller),
                       /*slot_public=*/false);
            if (cid != drawn) rest.push_back(cid);
        }
        if (drawn != kInvalidId) {
            // No EffectExecutor::lookBeforeReveal (Void Hatchling 341) here: the
            // card revealed was picked from cards already looked at, so a look at
            // the top card shows nothing new, and recycling it first changes no
            // zone — "Recycle the rest" puts it on the bottom anyway, and if it is
            // the picked card it is drawn anyway.
            // Revealed, then drawn as a draw (so "when you draw" sees it).
            const auto& d = ctx.state.getObject(drawn);
            ctx.events.emit(CardRevealedEvent{drawn, d.card_def_id, d.owner,
                                              /*revealed_to_all=*/true, PlayerId::None,
                                              ZoneType::MainDeck});
            deck::putOnTop(ctx.state, ctx.controller, drawn, deck::Place::Held,
                           knowledge::everyone());
            ctx.executor.drawCards(ctx.controller, 1);
            RIFT_TRACE(ctx.events, "DOUBLE TROUBLE: drew unit " + ctx.state.getObject(drawn).name);
        }
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
            RIFT_TRACE(ctx.events, "DOUBLE TROUBLE: recycled " +
                                std::to_string(rest.size()) + " card(s)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 594;
        d.def_id = R"RB(unl-032-219)RB";
        d.name = R"RB(Double Trouble)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-032/219)RB";
        d.collector_number = 32;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/64e977213f50471ad7b6e8664488fb9017693f71-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_594(CardRegistry& r) {
    r.registerCard(594, std::make_unique<DoubleTrouble>());
}

} // namespace riftbound
