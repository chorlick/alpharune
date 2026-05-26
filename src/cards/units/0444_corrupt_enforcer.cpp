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

class CorruptEnforcer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIMoveToFB, TriggerType::WhenIWinCombat};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("CORRUPT ENFORCER: win combat -> draw 1");
            return;
        }
        // WhenIMoveToFB (default branch): discard 1 via the resumable
        // single-card discard choice.
        discardThenAct(ctx, 1, "Corrupt Enforcer: discard 1");
    }

private:
    // Local copy of the resumable discard helper (the deck_cards.cpp version
    // is file-local and not exported). Posts one MakeChoice per hand card;
    // resume_point 1 commits the chosen discard.
    static void discardThenAct(CardContext& ctx, int count,
                                const std::string& label) {
        if (!ctx.state.chain.resuming.has_value()) {
            // No chain context (e.g. direct test invocation): best-effort
            // discard via executor's own chooser.
            ctx.executor.discardCards(ctx.controller, count);
            return;
        }
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        auto build_choices = [&]() {
            std::vector<Intent> out;
            for (auto cid : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {cid};
                out.push_back(std::move(c));
            }
            return out;
        };
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty() || count <= 0) return;
            ri.resume_data = {count};
            ctx.executor.requestChoice(ctx.controller, build_choices(), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }
            if (!ri.resume_data.empty()) ri.resume_data[0]--;
            if (ri.resume_data.empty() || ri.resume_data[0] <= 0 || ps.hand.empty()) {
                return;
            }
            ctx.executor.requestChoice(ctx.controller, build_choices(), label);
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 444;
        d.def_id = R"RB(sfd-123-221)RB";
        d.name = R"RB(Corrupt Enforcer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-123/221)RB";
        d.collector_number = 123;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.ability_text = R"RB(When I move to a battlefield, discard 1.
When I win a combat, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c0a6bf40b629fa038299329c1d803fc9fc8e61e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_444(CardRegistry& r) {
    r.registerCard(444, std::make_unique<CorruptEnforcer>());
}

} // namespace riftbound
