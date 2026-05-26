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

class Abandon : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Per CR 355.9.a.2 + 355.10: "Counter a spell" is a target
    // requirement on a Chain object. Abandon needs a spell on chain
    // to be playable; the Predict 1 side-effect doesn't make it
    // playable on its own. Earlier session left this as "always
    // playable" which is wrong per CR — corrected after user CR review.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            // Counter step. items.back() is the spell below Abandon
            // (Abandon itself is now in `resuming`, not on `items`).
            if (!ctx.state.chain.items.empty()) {
                auto& top = ctx.state.chain.items.back();
                if (top.is_spell) {
                    auto countered_source = top.source;
                    revertCounteredPlay(ctx, top);  // CR 425.1.b
                    ctx.state.chain.items.pop_back();
                    if (ctx.state.objectExists(countered_source)) {
                        auto& obj = ctx.state.getObject(countered_source);
                        ctx.events.logTrace("COUNTER: " + obj.name +
                                            " countered by Abandon -> hand");
                        obj.zone = ZoneType::Hand;
                        obj.location = std::nullopt;
                        ctx.state.player(obj.owner).hand.push_back(countered_source);
                    }
                }
            }

            // Predict 1: peek top, publish recycle/keep choice.
            if (ps.main_deck.empty()) return;
            auto top_card = ps.main_deck.back();
            ps.main_deck.pop_back();
            ri.resume_data = {static_cast<int32_t>(top_card)};

            std::vector<Intent> choices;
            Intent recycle;
            recycle.type = IntentType::MakeChoice;
            recycle.player = ctx.controller;
            recycle.chosen_objects = {top_card};
            choices.push_back(recycle);
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            choices.push_back(keep);

            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 1: recycle or keep (Abandon)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            auto top_card = static_cast<GameObjectId>(ri.resume_data[0]);
            if (!ctx.state.objectExists(top_card)) return;

            std::string n = ctx.state.getObject(top_card).name;
            if (choice && !choice->chosen_objects.empty()) {
                ctx.state.getObject(top_card).zone = ZoneType::MainDeck;
                ps.main_deck.insert(ps.main_deck.begin(), top_card);
                ctx.events.logTrace("PREDICT: " + n +
                                     " -> bottom of deck (recycled)");
            } else {
                // Put back on top. Was previously silent — surfaced after
                // user noticed the keep-case wasn't visible in the replay.
                ps.main_deck.push_back(top_card);
                ctx.events.logTrace("PREDICT: " + n + " -> top of deck (kept)");
            }
            return;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 693;
        d.def_id = R"RB(unl-131-219)RB";
        d.name = R"RB(Abandon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-131/219)RB";
        d.collector_number = 131;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Predict);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter a spell. Return it to its owner's hand instead of putting it in their trash.
[Predict]. (Look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/89929cfa4417c99576477793529c6808af145919-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_693(CardRegistry& r) {
    r.registerCard(693, std::make_unique<Abandon>());
}

} // namespace riftbound
