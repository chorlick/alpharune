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

class Reinforce : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Look at the top 5 cards of your Main Deck. You may banish a unit from
    //  among them, then play it, reducing its cost by [5]. Recycle the
    //  remaining cards."
    // NOTE: there is no partial cost-reduction play path in the executor;
    // playIgnoringCost ignores ALL cost. The chosen unit is played for free
    // (documented approximation of "-[5]"; most reachable units cost <= 5).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);

        // First entry: peek up to 5 from the top, stash them in ri.targets.
        // A leading sentinel (kInvalidId) marks "already peeked".
        if (ri.targets.empty()) {
            ri.targets.push_back(kInvalidId);  // sentinel: peeked-flag
            int n = std::min(5, static_cast<int>(ps.main_deck.size()));
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    cid, obj.card_def_id, obj.owner,
                    /*revealed_to_all=*/false, ctx.controller, ZoneType::MainDeck});
                ri.targets.push_back(cid);
            }
        }

        // Build the list of unit candidates among the peeked cards.
        std::vector<GameObjectId> units;
        for (size_t i = 1; i < ri.targets.size(); ++i) {
            GameObjectId cid = ri.targets[i];
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) units.push_back(cid);
        }

        GameObjectId chosen = kInvalidId;
        if (!units.empty()) {
            chosen = pickTarget(ctx, "Reinforce: banish a unit to play", units);
            // pickTarget returns kInvalidId on suspend (units non-empty, so it
            // can't be a no-targets fizzle) — yield to the agent.
            if (chosen == kInvalidId) return;
        }

        // Recycle the remaining peeked cards (everything except the chosen),
        // detaching all peeked cards from the deck first.
        std::vector<GameObjectId> rest;
        for (size_t i = 1; i < ri.targets.size(); ++i) {
            GameObjectId cid = ri.targets[i];
            if (cid == chosen) continue;
            if (!ctx.state.objectExists(cid)) continue;
            rest.push_back(cid);
        }
        for (GameObjectId cid : ri.targets) {
            if (cid == kInvalidId) continue;
            ps.main_deck.erase(std::remove(ps.main_deck.begin(), ps.main_deck.end(), cid),
                               ps.main_deck.end());
        }
        if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);

        if (chosen != kInvalidId && ctx.state.objectExists(chosen)) {
            ctx.executor.banishObject(chosen);
            ctx.executor.playIgnoringCost(ctx.controller, chosen);
            ctx.events.logTrace("REINFORCE: banished + played " +
                                ctx.state.getObject(chosen).name + " (cost reduced)");
        } else {
            ctx.events.logTrace("REINFORCE: no unit chosen; recycled top 5");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 62;
        d.def_id = R"RB(ogn-062-298)RB";
        d.name = R"RB(Reinforce)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-062/298)RB";
        d.collector_number = 62;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 5;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Look at the top 5 cards of your Main Deck. You may banish a unit from among them, then play it, reducing its cost by [5]. Recycle the remaining cards.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5bb4ce960c7636dac103c560a2e2f4e32f9d5390-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_62(CardRegistry& r) {
    r.registerCard(62, std::make_unique<Reinforce>());
}

} // namespace riftbound
