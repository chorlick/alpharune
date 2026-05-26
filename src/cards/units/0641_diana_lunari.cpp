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

class DianaLunari : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a showdown begins here, ..." — now fires on the real event
    // (was approximated by WhenIAttackOrDefend).
    TriggerType triggerType() const override { return TriggerType::WhenAShowdownBeginsHere; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Diana Lunari: pay [1] to Predict + reveal?",
                                   still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;

        // [Predict] — look at top card, agent may recycle it.
        ctx.executor.predict(ctx.controller, 1);

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
            ps.main_deck.pop_back();
            card.zone = ZoneType::Hand;
            card.location = std::nullopt;
            ps.hand.push_back(top);
            ctx.events.logTrace("DIANA LUNARI: revealed spell " + card.name +
                                 " -> draw it");
        } else {
            ctx.events.logTrace("DIANA LUNARI: revealed non-spell " + card.name);
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
        d.ability_text = R"RB(When a showdown begins here, you may pay [1]. If you do, [Predict], then reveal the top card of your Main Deck. If it's a spell, draw it. (To Predict, look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3e284397713abb21d2c8d9b85202ab65d21689e9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_641(CardRegistry& r) {
    r.registerCard(641, std::make_unique<DianaLunari>());
}

} // namespace riftbound
