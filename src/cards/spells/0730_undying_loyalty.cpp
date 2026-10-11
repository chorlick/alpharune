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
// The unit is chosen as the spell is played, before its cost is paid, through
// the additional-play-cost hook (additionalPlayCost / payAdditionalPlayCost):
// only units the spell can then be paid for are offered, and the [2] discount
// is returned only when the unit actually chosen is a Bird, Cat, Dog or Poro.
// The choice is carried to resolution in a card counter (kChosen).

class UndyingLoyalty : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }

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
    // "This costs [2] less if you choose a Bird, Cat, Dog, or Poro." The
    // unit is chosen as the spell is played, before its cost is determined
    // (CR 355 -> 356), through the pre-payment hook — which sees the CardDB,
    // so only units the text allows are on offer and the discount follows the
    // unit actually chosen. kChosen carries the choice to resolution.
    static constexpr const char* kChosen = "__undying_loyalty_unit";
    static std::vector<GameObjectId> choosable(const GameState& state, const CardDB& db,
                                               PlayerId player) {
        std::vector<GameObjectId> out;
        for (auto cid : state.player(player).trash)
            if (state.objectExists(cid) && eligible(state.getObject(cid), db))
                out.push_back(cid);
        std::sort(out.begin(), out.end());
        return out;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId /*self*/) const override {
        AdditionalPlayCost c;
        c.exists = true;
        c.mandatory = true;          // the choice is part of playing the spell
        for (auto cid : choosable(state, db, player)) {
            c.payable = true;
            if (discounts(state.getObject(cid))) c.best.energy_reduction = 2;
        }
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        PlayCostAdjust discount;
        discount.energy_reduction = 2;
        // Only units the spell can still be paid for with.
        std::vector<GameObjectId> legal;
        for (auto cid : choosable(ctx.state, ctx.executor.cardDB(), ctx.controller))
            if (affordable(discounts(ctx.state.getObject(cid)) ? discount : PlayCostAdjust{}))
                legal.push_back(cid);
        auto& me = ctx.state.getObject(ctx.source);
        me.card_counters[kChosen] = 0;
        const GameObjectId picked = ctx.executor.chooseObjectNow(ctx.controller, legal);
        if (picked == kInvalidId) return {};
        me.card_counters[kChosen] = static_cast<int>(picked) + 1;
        ctx.events.logTrace("UNDYING LOYALTY: chose " + ctx.state.getObject(picked).name);
        return discounts(ctx.state.getObject(picked)) ? discount : PlayCostAdjust{};
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        // Whether a unit of the right cost is there is judged by the play
        // gate (additionalPlayCost, which has the CardDB); here: any unit.
        for (auto cid : state.player(controller).trash)
            if (state.objectExists(cid) && state.getObject(cid).isUnit()) return true;
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // The unit chosen as the spell was played; it must still be in the
        // trash and still what the text allows.
        const auto& counters = ctx.state.getObject(ctx.source).card_counters;
        const auto it = counters.find(kChosen);
        const GameObjectId picked = (it == counters.end() || it->second <= 0)
            ? kInvalidId : static_cast<GameObjectId>(it->second - 1);
        const bool still_there = picked != kInvalidId && ctx.state.objectExists(picked) &&
            std::find(ps.trash.begin(), ps.trash.end(), picked) != ps.trash.end() &&
            eligible(ctx.state.getObject(picked), ctx.executor.cardDB());
        // Its controller's base or a battlefield they control (CR 355.2.a).
        const auto where = playLocations(ctx.state, ctx.controller);
        // (playLocations leaves out a battlefield units can't be played to.)
        const int mode = pickMode(ctx, "Undying Loyalty: where to play it",
                                  where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (!still_there) return;
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
