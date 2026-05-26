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
            std::string label = "Lunar Boon: discard 1 then draw 2 [";
            bool first = true;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
                if (!first) label += " | ";
                first = false;
                if (ctx.state.objectExists(card_id)) {
                    label += ctx.state.getObject(card_id).name;
                }
            }
            label += "]";
            ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                auto cid = choice->chosen_objects[0];
                std::string n = ctx.state.objectExists(cid)
                    ? ctx.state.getObject(cid).name : "card";
                ctx.events.logTrace("LUNAR BOON: discarded " + n);
                ctx.executor.applyDiscard(ctx.controller, cid);
            }
            // Wrap drawCards with a per-spell prefix so V&V can grep
            // "LUNAR BOON: drew" for the cards landed by THIS spell
            // (vs the generic "DREW: ..." that fires for all draws).
            auto& ps = ctx.state.player(ctx.controller);
            size_t before = ps.hand.size();
            ctx.executor.drawCards(ctx.controller, 2);
            for (size_t i = before; i < ps.hand.size(); ++i) {
                auto did = ps.hand[i];
                std::string dn = ctx.state.objectExists(did)
                    ? ctx.state.getObject(did).name : "card";
                ctx.events.logTrace("LUNAR BOON: drew " + dn +
                                     " — PRIVATE to " + toString(ctx.controller));
            }
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
