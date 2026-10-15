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

class LunarBoon : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty()) {
                // No discard possible — fall through to the draw.
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
            // The prompt's header is read by both players (the DECISION and
            // CHOICE-REQUEST lines): it names no card of the hand (testing
            // phase R1; D11).
            // At resolution: the discard is chosen from a hidden hand (CR 355.10.a; batch M1).
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "Lunar Boon: discard 1, then draw 2",
                                       PromptKind::Discard);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                auto cid = choice->chosen_objects[0];
                std::string n = ctx.state.objectExists(cid)
                    ? ctx.state.getObject(cid).name.get() : "card";
                RIFT_TRACE(ctx.events, "LUNAR BOON: discarded " + n);
                ctx.executor.applyDiscard(ctx.controller, cid);
            }
            // Wrap drawCards with a per-spell prefix so V&V can grep
            // "LUNAR BOON: drew" for the cards landed by THIS spell
            // (vs the generic "DREW: ..." that fires for all draws). A drawn
            // card is its controller's to know: the line is theirs alone
            // (testing phase R1).
            auto& ps = ctx.state.player(ctx.controller);
            size_t before = ps.hand.size();
            ctx.executor.drawCards(ctx.controller, 2);
            for (size_t i = before; i < ps.hand.size(); ++i) {
                auto did = ps.hand[i];
                RIFT_TRACE_PRIVATE(ctx.events, ctx.controller, "LUNAR BOON: drew " +
                    (ctx.state.objectExists(did) ? ctx.state.getObject(did).name.get()
                                                 : std::string("card")));
            }
            RIFT_TRACE(ctx.events, "LUNAR BOON: drew " +
                                std::to_string(ps.hand.size() - before) + " card(s)");
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 687;
        d.def_id = R"RB(unl-125-219)RB";
        d.name = R"RB(Lunar Boon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-125/219)RB";
        d.collector_number = 125;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Discard 1, then draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7dc31cec93a355ba52b8cf2ca0b232a0edd57586-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_687(CardRegistry& r) {
    r.registerCard(687, std::make_unique<LunarBoon>());
}

} // namespace riftbound
