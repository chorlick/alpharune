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

class HardBargain : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [2]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 2}};
    }
    // "Counter a spell unless its controller pays [2]."
    // Peek-and-pop: the spell answered is the top of the chain, so that is
    // also what makes this playable — a spell further down, under an ability,
    // cannot be reached.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return !state.chain.items.empty() && state.chain.items.back().is_spell;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        // Counters the spell `target_source` if it is still the chain top.
        auto counter = [&](GameObjectId target_source, const std::string& why) {
            if (ctx.state.chain.items.empty() ||
                ctx.state.chain.items.back().source != target_source) return;
            revertCounteredPlay(ctx, ctx.state.chain.items.back());  // CR 425.1.b
            ctx.state.chain.items.pop_back();
            if (!ctx.state.objectExists(target_source)) return;
            ctx.events.logTrace("HARD BARGAIN: countered " +
                                ctx.state.getObject(target_source).name + " (" + why + ")");
            disposeCounteredCard(ctx, target_source);
        };
        switch (ri.resume_point) {
        case 0: {
            if (ctx.state.chain.items.empty()) {
                ctx.events.logTrace("HARD BARGAIN: nothing on chain to counter — no-op");
                return;
            }
            auto& top = ctx.state.chain.items.back();
            if (!top.is_spell || isUncounterable(ctx, top)) {
                ctx.events.logTrace("HARD BARGAIN: top of chain isn't a spell — no-op");
                return;
            }
            PlayerId target_controller = top.controller;
            GameObjectId target_source = top.source;
            ri.resume_data = {static_cast<int32_t>(target_source),
                              static_cast<int32_t>(target_controller)};

            // [2] is Energy: floating Energy and ready runes both pay it.
            if (payableEnergy(ctx.state, target_controller) < 2) {
                counter(target_source, std::string(toString(target_controller)) +
                                       " can't pay [2] to save it");
                ri.resume_point = 2;
                return;
            }

            // Offer the target's controller a binary choice: pay 2 to save,
            // or let it be countered. Two MakeChoice intents — chosen_objects
            // is empty for "let it die," contains the target_source for "pay."
            std::vector<Intent> choices;
            Intent pay;
            pay.type = IntentType::MakeChoice;
            pay.player = target_controller;
            pay.chosen_objects = {target_source};
            choices.push_back(pay);
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = target_controller;
            decline.chosen_value = 0;   // nameable by value, not only by position
            decline.choice_label = "Hard Bargain: don't pay";
            choices.push_back(decline);
            std::string spell_name = ctx.state.objectExists(target_source)
                ? ctx.state.getObject(target_source).name : "spell";
            ctx.executor.requestChoice(target_controller, std::move(choices),
                                        "Hard Bargain: pay 2 to save " + spell_name +
                                        "? [pay | decline]");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            ri.resume_point = 2;
            if (ri.resume_data.size() < 2) return;
            auto target_source = static_cast<GameObjectId>(ri.resume_data[0]);
            auto target_controller = static_cast<PlayerId>(ri.resume_data[1]);
            if (!ctx.state.objectExists(target_source)) return;

            bool wants_to_pay = (choice && !choice->chosen_objects.empty() &&
                                 choice->chosen_objects[0] == target_source);
            if (wants_to_pay && payEnergy(ctx, target_controller, 2)) {
                ctx.events.logTrace(std::string("HARD BARGAIN: ") +
                                     toString(target_controller) +
                                     " paid [2] to save " +
                                     ctx.state.getObject(target_source).name);
                // Spell stays on chain — no pop.
            } else {
                counter(target_source, "its controller did not pay [2]");
            }
            return;
        }
        default:
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 457;
        d.def_id = R"RB(sfd-136-221)RB";
        d.name = R"RB(Hard Bargain)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-136/221)RB";
        d.collector_number = 136;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
[Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Counter a spell unless its controller pays [2].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2b09887a6e0d2f109aefea62ca6744488794c233-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_457(CardRegistry& r) {
    r.registerCard(457, std::make_unique<HardBargain>());
}

} // namespace riftbound
