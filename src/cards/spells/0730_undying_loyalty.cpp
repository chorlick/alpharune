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

// "This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
//  Play a unit with cost no more than [2] and no more than [A] from your
//  trash, ignoring its cost."
//
// The unit is chosen as the spell is played. The cost path cannot see that
// choice, so the [2] discount is granted up front whenever a Bird, Cat, Dog or
// Poro is among the units that can be chosen, and a player who then chooses a
// different unit pays the [2] back as the choice is made (such a unit is only
// offered if they can). Either way the spell costs what the text says.

class UndyingLoyalty : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }

    static bool discounts(const GameObject& obj) {
        return hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
               hasTag(obj, "Dog") || hasTag(obj, "Poro");
    }
    /// "a unit with cost no more than [2] and no more than [A]": at most 2
    /// Energy and at most one Power.
    static bool eligible(const GameObject& obj, const CardDB& db) {
        if (!obj.isUnit() || obj.card_def_id == kInvalidId) return false;
        const auto& d = db.get(obj.card_def_id);
        return d.energy_cost <= 2 && d.power_cost <= 1;
    }
    /// The cost path has no CardDB to judge "can be chosen" with, so the
    /// discount keys on a Bird / Cat / Dog / Poro unit being in the trash and
    /// the surcharge in onResolve settles the difference.
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        for (auto cid : state.player(player).trash) {
            if (!state.objectExists(cid)) continue;
            const auto& obj = state.getObject(cid);
            if (obj.isUnit() && discounts(obj)) return 2;
        }
        return 0;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        const bool discounted = selfCostReduction(ctx.state, ctx.controller) > 0;
        const bool settled = costAlreadyPaid(ctx);
        std::vector<GameObjectId> legal;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (!eligible(obj, ctx.executor.cardDB())) continue;
            // A unit that does not earn the discount costs the [2] back.
            if (!settled && discounted && !discounts(obj) &&
                payableEnergy(ctx.state, ctx.controller) < 2) continue;
            legal.push_back(cid);
        }
        GameObjectId picked = pickTarget(ctx, "Undying Loyalty (unit from trash)", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;  // waiting / none
        // Its controller's base or a battlefield they control (CR 355.2.a).
        const auto where = playLocations(ctx.state, ctx.controller);
        const int mode = pickMode(ctx, "Undying Loyalty: where to play it",
                                  where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;   // waiting for the agent
        if (costDueNow(ctx) && discounted && !discounts(ctx.state.getObject(picked))) {
            if (!payEnergy(ctx, ctx.controller, 2)) return;
            ctx.events.logTrace("UNDYING LOYALTY: not a Bird, Cat, Dog or Poro — pays the [2]");
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        ctx.executor.playIgnoringCost(ctx.controller, picked, where.locations[mode]);
        ctx.events.logTrace("UNDYING LOYALTY: played a unit from trash, ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 730;
        d.def_id = R"RB(unl-168-219)RB";
        d.name = R"RB(Undying Loyalty)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-168/219)RB";
        d.collector_number = 168;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
Play a unit with cost no more than [2] and no more than [A] from your trash, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/703b85f8284ed13865012289d03602e2cd24f4b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_730(CardRegistry& r) {
    r.registerCard(730, std::make_unique<UndyingLoyalty>());
}

} // namespace riftbound
