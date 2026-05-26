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

class FateWeaver : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, look at the top 4 cards of your Main Deck. You may
    //  reveal a spell with Energy cost [4] or more from among them and draw it.
    //  Recycle the rest."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }

    // Top (up to) 4 cards of the deck (back = top).
    std::vector<GameObjectId> topFour(CardContext& ctx) const {
        const auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> out;
        for (auto it = ps.main_deck.rbegin();
             it != ps.main_deck.rend() && out.size() < 4; ++it) {
            if (ctx.state.objectExists(*it)) out.push_back(*it);
        }
        return out;
    }
    std::vector<GameObjectId> eligibleSpells(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        for (auto id : topFour(ctx)) {
            const auto& obj = ctx.state.getObject(id);
            if (!obj.isSpell()) continue;
            if (ctx.executor.cardDB().get(obj.card_def_id).energy_cost >= 4) out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int conf = confirmOptional(ctx, "Fate Weaver: reveal a spell ([4]+) from the top 4 and draw it?",
                                   [&]() { return !eligibleSpells(ctx).empty(); });
        if (conf == -1) return;  // waiting for agent

        // Snapshot the original top-4 BEFORE we draw (drawing shifts the deck).
        std::vector<GameObjectId> original_top = topFour(ctx);

        GameObjectId drawn = kInvalidId;
        if (conf == 1) {
            drawn = pickTarget(ctx, "Fate Weaver: choose a spell ([4]+) to draw",
                               eligibleSpells(ctx));
            if (drawn == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;  // suspended for the choice
            }
            if (drawn != kInvalidId && ctx.state.objectExists(drawn)) {
                auto& ps = ctx.state.player(ctx.controller);
                auto& obj = ctx.state.getObject(drawn);
                ctx.events.emit(CardRevealedEvent{
                    drawn, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
                    ZoneType::MainDeck});
                auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), drawn);
                if (it != ps.main_deck.end()) ps.main_deck.erase(it);
                obj.zone = ZoneType::Hand;
                ps.hand.push_back(drawn);
                ctx.events.logTrace("FATE WEAVER: revealed spell " + obj.name + " -> drew it");
            }
        }

        // "Recycle the rest." — recycle the original top-4 minus the drawn card.
        // recycleCards inserts at the deck bottom WITHOUT removing the card from
        // its current position, so remove each from the deck first (mirrors the
        // pop-then-recycle pattern used by Apprentice Smith).
        std::vector<GameObjectId> rest;
        auto& ps = ctx.state.player(ctx.controller);
        for (auto id : original_top) {
            if (id == drawn) continue;
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), id);
            if (it != ps.main_deck.end()) ps.main_deck.erase(it);
            rest.push_back(id);
        }
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("FATE WEAVER: recycled the rest of the top 4");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 626;
        d.def_id = R"RB(unl-064-219)RB";
        d.name = R"RB(Fate Weaver)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-064/219)RB";
        d.collector_number = 64;
        d.artist = R"RB(Valentin Gloaguen)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.ability_text = R"RB(When you play me, look at the top 4 cards of your Main Deck. You may reveal a spell with Energy cost [4] or more from among them and draw it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab713d332b48a7526585b0be3718fea9e9f93622-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_626(CardRegistry& r) {
    r.registerCard(626, std::make_unique<FateWeaver>());
}

} // namespace riftbound
