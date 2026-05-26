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

class PromisingFuture : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each player looks at the top 5 cards of their Main Deck, banishes one of
    // them, then recycles the rest. Starting with the next player, each player
    // plays those cards [the banished card], ignoring Energy costs."
    // (Power cost approximated as free.)
    //
    // Resumable: rp0 prompt controller to banish one of their top 5; rp1 apply
    // controller's banish + recycle rest, prompt opponent; rp2 apply opponent's
    // banish + recycle rest, then both players play their banished card
    // (next player = opponent first). Banished ids stashed in resume_data[0/1].
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);

        // Reveal top 5 (privately, to that player) and offer a banish choice.
        auto prompt_banish = [&](PlayerId p) -> bool {
            auto& ps = ctx.state.player(p);
            int n = std::min(5, static_cast<int>(ps.main_deck.size()));
            if (n == 0) return false;
            std::vector<Intent> choices;
            for (int i = 0; i < n; ++i) {
                GameObjectId cid = ps.main_deck[ps.main_deck.size() - 1 - i];
                if (!ctx.state.objectExists(cid)) continue;
                const auto& obj = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{cid, obj.card_def_id, obj.owner,
                                                  false, p, ZoneType::MainDeck});
                Intent c; c.type = IntentType::MakeChoice; c.player = p;
                c.chosen_objects = {cid}; choices.push_back(std::move(c));
            }
            if (choices.empty()) return false;
            ctx.executor.requestChoice(p, std::move(choices),
                                        "Promising Future: banish one of your top 5");
            return true;
        };

        // Apply: banish the chosen card, recycle the rest of the top 5.
        // Returns the banished id (or kInvalidId).
        auto apply_banish = [&](PlayerId p) -> GameObjectId {
            auto& ps = ctx.state.player(p);
            GameObjectId banished = kInvalidId;
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty())
                banished = choice->chosen_objects[0];
            // Collect the top 5 (now), excluding the banished one for recycling.
            int n = std::min(5, static_cast<int>(ps.main_deck.size()));
            std::vector<GameObjectId> top;
            for (int i = 0; i < n; ++i)
                top.push_back(ps.main_deck[ps.main_deck.size() - 1 - i]);
            // Remove all top-5 from the deck.
            for (auto cid : top) {
                auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
                if (it != ps.main_deck.end()) ps.main_deck.erase(it);
            }
            // Banish the chosen, recycle the rest.
            std::vector<GameObjectId> rest;
            for (auto cid : top) {
                if (cid == banished) continue;
                rest.push_back(cid);
            }
            if (banished != kInvalidId && ctx.state.objectExists(banished)) {
                auto& b = ctx.state.getObject(banished);
                b.zone = ZoneType::Banishment;
                b.location = std::nullopt;
                ctx.state.player(b.owner).banishment.push_back(banished);
            }
            if (!rest.empty()) ctx.executor.recycleCards(p, rest);
            return banished;
        };

        auto play_banished = [&](PlayerId p, GameObjectId banished) {
            if (banished == kInvalidId || !ctx.state.objectExists(banished)) return;
            auto& bz = ctx.state.player(p).banishment;
            auto it = std::find(bz.begin(), bz.end(), banished);
            if (it != bz.end()) bz.erase(it);
            // Only units/gear are playable to a board; spells would need chain
            // handling. playIgnoringCost places permanents; for spells it puts
            // them on board zone without resolution (best-effort, free).
            ctx.executor.playIgnoringCost(p, banished);
        };

        // Finish: play both banished cards (next player = opponent first).
        auto finish = [&]() {
            GameObjectId ctrl_b = static_cast<GameObjectId>(ri.resume_data[0]);
            GameObjectId opp_b   = static_cast<GameObjectId>(ri.resume_data[1]);
            play_banished(opp, opp_b);
            play_banished(ctx.controller, ctrl_b);
            ctx.events.logTrace("PROMISING FUTURE: each player banished 1 of top 5, "
                                "recycled the rest, then played the banished card");
        };

        if (ri.resume_point == 0) {
            while (ri.resume_data.size() < 2) ri.resume_data.push_back(-1);
            if (prompt_banish(ctx.controller)) { ri.resume_point = 1; return; }
            // controller has no cards — go straight to opponent's banish.
            if (prompt_banish(opp)) { ri.resume_point = 2; return; }
            finish();
            return;
        }
        if (ri.resume_point == 1) {
            ri.resume_data[0] = static_cast<int32_t>(apply_banish(ctx.controller));
            if (prompt_banish(opp)) { ri.resume_point = 2; return; }
            finish();
            return;
        }
        if (ri.resume_point == 2) {
            ri.resume_data[1] = static_cast<int32_t>(apply_banish(opp));
            finish();
            return;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 115;
        d.def_id = R"RB(ogn-115-298)RB";
        d.name = R"RB(Promising Future)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-115/298)RB";
        d.collector_number = 115;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Each player looks at the top 5 cards of their Main Deck, banishes one of them, then recycles the rest. Starting with the next player, each player plays those cards, ignoring Energy costs. (They must still pay Power costs.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1ea5e7e50aaecee0b2ee2657d3c829dff0718fba-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_115(CardRegistry& r) {
    r.registerCard(115, std::make_unique<PromisingFuture>());
}

} // namespace riftbound
