#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "engine/deck_ops.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class ScryerSBloom : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "This enters exhausted." (read by GameEngine::resolvePermanent)
    bool entersExhausted() const override { return true; }

    // "Kill this, [1], [E]: [Predict 2], then draw 1. Gain 1 XP."
    // The whole cost — kill this, [1], [E] — is declared and paid by the
    // engine as the ability is activated (ActivationCost::kill_self).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 1, .kill_self = true};
    }
    // Predict 2, by hand so the draw can follow it: look at the top two cards,
    // recycle any of them, put the rest back in any order (CR 436). One
    // recycle-or-keep prompt per card, then — if both are kept — which one
    // goes on top. resume_data = [count, card, card, recycle?, recycle?].
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;

        auto card_at = [&](int i) { return GameObjectId::fromRaw(ri.resume_data[1 + i]); };
        auto ask_recycle = [&](int i) {
            std::vector<Intent> choices;
            choices.push_back(objChoice(ctx.controller, card_at(i)));   // recycle it
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            keep.chosen_value = 0;         // as EffectExecutor::predict names it
            keep.choice_label = "Keep it";
            choices.push_back(keep);                                    // keep it
            // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 2: recycle or keep (Scryer's Bloom)");
        };
        auto take_recycle = [&](int i) {
            auto choice = ctx.executor.takeChoice();
            ri.resume_data[3 + i] = (choice && !choice->chosen_objects.empty()) ? 1 : 0;
        };
        // Put the looked-at cards back (`top` = the kept card that ends up on
        // top), then the rest of the effect.
        auto finish = [&](GameObjectId top) {
            const int n = ri.resume_data[0];
            GameObjectId recycled = kInvalidId;
            std::vector<GameObjectId> kept;
            for (int i = 0; i < n; ++i) {
                const GameObjectId cid = card_at(i);
                if (!ctx.state.objectExists(cid)) continue;
                if (ri.resume_data[3 + i] == 1) {
                    deck::putOnBottom(ctx.state, ctx.controller, cid, deck::Place::Held,
                                      knowledge::only(ctx.controller));
                    RIFT_TRACE_PRIVATE(ctx.events, ctx.controller,
                        "PREDICT: recycled " + ctx.state.getObject(cid).name);
                    recycled = cid;
                } else {
                    kept.push_back(cid);
                }
            }
            // The card that goes on top is put back last.
            for (bool last : {false, true})
                for (auto cid : kept)
                    if ((cid == top) == last)
                        deck::putOnTop(ctx.state, ctx.controller, cid, deck::Place::Held,
                                       knowledge::only(ctx.controller));
            // "When you recycle one or more cards to your Main Deck" (once).
            if (recycled != kInvalidId)
                ctx.events.emit(ObjectStateChangedEvent{recycled, "recycled_main"});
            ri.resume_point = kDone;
            ctx.executor.drawCards(ctx.controller, 1);
            gainXp(ctx, ctx.controller, 1);
            RIFT_TRACE(ctx.events, "SCRYER'S BLOOM: predict 2, draw 1, +1 XP");
        };
        // After the keep/recycle answers: ask the order if both were kept.
        auto order_or_finish = [&]() {
            const int n = ri.resume_data[0];
            if (n == 2 && ri.resume_data[3] == 0 && ri.resume_data[4] == 0) {
                std::vector<Intent> choices{objChoice(ctx.controller, card_at(0)),
                                            objChoice(ctx.controller, card_at(1))};
                // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
                ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                            "predict 2: which card goes on top? (Scryer's Bloom)");
                ri.resume_point = kAskOrder;
                return;
            }
            finish(kInvalidId);   // at most one card is kept: no order to choose
        };

        switch (ri.resume_point) {
        case kStart: {
            auto& ps = ctx.state.player(ctx.controller);
            // Look at up to 2 cards (fewer if the deck is shallow — CR 436.4).
            const int actual = std::min(2, static_cast<int>(ps.main_deck.size()));
            ri.resume_data.assign(5, 0);
            ri.resume_data[0] = actual;
            for (int i = 0; i < actual; ++i) {
                auto cid = deck::takeTop(ctx.state, ctx.controller, deck::Place::Held, knowledge::only(ctx.controller));
                ri.resume_data[1 + i] = static_cast<int32_t>(cid.raw());
                if (ctx.state.objectExists(cid)) {
                    auto& obj = ctx.state.getObject(cid);
                    RIFT_TRACE_PRIVATE(ctx.events, ctx.controller,
                                         "  PEEKED: " + obj.name + " (id=" +
                                         toString(cid) +
                                         ") — PRIVATE to " + toString(ctx.controller));
                    ctx.events.emit(CardRevealedEvent{
                        cid, obj.card_def_id, obj.owner,
                        false, ctx.controller, ZoneType::MainDeck,
                    });
                }
            }
            if (actual == 0) { finish(kInvalidId); return; }
            ask_recycle(0);
            ri.resume_point = kAskFirst;
            return;
        }
        case kAskFirst:
            take_recycle(0);
            if (ri.resume_data[0] >= 2) {
                ask_recycle(1);
                ri.resume_point = kAskSecond;
            } else {
                order_or_finish();
            }
            return;
        case kAskSecond:
            take_recycle(1);
            order_or_finish();
            return;
        case kAskOrder: {
            GameObjectId top = takenObject(ctx);
            if (top != card_at(0) && top != card_at(1)) top = card_at(0);
            finish(top);
            return;
        }
        }
    }
private:
    // Clear of the choice helpers' resume_point values (1..13).
    static constexpr int kStart = 0;
    static constexpr int kAskFirst = 20;
    static constexpr int kAskSecond = 21;
    static constexpr int kAskOrder = 22;
    static constexpr int kDone = 23;
    const CardDef def_ = [] {
        CardDef d;
        d.id = 698;
        d.def_id = R"RB(unl-136-219)RB";
        d.name = R"RB(Scryer's Bloom)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-136/219)RB";
        d.collector_number = 136;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB(This enters exhausted.
Kill this, [1], [E]: [Predict 2], then draw 1. Gain 1 XP. (To Predict 2, look at the top two cards of your Main Deck. Recycle any of them and put the rest back in any order.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b10a544d24d3ae3e177d4791743bb2cfb742abcc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_698(CardRegistry& r) {
    r.registerCard(698, std::make_unique<ScryerSBloom>());
}

} // namespace riftbound
