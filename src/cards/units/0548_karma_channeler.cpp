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

// "[Vision] (When you play me, look at the top card of your Main Deck. You may
//  recycle it.) When you recycle one or more cards to your Main Deck, buff a
//  friendly unit."

class KarmaChanneler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // [Vision] is keyword-driven in the engine but card 548's registry data
    // has no keyword set; implement the look-at-top here on play. A spell on
    // top is recycled to the bottom (mirrors the engine's Vision behavior);
    // non-spell top cards are left in place.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.main_deck.empty()) return;
        auto top_card = ps.main_deck.back();
        if (!ctx.state.objectExists(top_card)) return;
        auto& top_obj = ctx.state.getObject(top_card);
        ctx.events.emit(CardRevealedEvent{
            top_card, top_obj.card_def_id, top_obj.owner,
            /*revealed_to_all=*/false, /*revealed_to=*/ctx.controller,
            ZoneType::MainDeck});
        if (top_obj.isSpell()) {
            ps.main_deck.pop_back();
            ps.main_deck.insert(ps.main_deck.begin(), top_card);
            ctx.events.logTrace("KARMA, CHANNELER: [Vision] recycled top spell to bottom");
        }
        // ENGINE GAP: "When you recycle one or more cards to your Main Deck,
        // buff a friendly unit" — there is no WhenYouRecycle(-to-deck) trigger
        // event in the engine, so the recycle-buff clause is not wired.
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 548;
        d.def_id = R"RB(sfd-237-221)RB";
        d.name = R"RB(Karma, Channeler)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-237/221)RB";
        d.collector_number = 237;
        d.artist = R"RB(Andres Blanco)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Karma)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Vision);
        d.ability_text = R"RB([Vision] (When you play me, look at the top card of your Main Deck. You may recycle it.)
When you recycle one or more cards to your Main Deck, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff. Runes aren't cards.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ba0113d449813c94534ae0e74f3ef38f9b8010c2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_548(CardRegistry& r) {
    r.registerCard(548, std::make_unique<KarmaChanneler>());
}

} // namespace riftbound
