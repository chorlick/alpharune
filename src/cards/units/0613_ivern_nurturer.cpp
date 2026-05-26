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

class IvernNurturer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);

        // Peek the top 3 (top = back of vector). We do not pop yet so that
        // a confirmOptional suspend/resume doesn't lose the peeked set; the
        // pop happens after the agent decides.
        std::vector<GameObjectId> peeked;
        for (int i = 0; i < 3; ++i) {
            int idx = static_cast<int>(ps.main_deck.size()) - 1 - i;
            if (idx < 0) break;
            peeked.push_back(ps.main_deck[idx]);
        }
        if (peeked.empty()) return;

        // Is there a unit among the peeked cards?
        auto has_unit = [&]() -> bool {
            for (auto id : peeked) {
                if (!ctx.state.objectExists(id)) continue;
                if (ctx.state.getObject(id).isUnit()) return true;
            }
            return false;
        };

        // "You may reveal a unit ... and draw it." Optional.
        int conf = confirmOptional(ctx,
            "Ivern, Nurturer: reveal a unit from top 3 and draw it?", has_unit);
        if (conf == -1) return;  // waiting for agent

        // Now actually remove the peeked cards from the top of the deck.
        std::vector<GameObjectId> top3;
        for (int i = 0; i < 3 && !ps.main_deck.empty(); ++i) {
            top3.push_back(ps.main_deck.back());
            ps.main_deck.pop_back();
        }

        GameObjectId drafted = kInvalidId;
        std::vector<GameObjectId> rest;
        bool themed = false;

        if (conf == 1) {
            // Draft the first unit; rest get recycled.
            for (auto id : top3) {
                if (!ctx.state.objectExists(id)) { rest.push_back(id); continue; }
                const auto& obj = ctx.state.getObject(id);
                if (drafted == kInvalidId && obj.isUnit()) {
                    drafted = id;
                } else {
                    rest.push_back(id);
                }
            }
        } else {
            // Declined the reveal/draw — everything is recycled.
            rest = top3;
        }

        if (drafted != kInvalidId) {
            const auto& drafted_obj = ctx.state.getObject(drafted);
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            ctx.events.logTrace("IVERN NURTURER: drafted " + drafted_obj.name);
            // Move it to hand directly (already popped off the deck).
            auto& obj = ctx.state.getObject(drafted);
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(drafted);

            for (const auto& t : drafted_obj.tags) {
                if (t == "Bird" || t == "Cat" || t == "Dog" || t == "Poro") {
                    themed = true; break;
                }
            }
        }

        // Recycle the rest to the bottom of the deck.
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
        }

        // Themed reveal → [Buff] a friendly unit (+1 buff counter).
        if (themed) {
            for (auto& [oid, u] : ctx.state.objects) {
                if (!u.isUnit() || u.controller != ctx.controller) continue;
                if (!u.location.has_value()) continue;
                ctx.executor.buffUnit(oid);
                ctx.events.logTrace("IVERN NURTURER: themed draft — buff " +
                                     u.name);
                break;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 613;
        d.def_id = R"RB(unl-051-219)RB";
        d.name = R"RB(Ivern, Nurturer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-051/219)RB";
        d.collector_number = 51;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ivern)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me or when I hold, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest. Then if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a72b08dd57422d3441df88eb61c000cdb4ac688a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_613(CardRegistry& r) {
    r.registerCard(613, std::make_unique<IvernNurturer>());
}

} // namespace riftbound
