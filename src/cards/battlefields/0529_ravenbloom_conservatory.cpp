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

class RavenbloomConservatory : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.main_deck.empty()) return;

        // Top of deck = back of vector.
        GameObjectId top = ps.main_deck.back();
        if (!ctx.state.objectExists(top)) return;
        auto& card = ctx.state.getObject(top);

        // Reveal it publicly.
        ctx.events.emit(CardRevealedEvent{
            /*card=*/top,
            /*card_def_id=*/card.card_def_id,
            /*owner=*/card.owner,
            /*revealed_to_all=*/true,
            /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::MainDeck,
        });

        if (card.isSpell()) {
            // Put it in hand.
            ps.main_deck.pop_back();
            card.zone = ZoneType::Hand;
            card.location = std::nullopt;
            ps.hand.push_back(top);
            ctx.events.logTrace("RAVENBLOOM: revealed spell " + card.name +
                                 " -> hand");
        } else {
            // Recycle it (bottom of owner's main deck). Pop first so
            // recycleCards re-inserts it at the bottom exactly once.
            ps.main_deck.pop_back();
            ctx.executor.recycleCards(ctx.controller, {top});
            ctx.events.logTrace("RAVENBLOOM: revealed non-spell " + card.name +
                                 " -> recycled");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 529;
        d.def_id = R"RB(sfd-215-221)RB";
        d.name = R"RB(Ravenbloom Conservatory)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-215/221)RB";
        d.collector_number = 215;
        d.artist = R"RB(Will Gist)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you defend here, reveal the top card of your Main Deck. If it's a spell, put it in your hand. Otherwise, recycle it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d4ded211cb972b7f64bad2b99ee53abe8d13e4e2-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_529(CardRegistry& r) {
    r.registerCard(529, std::make_unique<RavenbloomConservatory>());
}

} // namespace riftbound
