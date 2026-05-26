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

class EzrealProdigy : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "discard 1, then draw 2" — resumable discard via the executor's
        // pending-choice slot (resume_point 0 = publish, 1 = commit + draw).
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty()) {
                // Can't discard — still draw 2 (CR: "discard 1, then draw 2";
                // an empty hand means the discard does nothing).
                ctx.executor.drawCards(ctx.controller, 2);
                return;
            }
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "discard 1 (Ezreal, Prodigy)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }
            ctx.executor.drawCards(ctx.controller, 2);
            ctx.events.logTrace("EZREAL PRODIGY: discarded 1, drew 2");
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 470;
        d.def_id = R"RB(sfd-149-221)RB";
        d.name = R"RB(Ezreal, Prodigy)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-149/221)RB";
        d.collector_number = 149;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ezreal)RB", R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play me, discard 1, then draw 2.
Optional additional costs you pay cost [1] or [A] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b48d6242ceb84c3eae7109317d51efaf1e56c9c3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_470(CardRegistry& r) {
    r.registerCard(470, std::make_unique<EzrealProdigy>());
}

} // namespace riftbound
