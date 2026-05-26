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

class StackedDeck : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            int actual = std::min(3, static_cast<int>(ps.main_deck.size()));
            ri.resume_data.clear();
            ri.resume_data.push_back(static_cast<int32_t>(actual));
            for (int i = 0; i < actual; ++i) {
                auto cid = ps.main_deck.back();
                ps.main_deck.pop_back();
                ri.resume_data.push_back(static_cast<int32_t>(cid));
                if (ctx.state.objectExists(cid)) {
                    auto& obj = ctx.state.getObject(cid);
                    ctx.events.logTrace("  PEEKED: " + obj.name + " (id=" +
                                         std::to_string(cid) +
                                         ") — PRIVATE to " + toString(ctx.controller));
                    ctx.events.emit(CardRevealedEvent{
                        cid, obj.card_def_id, obj.owner,
                        false, ctx.controller, ZoneType::MainDeck,
                    });
                }
            }
            if (actual == 0) return;  // empty deck — no-op

            // Publish a 1-of-N "pick the keeper" choice. Each Intent's
            // chosen_objects = [the card to put into hand].
            std::vector<Intent> choices;
            std::string label = "Stacked Deck: pick 1 of " +
                                std::to_string(actual) + " to draw [";
            for (int i = 1; i <= actual; ++i) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {static_cast<GameObjectId>(ri.resume_data[i])};
                choices.push_back(c);
                if (i > 1) label += " | ";
                if (ctx.state.objectExists(static_cast<GameObjectId>(ri.resume_data[i]))) {
                    label += ctx.state.getObject(static_cast<GameObjectId>(ri.resume_data[i])).name;
                }
            }
            label += "]";
            ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            int n = ri.resume_data.empty() ? 0 : ri.resume_data[0];
            GameObjectId keeper = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) {
                keeper = choice->chosen_objects[0];
            }
            // Recycle the non-chosen peeked cards to bottom; chosen → hand.
            // We process in original peek order so the recycle order is
            // deterministic (top peeked card recycles first, ends up
            // deeper in the deck after subsequent inserts).
            for (int i = 1; i <= n; ++i) {
                auto cid = static_cast<GameObjectId>(ri.resume_data[i]);
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (cid == keeper) {
                    obj.zone = ZoneType::Hand;
                    obj.location = std::nullopt;
                    ps.hand.push_back(cid);
                    ctx.events.logTrace("STACKED DECK: drew " + obj.name);
                } else {
                    obj.zone = ZoneType::MainDeck;
                    obj.location = std::nullopt;
                    ps.main_deck.insert(ps.main_deck.begin(), cid);
                    ctx.events.logTrace("STACKED DECK: recycled " + obj.name);
                }
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 183;
        d.def_id = R"RB(ogn-183-298)RB";
        d.name = R"RB(Stacked Deck)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-183/298)RB";
        d.collector_number = 183;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Look at the top 3 cards of your Main Deck. Put 1 into your hand and recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fdcb22cb620a4c1c37920d6b744edb615647cbd4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_183(CardRegistry& r) {
    r.registerCard(183, std::make_unique<StackedDeck>());
}

} // namespace riftbound
