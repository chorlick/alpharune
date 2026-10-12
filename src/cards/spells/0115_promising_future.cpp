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
    // A unit has no printed location, so the player playing it chooses their
    // base or a battlefield they control (CR 355.2.a): rp3 while the opponent
    // is asked where, rp4 while the controller is.
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
            // "banishes one of them": only a card among those looked at can be
            // the answer. Anything else is not on offer — the top card is
            // banished instead (the banish is not optional).
            if (std::find(top.begin(), top.end(), banished) == top.end())
                banished = top.empty() ? kInvalidId : top.front();
            // Remove all top-5 from the deck.
            for (auto cid : top) {
                deck::take(ctx.state, cid, deck::Place::Held, knowledge::only(p), /*slot_public=*/false);
            }
            // Banish the chosen, recycle the rest.
            std::vector<GameObjectId> rest;
            for (auto cid : top) {
                if (cid == banished) continue;
                rest.push_back(cid);
            }
            // Through the executor: it counts the banish and announces it
            // ("When you banish a card you own").
            if (banished != kInvalidId && ctx.state.objectExists(banished))
                ctx.executor.banishObject(banished, /*by=*/p);   // "each player ... banishes"
            if (!rest.empty()) ctx.executor.recycleCards(p, rest);
            return banished;
        };

        // "ignoring Energy costs": Power is still owed, and paying is
        // optional (CR 445.2) — unpaid, the card stays banished.
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        auto play_banished = [&](PlayerId p, GameObjectId banished,
                                 std::optional<LocationId> where) {
            if (banished == kInvalidId || !ctx.state.objectExists(banished)) return;
            // The play takes the card out of banishment once its Power is paid.
            ctx.executor.playPayingCost(p, banished, where, power_only);
        };

        // A battlefield `p` may play a unit to (as Wild Claw 876): one they
        // control, not one where units can't be played, and none while they
        // may only play units to base.
        const auto& bfs = ctx.state.battlefields;
        auto unitPlayableAt = [&](PlayerId p, const BattlefieldState& bf) {
            return bf.controller.has_value() && *bf.controller == p &&
                   !bf.blocks_unit_play && !ctx.state.player(p).units_play_base_only;
        };
        // CR 355.2.a: ask `p` where their banished unit goes. Only for a unit
        // they can pay for, and only with a battlefield to offer. Returns
        // whether a prompt went out. 0 = base, 1 + i = battlefield slot i.
        auto prompt_where = [&](PlayerId p, GameObjectId banished) -> bool {
            if (banished == kInvalidId || !ctx.state.objectExists(banished)) return false;
            if (!ctx.state.getObject(banished).isUnit()) return false;
            if (!ctx.executor.canPlayPayingCost(p, banished, power_only)) return false;
            std::vector<Intent> where;
            for (size_t i = 0; i < bfs.size(); ++i)
                if (unitPlayableAt(p, bfs[i]))
                    where.push_back(valChoice(p, static_cast<int>(i) + 1,
                        "Promising Future: play to battlefield " + std::to_string(i)));
            if (where.empty()) return false;
            where.insert(where.begin(), valChoice(p, 0, "Promising Future: play to base"));
            ctx.executor.requestChoice(p, std::move(where),
                                       "Promising Future: where to play it");
            return true;
        };
        auto taken_where = [&](PlayerId p) -> std::optional<LocationId> {
            const int v = takenValue(ctx, 0);
            if (v >= 1 && static_cast<size_t>(v) <= bfs.size() && unitPlayableAt(p, bfs[v - 1]))
                return LocationId{BattlefieldLocation{bfs[v - 1].id}};
            return std::nullopt;   // base
        };

        // Finish: play both banished cards (next player = opponent first),
        // each after its player said where. Re-entered at rp3 / rp4 with the
        // answer to the prompt that went out.
        auto finish = [&]() {
            GameObjectId ctrl_b = GameObjectId::fromRaw(ri.resume_data[0]);
            GameObjectId opp_b   = GameObjectId::fromRaw(ri.resume_data[1]);
            if (ri.resume_point < 3) {
                if (prompt_where(opp, opp_b)) { ri.resume_point = 3; return; }
                play_banished(opp, opp_b, std::nullopt);
            } else if (ri.resume_point == 3) {
                play_banished(opp, opp_b, taken_where(opp));
            }
            if (ri.resume_point != 4) {
                if (prompt_where(ctx.controller, ctrl_b)) { ri.resume_point = 4; return; }
                play_banished(ctx.controller, ctrl_b, std::nullopt);
            } else {
                play_banished(ctx.controller, ctrl_b, taken_where(ctx.controller));
            }
            ri.resume_point = 5;   // done
            RIFT_TRACE(ctx.events, "PROMISING FUTURE: each player banished 1 of top 5, "
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
            ri.resume_data[0] = static_cast<int32_t>((apply_banish(ctx.controller)).raw());
            if (prompt_banish(opp)) { ri.resume_point = 2; return; }
            finish();
            return;
        }
        if (ri.resume_point == 2) {
            ri.resume_data[1] = static_cast<int32_t>((apply_banish(opp)).raw());
            finish();
            return;
        }
        if (ri.resume_point == 3 || ri.resume_point == 4) {
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
