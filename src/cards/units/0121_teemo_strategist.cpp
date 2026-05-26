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

class TeemoStrategist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not defending at a battlefield

        // "choose an enemy unit here" — enemy units at this battlefield.
        auto legal = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;  // enemy only
                if (obj.battlefieldId() != my_bf) continue;       // "here"
                out.push_back(id);
            }
            return out;
        };
        GameObjectId picked = pickTarget(ctx, "Teemo, Strategist", legal());
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        // Reveal the top 5 of own Main Deck (top = back of vector).
        auto& ps = ctx.state.player(ctx.controller);
        int n = std::min(5, static_cast<int>(ps.main_deck.size()));
        std::vector<GameObjectId> revealed;
        for (int i = 0; i < n; ++i) {
            auto cid = ps.main_deck.back();
            ps.main_deck.pop_back();
            revealed.push_back(cid);
        }
        int hidden_count = 0;
        for (auto cid : revealed) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& c = ctx.state.getObject(cid);
            // Private reveal to the controller.
            ctx.events.emit(CardRevealedEvent{
                /*card=*/cid,
                /*card_def_id=*/c.card_def_id,
                /*owner=*/c.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            if (c.hasKeyword(Keyword::Hidden)) ++hidden_count;
        }
        ctx.events.logTrace("TEEMO, STRATEGIST: revealed " + std::to_string(n) +
                             " — " + std::to_string(hidden_count) + " Hidden");

        // Deal 1 for each Hidden card revealed.
        if (hidden_count > 0 && ctx.state.objectExists(picked)) {
            ctx.executor.dealDamage(picked, hidden_count, ctx.source);
            if (ctx.state.objectExists(picked) &&
                ctx.state.getObject(picked).hasLethalDamage()) {
                ctx.executor.killObject(picked);
            }
        }

        // Recycle the revealed cards (to bottom of deck).
        if (!revealed.empty()) {
            ctx.executor.recycleCards(ctx.controller, revealed);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 121;
        d.def_id = R"RB(ogn-121-298)RB";
        d.name = R"RB(Teemo, Strategist)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-121/298)RB";
        d.collector_number = 121;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB", R"RB(Teemo)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.might = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When I defend, choose an enemy unit here and reveal the top 5 cards of your Main Deck. Deal 1 to that unit for each card with [Hidden] revealed this way, then recycle the revealed cards.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b05f31bf744972983f61a9f5801b4ffd68fb9ebf-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_121(CardRegistry& r) {
    r.registerCard(121, std::make_unique<TeemoStrategist>());
}

} // namespace riftbound
