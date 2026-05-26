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
    // "When I move to a battlefield, look at the top 3 cards of your Main Deck.
    // You may reveal a unit from among them and draw it. Recycle the rest."
    // Agent-choice of WHICH unit approximated: draw the first revealed unit.
    void onMove(CardContext& ctx) {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> looked;
        for (int i = 0; i < 3 && !ps.main_deck.empty(); ++i) {
            looked.push_back(ps.main_deck.back());
            ps.main_deck.pop_back();
        }
        if (looked.empty()) return;
        GameObjectId drawn = kInvalidId;
        for (auto cid : looked) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            ctx.events.emit(CardRevealedEvent{
                cid, obj.card_def_id, obj.owner,
                /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
                ZoneType::MainDeck});
            if (drawn == kInvalidId && obj.isUnit()) {
                obj.zone = ZoneType::Hand;
                ps.hand.push_back(cid);
                drawn = cid;
                ctx.events.logTrace("RIFT HERALD: drew unit " + obj.name);
            }
        }
        // Recycle the rest (everything not drawn).
        std::vector<GameObjectId> rest;
        for (auto cid : looked) if (cid != drawn) rest.push_back(cid);
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
    }

    // [Deathknell]: "Play a unit from your hand to your base, ignoring its
    // Energy cost. (You must still pay its Power cost.)" Power-cost payment is
    // approximated as free (no structured per-effect power-payment hook).
    void onDeath(CardContext& ctx) {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> hand_units;
        for (auto cid : ps.hand) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) hand_units.push_back(cid);
        }
        if (hand_units.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Rift Herald (deathknell: unit from hand)",
                                          hand_units);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.playIgnoringCost(ctx.controller, picked,
                                      LocationId{BaseLocation{ctx.controller}});
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
