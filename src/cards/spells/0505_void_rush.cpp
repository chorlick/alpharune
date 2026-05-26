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

// "Reveal the top 2 cards of your Main Deck. You may banish one, then play it,
//  reducing its cost by [2]. Draw any you didn't banish."

class VoidRush : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Top of deck = back of vector. Reveal up to top 2.
        std::vector<GameObjectId> top2;
        int n = (int)ps.main_deck.size();
        if (n >= 1) top2.push_back(ps.main_deck[n - 1]);
        if (n >= 2) top2.push_back(ps.main_deck[n - 2]);
        if (top2.empty()) return;
        ctx.events.logTrace("VOID RUSH: revealed top " +
                            std::to_string(top2.size()) + " card(s)");

        // Choose which (if any) to banish-and-play: prefer a playable card.
        GameObjectId to_play = kInvalidId;
        for (auto cid : top2) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& c = ctx.state.getObject(cid);
            if (c.isUnit() || c.isGear() || c.isSpell()) { to_play = cid; break; }
        }

        if (to_play != kInvalidId && ctx.state.objectExists(to_play)) {
            // Remove from deck, banish, then play ignoring cost. The printed
            // "reduce its cost by [2]" has no partial-reduction play API, so
            // we play it for free (over-generous by at most [2]; documented).
            auto& deck = ps.main_deck;
            deck.erase(std::remove(deck.begin(), deck.end(), to_play), deck.end());
            ctx.executor.banishObject(to_play);
            auto& bz = ps.banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), to_play), bz.end());
            ctx.executor.playIgnoringCost(ctx.controller, to_play,
                                          LocationId{BaseLocation{ctx.controller}});
            ctx.events.logTrace("VOID RUSH: banished + played top card "
                                "(cost reduction [2] approximated as free)");
        }

        // Draw any of the revealed cards not banished (they are still on top).
        for (auto cid : top2) {
            if (cid == to_play) continue;
            auto& deck = ps.main_deck;
            if (std::find(deck.begin(), deck.end(), cid) != deck.end())
                ctx.executor.drawCards(ctx.controller, 1);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 505;
        d.def_id = R"RB(sfd-188-221)RB";
        d.name = R"RB(Void Rush)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-188/221)RB";
        d.collector_number = 188;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Reveal the top 2 cards of your Main Deck. You may banish one, then play it, reducing its cost by [2]. Draw any you didn't banish.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ce606f096ae76485cd8dd3cc337629b510f0d1e7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_505(CardRegistry& r) {
    r.registerCard(505, std::make_unique<VoidRush>());
}

} // namespace riftbound
