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

class BaitedHook : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main
    //  Deck. You may banish a unit from among them that has Might up to 1 more
    //  than the killed unit and play it, ignoring its cost. Then recycle the rest."
    // The cost is [1] + [C] + exhaust. [C] is one Power of this card's Domain
    // (CR 135.2.e.6), so Order — it is not "recycle this".
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1, .power = 1,
                              .power_domain = Domain::Order};
    }
    // Target = the friendly unit to kill.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }

    // The callback is re-entered after each answer, and the killed unit is no
    // longer a legal target by then (the engine filters `targets`), so the
    // kill is recorded on the chain item: resume_data[kKilledSlot] is
    // 1 + the killed unit's Might once the kill has happened.
    static constexpr size_t kKilledSlot = 7;

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ri = ctx.state.chain.resuming;
        int killed_might = 0;
        if (ri.has_value() && ri->resume_data.size() > kKilledSlot &&
            ri->resume_data[kKilledSlot] != 0) {
            killed_might = ri->resume_data[kKilledSlot] - 1;
        } else {
            // "Kill a friendly unit." A unit that left the board in response
            // is not killed, and then nothing below has a Might to compare
            // against (CR 359.3.e.12): the ability does nothing more.
            if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
            const GameObjectId victim = targets[0];
            {
                const auto& v = ctx.state.getObject(victim);
                if (!v.isUnit() || !v.location.has_value()) return;
                killed_might = v.current_might;
            }
            ctx.executor.killObject(victim);
            if (ctx.state.getObject(victim).location.has_value()) return;   // death replaced
            if (ri.has_value()) {
                while (ri->resume_data.size() <= kKilledSlot) ri->resume_data.push_back(0);
                ri->resume_data[kKilledSlot] = 1 + std::max(0, killed_might);
            }
        }

        // Look at the top 5 cards of the Main Deck (top = back).
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> top5;
        for (auto it = ps.main_deck.rbegin();
             it != ps.main_deck.rend() && (int)top5.size() < 5; ++it) {
            top5.push_back(*it);
        }
        // A private look: the controller now knows these five and their order.
        const auto looker = knowledge::only(ctx.controller);
        for (auto id : top5) knowledge::shown(ctx.state, id, looker);
        // "As you look at me" replacements (Nocturne, Horrifying; D71): a
        // card that leaves as it is looked at is not one of the five.
        top5.erase(std::remove_if(top5.begin(), top5.end(), [&](GameObjectId id) {
            return !ctx.executor.seenOnTopOfDeck(ctx.controller, id);
        }), top5.end());
        // Eligible: units with Might up to killed_might + 1.
        std::vector<GameObjectId> eligible;
        for (auto cid : top5) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.base_might > killed_might + 1) continue;
            eligible.push_back(cid);
        }
        // "You may banish a unit from among them ... and play it": the
        // controller may take none. (A card in a deck is picked, not chosen:
        // no Deflect, no "when you choose".)
        // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
        const auto picked = selectObject(ctx, "Baited Hook: banish and play a unit from the top 5",
                                         eligible, kKilledSlot + 1, /*may_decline=*/true);
        if (!picked.has_value()) return;   // waiting on the agent
        if (*picked != kInvalidId &&
            std::find(eligible.begin(), eligible.end(), *picked) != eligible.end()) {
            // CR 355.2.a: no location is printed, so the unit goes to its
            // controller's base or to a battlefield they control (and may be
            // played to). Asked before anything leaves the deck.
            // At resolution: a play location is a permission, not a target (CR 355.10.b; M5; batch M1).
            const int where = pickEffectPlayLocation(*this, ctx, "Baited Hook: play it to");
            if (where == -1) return;   // waiting for the agent
            // Banish it (out of the deck, counted and announced as a banish),
            // then play it from there ignoring its cost.
            ctx.executor.banishObject(*picked);
            ctx.executor.playIgnoringCost(ctx.controller, *picked,
                                          effectPlayLocation(ctx.state, ctx.controller, where));
        }
        // "Then recycle the rest" — the other looked-at cards go to the bottom
        // of the Main Deck (recycle = put on bottom). Move remaining top5 cards
        // (still in deck) to the bottom (front = bottom).
        std::vector<GameObjectId> to_recycle;
        for (auto cid : top5) {
            if (deck::take(ctx.state, cid, deck::Place::Held, looker, /*slot_public=*/false)) {
                to_recycle.push_back(cid);
            }
        }
        // Insert recycled cards at the bottom (front).
        for (auto it = to_recycle.rbegin(); it != to_recycle.rend(); ++it) {
            deck::putOnBottom(ctx.state, ctx.controller, *it, deck::Place::Held, looker);
        }
        // These cards were Recycled (CR 437.1): announce it once, the way
        // EffectExecutor::recycleCards does ("When you recycle ...").
        if (!to_recycle.empty())
            ctx.events.emit(ObjectStateChangedEvent{to_recycle.front(), "recycled_main"});
        RIFT_TRACE(ctx.events, "BAITED HOOK: kill friendly, dig top 5, recycle rest");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 242;
        d.def_id = R"RB(ogn-242-298)RB";
        d.name = R"RB(Baited Hook)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-242/298)RB";
        d.collector_number = 242;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([1][C], [E]: Kill a friendly unit. Look at the top 5 cards of your Main Deck. You may banish a unit from among them that has Might up to 1 more than the killed unit and play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7b4e6a1e32878a7d5fe4e40869c83e47ea08de7c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_242(CardRegistry& r) {
    r.registerCard(242, std::make_unique<BaitedHook>());
}

} // namespace riftbound
