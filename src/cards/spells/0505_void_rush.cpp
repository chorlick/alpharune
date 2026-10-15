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

// "Reveal the top 2 cards of your Main Deck. You may banish one, then play it,
//  reducing its cost by [2]. Draw any you didn't banish."

class VoidRush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    static std::vector<GameObjectId> topTwo(const GameState& state, PlayerId p) {
        const auto& deck = state.player(p).main_deck;   // top = back
        std::vector<GameObjectId> out;
        for (size_t i = 0; i < 2 && i < deck.size(); ++i) out.push_back(deck[deck.size() - 1 - i]);
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 2};
        // Reveal once — not again when re-entered with an answer.
        const bool first_pass = !ctx.state.chain.resuming.has_value() ||
                                ctx.state.chain.resuming->resume_point == 0;
        // Void Hatchling: "If you would reveal cards from a deck, look at the
        // top card first." — once, before the two are revealed.
        if (first_pass) ctx.executor.lookBeforeReveal(ctx.controller);
        const auto top = topTwo(ctx.state, ctx.controller);
        if (top.empty()) return;
        if (first_pass) {
            for (auto cid : top) {
                const auto& c = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{cid, c.card_def_id, c.owner,
                                                  /*revealed_to_all=*/true, PlayerId::None,
                                                  ZoneType::MainDeck});
            }
            RIFT_TRACE(ctx.events, "VOID RUSH: revealed top " + std::to_string(top.size()) + " card(s)");
        }
        // "You may banish one, then play it": offered for the cards that can
        // be played at the reduced cost, so nothing is banished and then left
        // there unplayed.
        auto playable = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : top) {
                const auto& c = ctx.state.getObject(cid);
                if (!c.isUnit() && !c.isGear() && !c.isSpell()) continue;
                if (ctx.executor.canPlayPayingCost(ctx.controller, cid, reduced)) out.push_back(cid);
            }
            return out;
        };
        // Choices first (the callback is re-entered with each answer): whether,
        // which card, and — for a unit — where it is played.
        // At resolution: a spell's "may" (CR 355.12, 355.18; batch M1).
        const int go = confirmOptional(ctx, "Void Rush: banish one and play it",
                                       [&]() { return !playable().empty(); });
        if (go == -1) return;
        GameObjectId chosen = kInvalidId;
        std::optional<LocationId> where_to;
        if (go == 1) {
            // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
            chosen = pickUnseenCard(ctx, "Void Rush: card to banish and play", playable());
            if (chosen == kInvalidId) return;   // waiting
            if (ctx.state.getObject(chosen).isUnit()) {
                const auto where = playLocationsFor(ctx, ctx.controller, chosen);
                // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
                const int mode = pickMode(ctx, "Void Rush: where to play it",
                                          where.count(), where.labels, ~0u,
                                          std::source_location::current(), PromptKind::Location);
                if (mode < 0 || mode >= where.count()) return;   // waiting
                where_to = where.locations[mode];
            }
        }

        int to_draw = static_cast<int>(top.size());
        if (chosen != kInvalidId) {
            --to_draw;
            // banishObject takes the card out of the deck through deck_ops.
            ctx.executor.banishObject(chosen);
            if (ctx.executor.playPayingCost(ctx.controller, chosen, where_to, reduced))
                RIFT_TRACE(ctx.events, "VOID RUSH: banished and played " +
                                    ctx.state.getObject(chosen).name + ", its cost reduced by [2]");
        }
        // "Draw any you didn't banish": they are still the top cards.
        if (to_draw > 0) ctx.executor.drawCards(ctx.controller, to_draw);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 505;
        d.def_id = R"RB(sfd-188-221)RB";
        d.name = R"RB(Void Rush)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-188/221)RB";
        d.collector_number = 188;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Reveal the top 2 cards of your Main Deck. You may banish one, then play it, reducing its cost by [2]. Draw any you didn't banish.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ce606f096ae76485cd8dd3cc337629b510f0d1e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_505(CardRegistry& r) {
    r.registerCard(505, std::make_unique<VoidRush>());
}

} // namespace riftbound
