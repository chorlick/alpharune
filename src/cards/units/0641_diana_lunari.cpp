#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class DianaLunari : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When a showdown begins here, ..." — now fires on the real event
    // (was approximated by WhenIAttackOrDefend).
    TriggerType triggerType() const override { return TriggerType::WhenAShowdownBeginsHere; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // CR 445.2 / 164.2.a: the [1] may come from floating Energy or from
        // exhausting a ready rune (payableEnergy / payEnergy).
        auto still_legal = [&ctx]() {
            return payableEnergy(ctx.state, ctx.controller) >= 1;
        };
        // No return ahead of the "may": with [1] unpayable confirmOptional
        // stores the "no" on the chain item, so the resolution asks nothing
        // if the [1] has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Diana Lunari: pay [1] to Predict + reveal?",
                                   still_legal);
        if (conf < 1) return;
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)

        // [Predict] — look at top card, agent may recycle it.
        ctx.executor.predict(ctx.controller, 1);

        // "then reveal the top card of your Main Deck" is a reveal of its own:
        // Void Hatchling's look ("If you would reveal cards from a deck, look
        // at the top card first") comes after the Predict, before the reveal.
        ctx.executor.lookBeforeReveal(ctx.controller);
        // Reveal the (possibly new) top card of the main deck.
        if (ps.main_deck.empty()) return;
        GameObjectId top = ps.main_deck.back();
        if (!ctx.state.objectExists(top)) return;
        auto& card = ctx.state.getObject(top);
        ctx.events.emit(CardRevealedEvent{
            /*card=*/top,
            /*card_def_id=*/card.card_def_id,
            /*owner=*/card.owner,
            /*revealed_to_all=*/true,
            /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::MainDeck,
        });
        if (card.isSpell()) {
            deck::drawTop(ctx.state, ctx.controller, knowledge::everyone());
            RIFT_TRACE(ctx.events, "DIANA LUNARI: revealed spell " + card.name +
                                 " -> draw it");
        } else {
            RIFT_TRACE(ctx.events, "DIANA LUNARI: revealed non-spell " + card.name);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 641;
        d.def_id = R"RB(unl-079-219)RB";
        d.name = R"RB(Diana, Lunari)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-079/219)RB";
        d.collector_number = 79;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Diana)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Predict);
        d.ability_text = R"RB(When a showdown begins here, you may pay [1] to [Predict], then reveal the top card of your Main Deck. If it's a spell, draw it. (To Predict, look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e284397713abb21d2c8d9b85202ab65d21689e9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_641(CardRegistry& r) {
    r.registerCard(641, std::make_unique<DianaLunari>());
}

} // namespace riftbound
