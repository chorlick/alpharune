#include "cards/card.h"
#include "cards/card_helpers.h"
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

class PakaaProtector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I move, reveal the top card of your Main Deck. If it's a unit, draw
    //  it. Otherwise, put it in your trash and give me +2 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Void Hatchling: "If you would reveal cards from a deck, look at the
        // top card first." — this reveal counts too.
        ctx.executor.lookBeforeReveal(ctx.controller);
        if (ps.main_deck.empty()) return;
        GameObjectId top = ps.main_deck.back();
        if (!ctx.state.objectExists(top)) return;
        auto& obj = ctx.state.getObject(top);
        ctx.events.emit(CardRevealedEvent{
            top, obj.card_def_id, obj.owner,
            /*revealed_to_all=*/true, /*revealed_to=*/ctx.controller,
            ZoneType::MainDeck});
        knowledge::seenInDeck(ctx.state, top, knowledge::everyone(), DeckEnd::Top, 0);
        if (obj.isUnit()) {
            // "draw it": the revealed card is still the top card.
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("PAKAA PROTECTOR: revealed unit " + obj.name + " -> drew it");
            return;
        }
        deck::takeTop(ctx.state, ctx.controller, deck::Place::Public, knowledge::everyone());
        obj.zone = ZoneType::Trash;
        obj.location = std::nullopt;
        ps.trash.push_back(top);
        ctx.events.logTrace("PAKAA PROTECTOR: revealed non-unit " + obj.name +
                            " -> trash, +2 [M] this turn");
        if (ctx.state.objectExists(ctx.source) &&
            ctx.state.getObject(ctx.source).location.has_value())
            ctx.executor.giveTemporaryMight(ctx.source, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 820;
        d.def_id = R"RB(ven-033-166)RB";
        d.name = R"RB(Pakaa Protector)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-033/166)RB";
        d.collector_number = 33;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Cat)RB", R"RB(Shurima)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move, reveal the top card of your Main Deck. If it's a unit, draw it. Otherwise, put it in your trash and give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab3be32adadb72831c07750d8defc7918fd32d96-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_820(CardRegistry& r) {
    r.registerCard(820, std::make_unique<PakaaProtector>());
}

}  // namespace riftbound
