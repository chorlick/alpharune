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

class Atakhan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ([Ganking] engine-handled.)
    //
    // "You may kill a friendly unit as an additional cost to play me. If you
    //  do, I cost [1] less for each Energy it costs and [Y] less for each Power
    //  it costs." An additional cost chosen before payment (CR 356.2.b); the
    //  killed unit's printed cost comes from the CardDB (a token costs nothing).
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == player && obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    static PlayCostAdjust worth(const GameState& state, const CardDB& db, GameObjectId unit) {
        PlayCostAdjust adj;
        const auto& obj = state.getObject(unit);
        if (obj.card_def_id == kInvalidId || obj.isToken()) return adj;
        const auto& d = db.get(obj.card_def_id);
        adj.energy_reduction = d.energy_cost;
        adj.power_reduction = d.power_cost;
        return adj;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)self;
        const auto units = friendlyUnits(state, player);
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = !units.empty();
        // Energy matters most for a [10] card; ties go to the Power.
        for (auto id : units) {
            const auto w = worth(state, db, id);
            if (w.energy_reduction > c.best.energy_reduction ||
                (w.energy_reduction == c.best.energy_reduction &&
                 w.power_reduction > c.best.power_reduction)) c.best = w;
        }
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        // Only units whose cost leaves me payable can be chosen when the
        // full cost is out of reach.
        const bool may_decline = affordable(PlayCostAdjust{});
        std::vector<GameObjectId> units;
        for (auto id : friendlyUnits(ctx.state, ctx.controller))
            if (may_decline || affordable(worth(ctx.state, ctx.executor.cardDB(), id)))
                units.push_back(id);
        if (units.empty()) return {};
        if (may_decline &&
            !ctx.executor.confirmNow(ctx.controller,
                                     "Atakhan: kill a friendly unit to reduce my cost by its cost"))
            return {};
        const GameObjectId unit = ctx.executor.chooseObjectNow(ctx.controller, units);
        if (unit == kInvalidId) return {};
        const PlayCostAdjust adj = worth(ctx.state, ctx.executor.cardDB(), unit);
        ctx.executor.killObject(unit);
        RIFT_TRACE(ctx.events, "ATAKHAN: killed a friendly unit -> costs [" +
                            std::to_string(adj.energy_reduction) + "] and " +
                            std::to_string(adj.power_reduction) + " Power less");
        return adj;
    }
    //
    // "When I attack, the defender must kill one of their units here." The
    // defender chooses which (nothing is targeted, CR 355.10.e).
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        const PlayerId defender = opponent(ctx.controller);
        std::vector<GameObjectId> theirs_here;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != defender) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf) theirs_here.push_back(id);   // "here"
        }
        auto victim = selectObjectAs(ctx, defender, "Atakhan: kill one of your units here",
                                     theirs_here);
        if (!victim.has_value()) return;     // waiting on the defender
        if (*victim == kInvalidId || !ctx.state.objectExists(*victim)) return;
        const auto& v = ctx.state.getObject(*victim);
        if (!v.isUnit() || v.controller != defender || v.battlefieldId() != my_bf) return;
        ctx.executor.killObject(*victim);
        RIFT_TRACE(ctx.events, "ATAKHAN: attack -> defender killed one of their units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 732;
        d.def_id = R"RB(unl-170-219)RB";
        d.name = R"RB(Atakhan)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-170/219)RB";
        d.collector_number = 170;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB", R"RB(Demon)RB"};
        d.energy_cost = 10;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(You may kill a friendly unit as an additional cost to play me. If you do, I cost [1] less for each Energy it costs and [Y] less for each Power it costs.
[Ganking] (I can move from battlefield to battlefield.)
When I attack, the defender must kill one of their units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0b3994aad6b6c64ace14193c1363713e6b6ede2-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_732(CardRegistry& r) {
    r.registerCard(732, std::make_unique<Atakhan>());
}

} // namespace riftbound
