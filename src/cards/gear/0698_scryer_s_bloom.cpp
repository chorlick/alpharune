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

class ScryerSBloom : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).is_exhausted = true;
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 1};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);

        // Helper: present a recycle-or-keep choice for a single peeked card.
        auto requestPredictChoice = [&](GameObjectId card_id) {
            std::vector<Intent> choices;
            Intent recycle;
            recycle.type = IntentType::MakeChoice;
            recycle.player = ctx.controller;
            recycle.chosen_objects = {card_id};
            choices.push_back(recycle);
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            choices.push_back(keep);
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 2: recycle or keep (Scryer's Bloom)");
        };
        // Helper: apply the agent's chosen recycle-or-keep for a card.
        auto applyPredictChoice = [&](GameObjectId card_id) {
            auto choice = ctx.executor.takeChoice();
            if (!ctx.state.objectExists(card_id)) return;
            if (choice && !choice->chosen_objects.empty()) {
                // Recycle to bottom.
                ctx.state.getObject(card_id).zone = ZoneType::MainDeck;
                ps.main_deck.insert(ps.main_deck.begin(), card_id);
                ctx.events.logTrace("PREDICT: recycled " +
                                     ctx.state.getObject(card_id).name);
            } else {
                // Keep on top.
                ps.main_deck.push_back(card_id);
            }
        };
        // Helper: finish — draw, XP, kill self.
        auto finish = [&]() {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.state.player(ctx.controller).xp += 1;
            if (ctx.state.objectExists(ctx.source)) {
                ctx.executor.killObject(ctx.source);
            }
            ctx.events.logTrace("SCRYER'S BLOOM: predict 2, draw 1, +1 XP, kill self");
        };

        switch (ri.resume_point) {
        case 0: {
            // Peek up to 2 cards (fewer if deck shallow — CR 436.4).
            int actual = std::min(2, static_cast<int>(ps.main_deck.size()));
            ri.resume_data.clear();
            ri.resume_data.push_back(static_cast<int32_t>(actual));
            for (int i = 0; i < actual; ++i) {
                auto cid = ps.main_deck.back();
                ps.main_deck.pop_back();
                ri.resume_data.push_back(static_cast<int32_t>(cid));
                if (ctx.state.objectExists(cid)) {
                    auto& obj = ctx.state.getObject(cid);
                    ctx.events.logTrace("  PEEKED: " + obj.name + " (id=" +
                                         std::to_string(cid) +
                                         ") — PRIVATE to " + toString(ctx.controller));
                    ctx.events.emit(CardRevealedEvent{
                        cid, obj.card_def_id, obj.owner,
                        false, ctx.controller, ZoneType::MainDeck,
                    });
                }
            }
            if (actual == 0) { finish(); return; }
            requestPredictChoice(static_cast<GameObjectId>(ri.resume_data[1]));
            ri.resume_point = 1;
            return;
        }
        case 1: {
            int n = ri.resume_data[0];
            applyPredictChoice(static_cast<GameObjectId>(ri.resume_data[1]));
            if (n >= 2) {
                requestPredictChoice(static_cast<GameObjectId>(ri.resume_data[2]));
                ri.resume_point = 2;
            } else {
                finish();
            }
            return;
        }
        case 2: {
            applyPredictChoice(static_cast<GameObjectId>(ri.resume_data[2]));
            finish();
            return;
        }
        }
    }
private:
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
