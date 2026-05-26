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

class Sabotage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        auto& opps = ctx.state.player(opp);
        switch (ri.resume_point) {
        case 0: {
            if (opps.hand.empty()) return;
            // Reveal: emit CardRevealedEvent for every card in the
            // opponent's hand, private to the caster. The subscriber on
            // GameEngine increments observed_cards[card_def_id] for the
            // caster, which is what makes this card actually informative
            // (the caster now knows what's in the opponent's hand for
            // future decisions).
            for (auto card_id : opps.hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                const auto& obj = ctx.state.getObject(card_id);
                ctx.events.emit(CardRevealedEvent{
                    /*card=*/card_id,
                    /*card_def_id=*/obj.card_def_id,
                    /*owner=*/obj.owner,
                    /*revealed_to_all=*/false,
                    /*revealed_to=*/ctx.controller,
                    /*source_zone=*/ZoneType::Hand,
                });
                ctx.events.logTrace("SABOTAGE: revealed " + obj.name +
                                     " (id=" + std::to_string(card_id) +
                                     ") to " + std::string(toString(ctx.controller)));
            }
            // Build choice set: non-unit cards only.
            std::vector<Intent> choices;
            for (auto card_id : opps.hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                const auto& obj = ctx.state.getObject(card_id);
                if (obj.isUnit()) continue;  // CR: "non-unit card"
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            if (choices.empty()) {
                ctx.events.logTrace("SABOTAGE: opponent has no non-unit cards in "
                                     "hand; recycle skipped.");
                return;
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "recycle 1 non-unit from opponent's hand (Sabotage)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) return;
            auto card_id = choice->chosen_objects[0];
            if (!ctx.state.objectExists(card_id)) return;

            // Remove from opponent's hand, then recycle (bottom of
            // opponent's main deck). recycleCards inserts at the front
            // of the deck vector (bottom = front, top = back).
            auto& hand = opps.hand;
            auto it = std::find(hand.begin(), hand.end(), card_id);
            if (it == hand.end()) return;
            const auto& obj = ctx.state.getObject(card_id);
            ctx.events.logTrace("SABOTAGE: recycling " + obj.name + " (id=" +
                                 std::to_string(card_id) + ") to bottom of " +
                                 std::string(toString(opp)) + "'s deck");
            hand.erase(it);
            ctx.executor.recycleCards(opp, {card_id});
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 156;
        d.def_id = R"RB(ogn-156-298)RB";
        d.name = R"RB(Sabotage)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-156/298)RB";
        d.collector_number = 156;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Choose an opponent. They reveal their hand. Choose a non-unit card from it, and recycle that card.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/70f186b85fd224ee23dcf64957f0fab835d28040-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_156(CardRegistry& r) {
    r.registerCard(156, std::make_unique<Sabotage>());
}

} // namespace riftbound
