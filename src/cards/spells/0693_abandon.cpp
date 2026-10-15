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
    // "Counter a spell. Return it to its owner's hand instead of putting it in
    // their trash. [Predict]." The spell is a target (CR 355.9.a.2, 355.10),
    // chosen as Abandon is played (CR 355.5); Predict alone does not make it
    // playable.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci) { return ci.is_spell; }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, qualifies);
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto spell = pickChainItem(ctx, "Abandon: counter a spell",
                                         chainItemChoices(ctx, qualifies));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            // The counter: the chosen spell, wherever it is on the chain.
            if (*spell != kInvalidId) counterChosenItem(ctx, *spell, ZoneType::Hand);
            // Predict 1: peek top, publish recycle/keep choice.
            if (ps.main_deck.empty()) { ri.resume_point = 2; return; }
            auto top_card = deck::takeTop(ctx.state, ctx.controller, deck::Place::Held, knowledge::only(ctx.controller));
            // "As you look at me" (Nocturne, Horrifying; D71): gone, there is
            // nothing left to recycle or keep.
            if (!ctx.executor.seenOnTopOfDeck(ctx.controller, top_card)) {
                ri.resume_point = 2;
                return;
            }
            ri.resume_data.resize(std::max<size_t>(ri.resume_data.size(), kPredictSlot + 1), 0);
            ri.resume_data[kPredictSlot] = static_cast<int32_t>(top_card.raw());
            std::vector<Intent> choices;
            Intent recycle;
            recycle.type = IntentType::MakeChoice;
            recycle.player = ctx.controller;
            recycle.chosen_objects = {top_card};
            choices.push_back(recycle);
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            keep.chosen_value = 0;         // as EffectExecutor::predict names it
            keep.choice_label = "Keep it";
            choices.push_back(keep);
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 1: recycle or keep (Abandon)", PromptKind::LookTop);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            ri.resume_point = 2;
            if (ri.resume_data.size() <= kPredictSlot) return;
            auto top_card = GameObjectId::fromRaw(ri.resume_data[kPredictSlot]);
            if (!ctx.state.objectExists(top_card)) return;
            std::string n = ctx.state.getObject(top_card).name;
            if (choice && !choice->chosen_objects.empty()) {
                // Recycled (CR 437): recycleCards puts it on the bottom and
                // announces it ("recycled_main").
                ctx.executor.recycleCards(ctx.controller, {top_card});
                // The looked-at card is the predicting player's to know (D11).
                RIFT_TRACE_PRIVATE(ctx.events, ctx.controller,
                                   "PREDICT: " + n + " -> bottom of deck (recycled)");
            } else {
                deck::putOnTop(ctx.state, ctx.controller, top_card, deck::Place::Held, knowledge::only(ctx.controller));
                RIFT_TRACE_PRIVATE(ctx.events, ctx.controller,
                                   "PREDICT: " + n + " -> top of deck (kept)");
            }
            return;
        }
        default:
            return;
        }
    }
    // resume_data slot of the looked-at card (clear of the choice helpers').
    static constexpr size_t kPredictSlot = Card::kChoiceStateSlot + 1;
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
        // No Keyword::Predict: Predict is a game action (CR 437), not a
        // keyword the card has (testing phase R12; plans/08 8.8).
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
