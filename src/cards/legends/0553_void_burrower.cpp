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
//   - confirmOptional gate ("exhaust me?") reserves resume_points 0/1/2 +
//     resume_data[0].
//   - After confirm we exhaust the legend, snapshot the top-2 ids into
//     resume_data[1]/[2], and publish a banish choice (a "skip" slot plus
//     each revealed card). Phase tracked in resume_data[3].
//   - On re-entry: banish + play the chosen card (or none), recycle whatever
//     of the top-2 wasn't banished.
class VoidBurrower : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ri = ctx.state.chain.resuming.value();

        // "you may exhaust me" — legal only if I'm currently ready.
        auto still_legal = [&]() -> bool {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx, "Void Burrower: exhaust me to reveal 2?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;   // declined or no longer legal

        // Ensure our private resume_data slots exist: [0]=confirm, [1]/[2]=
        // the revealed ids, [3]=phase.
        while (ri.resume_data.size() < 4) ri.resume_data.push_back(-1);
        const int phase = ri.resume_data[3];

        if (phase < 0) {
            // First pass after confirmation: exhaust me + reveal top 2.
            ctx.executor.exhaustObject(ctx.source);
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
                ctx.events.emit(CardRevealedEvent{cid, o.card_def_id, o.owner,
                                                  false, ctx.controller, ZoneType::MainDeck});
                revealed.push_back(cid);
            }
            if (revealed.empty()) {
                ctx.events.logTrace("VOID BURROWER: deck empty, nothing to reveal");
                return;
            }
            // Publish "you may banish one": a skip slot (empty chosen_objects)
            // plus one slot per revealed card.
            std::vector<Intent> choices;
            { Intent skip; skip.type = IntentType::MakeChoice; skip.player = ctx.controller;
              choices.push_back(std::move(skip)); }
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
        auto choice = ctx.executor.takeChoice();
        if (choice && !choice->chosen_objects.empty())
            banished = choice->chosen_objects[0];

        auto& ps = ctx.state.player(ctx.controller);
        // Remove the top-2 from the deck (those we revealed).
        std::vector<GameObjectId> top;
        for (auto cid : {a, b}) {
            if (cid == kInvalidId || !ctx.state.objectExists(cid)) continue;
            auto it = std::find(ps.main_deck.begin(), ps.main_deck.end(), cid);
            if (it != ps.main_deck.end()) { ps.main_deck.erase(it); top.push_back(cid); }
        }

        // Banish + play the chosen card; recycle the rest.
        std::vector<GameObjectId> rest;
        for (auto cid : top) {
            if (cid == banished) continue;
            rest.push_back(cid);
        }
        if (banished != kInvalidId && ctx.state.objectExists(banished)) {
            auto& bobj = ctx.state.getObject(banished);
            // "banish one" — momentarily route to banishment.
            bobj.zone = ZoneType::Banishment;
            bobj.location = std::nullopt;
            auto& bz = ctx.state.player(bobj.owner).banishment;
            bz.push_back(banished);
            ctx.events.logTrace("VOID BURROWER: banished " + bobj.name + " then play it");
            // "then play it" — leave banishment and play ignoring its cost.
            // playIgnoringCost does not remove the card from the banishment
            // vector, so erase it here first. Per CR 355.2.a permanents land
            // at base by default.
            auto it = std::find(bz.begin(), bz.end(), banished);
            if (it != bz.end()) bz.erase(it);
            ctx.executor.playIgnoringCost(ctx.controller, banished);
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
