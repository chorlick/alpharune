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

class Mindsplitter : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        auto& opps = ctx.state.player(opp);
        switch (ri.resume_point) {
        case 0: {
            if (opps.hand.empty()) return;
            // CR: "They reveal their hand." Emit a private
            // CardRevealedEvent (revealed_to = controller) per card so
            // PlayerState::observed_cards updates for the controller —
            // that's the imperfect-info-observable side effect that
            // makes Mindsplitter actually informative for ML training.
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
                ctx.events.logTrace("MINDSPLITTER: revealed " + obj.name +
                                     " (id=" + std::to_string(card_id) +
                                     ") to " + std::string(toString(ctx.controller)));
            }
            // CR: the CONTROLLER chooses the card; the opponent then
            // discards it. Route the MakeChoice to ctx.controller (the
            // chooser), then applyDiscard against the opponent's hand.
            std::vector<Intent> choices;
            for (auto card_id : opps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "choose opponent's card to discard (Mindsplitter)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(opp, choice->chosen_objects[0]);
                ctx.events.logTrace("MINDSPLITTER: opponent discards 1");
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 192;
        d.def_id = R"RB(ogn-192-298)RB";
        d.name = R"RB(Mindsplitter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-192/298)RB";
        d.collector_number = 192;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, choose an opponent. They reveal their hand. Choose a card from it, and they discard that card.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8753058698cff6455da6cccd13a7fe901a164051-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_192(CardRegistry& r) {
    r.registerCard(192, std::make_unique<Mindsplitter>());
}

} // namespace riftbound
