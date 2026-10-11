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

// "When you conquer, you may exhaust me to reveal the top 2 cards of your Main
// Deck. You may banish one, then play it. Recycle the rest."
//
// Legend trigger (fires via TriggerManager::fireLegendTrigger on the
// controller's Conquer score). Resumable:
//   - "you may exhaust me to ...": the "may" and the cost open the effect, so
//     both belong to finalization (CR 383.3.a / 383.3.b) — the legend is
//     exhausted before anyone can respond. confirmOptional asks.
//   - The cards are only revealed as the ability resolves, so "You may banish
//     one" is decided then (CR 383.3.a.3): we snapshot the top-2 ids into
//     resume_data[1]/[2], and publish a banish choice (a "skip" slot plus
//     each revealed card). Phase tracked in resume_data[3]: -1 nothing done,
//     0 cost paid, 1 waiting for the banish choice, 2 waiting for where a
//     banished unit is played (CR 355.2.a; resume_data[0] is that unit).
//   - On re-entry: banish + play the chosen card (or none), recycle whatever
//     of the top-2 wasn't banished.
class VoidBurrower : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ri = ctx.state.chain.resuming.value();

        // "you may exhaust me" — legal only if I'm currently ready.
        // (Once the cost is paid I am exhausted: that no longer argues
        // against the effect.)
        auto still_legal = [&]() -> bool {
            return costAlreadyPaid(ctx) ||
                   (ctx.state.objectExists(ctx.source) &&
                    !ctx.state.getObject(ctx.source).is_exhausted);
        };
        int conf = confirmOptional(ctx, "Void Burrower: exhaust me to reveal 2?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent

        // Ensure our private resume_data slots exist: [1]/[2]= the revealed
        // ids, [3]=phase, [0]= the unit whose location is being asked.
        while (ri.resume_data.size() < 4) ri.resume_data.push_back(-1);
        if (conf == 1 && costDueNow(ctx) && ri.resume_data[3] < 0) {
            ctx.executor.exhaustObject(ctx.source);   // the cost, paid once
            ri.resume_data[3] = 0;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf == 0) return;   // declined or no longer legal
        if (ri.resume_data[3] < 0) return;   // the cost was not paid
        const int phase = ri.resume_data[3];

        if (phase == 0) {
            // First pass of the effect: reveal top 2.
            // Void Hatchling: "If you would reveal cards from a deck, look at
            // the top card first." — once, before the two are revealed.
            ctx.executor.lookBeforeReveal(ctx.controller);
            auto& ps = ctx.state.player(ctx.controller);
            GameObjectId a = kInvalidId, b = kInvalidId;
            int n = static_cast<int>(ps.main_deck.size());
            if (n >= 1) a = ps.main_deck[ps.main_deck.size() - 1];
            if (n >= 2) b = ps.main_deck[ps.main_deck.size() - 2];
            ri.resume_data[1] = static_cast<int32_t>(a);
            ri.resume_data[2] = static_cast<int32_t>(b);

            std::vector<GameObjectId> revealed;
            for (auto cid : {a, b}) {
                if (cid == kInvalidId || !ctx.state.objectExists(cid)) continue;
                const auto& o = ctx.state.getObject(cid);
                // "Reveal": both players see the top two.
                ctx.events.emit(CardRevealedEvent{cid, o.card_def_id, o.owner,
                                                  true, ctx.controller, ZoneType::MainDeck});
                knowledge::seenInDeck(ctx.state, cid, knowledge::everyone(), DeckEnd::Top,
                                      cid == a ? 0 : 1);
                revealed.push_back(cid);
            }
            if (revealed.empty()) {
                ctx.events.logTrace("VOID BURROWER: deck empty, nothing to reveal");
                ri.resume_data[3] = 1;   // nothing more to do
                return;
            }
            // Publish "you may banish one": a skip slot (no object, value 0, so
            // an agent can name it) plus one slot per revealed card.
            std::vector<Intent> choices;
            choices.push_back(valChoice(ctx.controller, 0, "Void Burrower: banish none"));
            for (auto cid : revealed) {
                Intent c; c.type = IntentType::MakeChoice; c.player = ctx.controller;
                c.chosen_objects = {cid}; choices.push_back(std::move(c));
            }
            ri.resume_data[3] = 1;  // phase = awaiting banish choice
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                       "Void Burrower: banish one of the top 2 (then play it)");
            return;
        }

        // phase == 1: consume the banish choice and finish.
        GameObjectId a = static_cast<GameObjectId>(ri.resume_data[1]);
        GameObjectId b = static_cast<GameObjectId>(ri.resume_data[2]);
        GameObjectId banished = kInvalidId;
        // CR 355.2.a: no location is printed, so a unit goes to its
        // controller's base or to a battlefield they control (and may be
        // played to), their choice. Gear goes to base.
        const auto& bfs = ctx.state.battlefields;
        auto unitPlayableAt = [&](const BattlefieldState& bf) {
            return bf.controller.has_value() && *bf.controller == ctx.controller &&
                   !bf.blocks_unit_play &&
                   !ctx.state.player(ctx.controller).units_play_base_only;
        };
        std::optional<LocationId> where_to;
        if (phase == 2) {
            // Re-entered with the location (0 = base, 1 + i = battlefield slot i).
            banished = static_cast<GameObjectId>(ri.resume_data[0]);
            const int v = takenValue(ctx, 0);
            if (v >= 1 && static_cast<size_t>(v) <= bfs.size() && unitPlayableAt(bfs[v - 1]))
                where_to = LocationId{BattlefieldLocation{bfs[v - 1].id}};
        } else {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty())
                banished = choice->chosen_objects[0];
            // Asked while both cards are still revealed on the deck, and only
            // for a unit that can be paid for (unpaid, it is not played).
            if ((banished == a || banished == b) && banished != kInvalidId &&
                ctx.state.objectExists(banished) &&
                ctx.state.getObject(banished).isUnit() &&
                ctx.state.getObject(banished).zone == ZoneType::MainDeck &&
                ctx.executor.canPlayPayingCost(ctx.controller, banished)) {
                std::vector<Intent> where;
                for (size_t i = 0; i < bfs.size(); ++i)
                    if (unitPlayableAt(bfs[i]))
                        where.push_back(valChoice(ctx.controller, static_cast<int>(i) + 1,
                            "Void Burrower: play to battlefield " + std::to_string(i)));
                if (!where.empty()) {
                    where.insert(where.begin(),
                                 valChoice(ctx.controller, 0, "Void Burrower: play to base"));
                    ri.resume_data[0] = static_cast<int32_t>(banished);
                    ri.resume_data[3] = 2;   // phase = awaiting the location
                    ctx.executor.requestChoice(ctx.controller, std::move(where),
                                               "Void Burrower: where to play it");
                    return;
                }
            }
        }

        // Remove the top-2 from the deck (those we revealed).
        std::vector<GameObjectId> top;
        for (auto cid : {a, b}) {
            if (cid == kInvalidId || !ctx.state.objectExists(cid)) continue;
            if (deck::take(ctx.state, cid, deck::Place::Held, knowledge::everyone())) top.push_back(cid);
        }

        // Banish + play the chosen card; recycle the rest.
        std::vector<GameObjectId> rest;
        for (auto cid : top) {
            if (cid == banished) continue;
            rest.push_back(cid);
        }
        if (banished != kInvalidId && ctx.state.objectExists(banished) &&
            std::find(top.begin(), top.end(), banished) != top.end()) {
            // "banish one" — through the executor, so it counts as a card
            // banished this turn and "when you banish a card" abilities see it.
            ctx.events.logTrace("VOID BURROWER: banished " +
                                ctx.state.getObject(banished).name + " then play it");
            ctx.executor.banishObject(banished);
            // "then play it". The text does not waive the cost (CR 356.1.b.2):
            // playPayingCost takes the card out of banishment once it is paid
            // for; otherwise it simply stays banished.
            ctx.executor.playPayingCost(ctx.controller, banished, where_to);
        }
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
            ctx.events.logTrace("VOID BURROWER: recycled the rest");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 553;
        d.def_id = R"RB(sfd-243-221)RB";
        d.name = R"RB(Void Burrower)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-243/221)RB";
        d.collector_number = 243;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Fury, Domain::Order};
        d.tags = {R"RB(Rek'Sai)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you conquer, you may exhaust me to reveal the top 2 cards of your Main Deck. You may banish one, then play it. Recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/968eb5c484a25bbebd162f31736024b4ff3b0d07-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_553(CardRegistry& r) {
    r.registerCard(553, std::make_unique<VoidBurrower>());
}

} // namespace riftbound
