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
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Look at the top 3 (top = back of vector).
        std::vector<GameObjectId> top3;
        for (int i = (int)ps.main_deck.size() - 1;
             i >= 0 && (int)top3.size() < 3; --i) {
            top3.push_back(ps.main_deck[i]);
        }
        if (top3.empty()) return;

        // Reveal + draw a unit from among them (first unit found).
        GameObjectId drawn_unit = kInvalidId;
        for (auto cid : top3) {
            if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isUnit()) {
                drawn_unit = cid;
                break;
            }
        }
        if (drawn_unit != kInvalidId) {
            auto& deck = ps.main_deck;
            deck.erase(std::remove(deck.begin(), deck.end(), drawn_unit), deck.end());
            auto& obj = ctx.state.getObject(drawn_unit);
            obj.zone = ZoneType::Hand;
            obj.location = std::nullopt;
            ps.hand.push_back(drawn_unit);
            ctx.events.logTrace("DOUBLE TROUBLE: drew unit " + obj.name);
        }

        // Recycle the rest (the remaining looked-at cards) to bottom of deck.
        std::vector<GameObjectId> rest;
        for (auto cid : top3) {
            if (cid == drawn_unit) continue;
            auto& deck = ps.main_deck;
            auto it = std::find(deck.begin(), deck.end(), cid);
            if (it != deck.end()) {
                deck.erase(it);
                rest.push_back(cid);
            }
        }
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("DOUBLE TROUBLE: recycled " +
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
