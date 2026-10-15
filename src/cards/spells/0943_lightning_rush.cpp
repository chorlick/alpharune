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

class LightningRush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Look at the top 3 cards of your Main Deck. You may choose a card from
    //  among them and draw it. Put the rest into your trash."
    // Resumable: one pick over the looked-at cards plus "Draw none".
    // resume_data = the looked-at card ids.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        auto& ps = ctx.state.player(ctx.controller);
        if (ri.resume_point == 0) {
            ri.resume_data.clear();
            // Top of the Main Deck is the back of the vector.
            for (auto it = ps.main_deck.rbegin();
                 it != ps.main_deck.rend() && ri.resume_data.size() < 3; ++it)
                ri.resume_data.push_back(static_cast<int32_t>((*it).raw()));
            if (ri.resume_data.empty()) { ri.resume_point = 2; return; }
            // A private look at the top three.
            for (size_t i = 0; i < ri.resume_data.size(); ++i) {
                knowledge::shown(ctx.state, GameObjectId::fromRaw(ri.resume_data[i]),
                                 knowledge::only(ctx.controller));
            }
            // "As you look at me" replacements (Nocturne, Horrifying; D71): a
            // card that leaves as it is looked at is not one of the three.
            {
                std::vector<int32_t> kept;
                for (auto v : ri.resume_data)
                    if (ctx.executor.seenOnTopOfDeck(ctx.controller, GameObjectId::fromRaw(v)))
                        kept.push_back(v);
                ri.resume_data = kept;
                if (ri.resume_data.empty()) { ri.resume_point = 2; return; }
            }
            std::vector<Intent> opts;
            for (auto v : ri.resume_data)
                opts.push_back(objChoice(ctx.controller, GameObjectId::fromRaw(v)));
            opts.push_back(valChoice(ctx.controller, 0, "Draw none"));
            // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
            ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                       "Lightning Rush: draw one of the top 3 cards");
            ri.resume_point = 1;
            return;
        }
        if (ri.resume_point != 1) return;
        ri.resume_point = 2;
        GameObjectId picked = takenObject(ctx);
        // All of them leave the deck; the chosen one is drawn, THEN the rest
        // go to the trash, in the order printed (testing phase R6). The card
        // is already held, so it is drawn with drawHeldCard (a draw: "when you
        // draw" sees it), not put back on top and drawn again.
        std::vector<GameObjectId> rest;
        GameObjectId drawn = kInvalidId;
        for (auto v : ri.resume_data) {
            auto id = GameObjectId::fromRaw(v);
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), id);
            if (it == ps.main_deck.end()) continue;   // no longer in the deck
            deck::take(ctx.state, id, id == picked ? deck::Place::Held : deck::Place::Public,
                       knowledge::only(ctx.controller), /*slot_public=*/false);
            if (id == picked) drawn = id;
            else rest.push_back(id);
        }
        if (drawn != kInvalidId)
            ctx.executor.drawHeldCard(ctx.controller, drawn, /*seen_by_all=*/false);
        for (auto id : rest) {
            // "Put the rest into your trash." (From the Main Deck, so not a Burn
            // and not redirected by "would go to your trash from anywhere other
            // than your Main Deck".)
            auto& card = ctx.state.getObject(id);
            card.zone = ZoneType::Trash;
            card.location = std::nullopt;
            ps.trash.push_back(id);
            RIFT_TRACE(ctx.events, "LIGHTNING RUSH: " + card.name + " -> trash");
        }
    }
    // "[Flow] [2][A]"
    FlowCost flowCost() const override { return flow(2, 1, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 943;
        d.def_id = R"RB(ven-156-166)RB";
        d.name = R"RB(Lightning Rush)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-156/166)RB";
        d.collector_number = 156;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Order, Domain::Chaos};
        d.tags = {R"RB(Kennen)RB"};
        d.energy_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Look at the top 3 cards of your Main Deck. You may choose a card from among them and draw it. Put the rest into your trash.
[Flow] [2][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/22375e0d2d1a49cb0eb7e31aebf3e73a2e0bba55-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_943(CardRegistry& r) {
    r.registerCard(943, std::make_unique<LightningRush>());
}

}  // namespace riftbound
