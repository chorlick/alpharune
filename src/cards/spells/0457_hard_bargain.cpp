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
    // "Counter a spell unless its controller pays [2]." The spell is a target
    // (CR 355.9.a.2), chosen as this is played (CR 355.5); a paid Repeat's
    // execution chooses its own as well (CR 820.2), and may choose another.
    // Whether to pay is the spell's controller's decision as this resolves.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    static bool qualifies(const ChainItem& ci) { return ci.is_spell; }
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        return anyChainItem(state, qualifies);
    }
    static constexpr int kAwaitPay = 20;   // resume_point: the payer is deciding
    static constexpr int kDone = 21;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto spell = pickChainItem(ctx, "Hard Bargain: counter a spell",
                                         chainItemChoices(ctx, qualifies));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;
        auto& ri = ctx.state.chain.resuming.value();
        if (ri.resume_point == kDone) return;
        const ChainItem* item = *spell != kInvalidId ? chainItem(ctx.state, *spell) : nullptr;
        if (!item) {
            RIFT_TRACE(ctx.events, "HARD BARGAIN: the chosen spell is gone — no-op");
            ri.resume_point = kDone;
            return;
        }
        const PlayerId payer = item->controller;
        const GameObjectId src = item->source;
        const std::string spell_name = ctx.state.objectExists(src)
            ? ctx.state.getObject(src).name.get() : std::string("spell");
        if (ri.resume_point != kAwaitPay) {
            // [2] is Energy: floating Energy, ready runes and [Add] Reactions pay it.
            if (!canPayEnergy(ctx, payer, 2)) {
                RIFT_TRACE(ctx.events, "HARD BARGAIN: " + std::string(toString(payer)) +
                                    " can't pay [2] to save " + spell_name);
                counterChosenItem(ctx, *spell);
                ri.resume_point = kDone;
                return;
            }
            // The spell's controller: pay [2] to save it, or let it be countered.
            std::vector<Intent> choices;
            Intent pay;
            pay.type = IntentType::MakeChoice;
            pay.player = payer;
            pay.chosen_objects = {src};
            choices.push_back(pay);
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = payer;
            decline.chosen_value = 0;   // nameable by value, not only by position
            decline.declines = true;
            decline.choice_label = "Hard Bargain: don't pay";
            choices.push_back(decline);
            ctx.executor.requestChoice(payer, std::move(choices),
                                       "Hard Bargain: pay 2 to save " + spell_name +
                                       "? [pay | decline]");
            ri.resume_point = kAwaitPay;
            return;
        }
        auto choice = ctx.executor.takeChoice();
        ri.resume_point = kDone;
        const bool wants_to_pay = choice && !choice->chosen_objects.empty() &&
                                  choice->chosen_objects[0] == src;
        if (wants_to_pay && payEnergy(ctx, payer, 2)) {
            RIFT_TRACE(ctx.events, "HARD BARGAIN: " + std::string(toString(payer)) +
                                " paid [2] to save " + spell_name);
            return;
        }
        RIFT_TRACE(ctx.events, "HARD BARGAIN: " + spell_name + " — its controller did not pay [2]");
        counterChosenItem(ctx, *spell);
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
