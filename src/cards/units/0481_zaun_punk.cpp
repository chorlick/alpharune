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

// "You may kill a friendly gear as an additional cost to play me.
//  When you play me, if you paid the additional cost, kill a gear."
//
// OptionalAdditionalCost only models energy/power, not "kill a gear", so we
// fold the optional cost + its payoff into the WhenYouPlayMe trigger: you may
// kill a friendly gear (the cost); if you do, kill any gear (the effect).

class ZaunPunk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "You may kill a friendly gear as an additional cost to play me." An
    // optional additional cost (CR 356.2.b): chosen and paid as I am played,
    // before my cost and before I enter the board. kPaid records it for the
    // play trigger ("if you paid the additional cost").
    static constexpr const char* kPaid = kAdditionalCostPaid;   // forgotten by the engine as each play begins
    static std::vector<GameObjectId> gearOnBoard(const GameState& state, PlayerId controller,
                                                 bool friendly_only) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isGear() || !obj.location.has_value()) continue;
            if (friendly_only && obj.controller != controller) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = !gearOnBoard(state, player, true).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        (void)affordable;
        auto& me = ctx.state.getObject(ctx.source);
        me.card_counters[kPaid] = 0;
        const auto mine = gearOnBoard(ctx.state, ctx.controller, true);
        if (mine.empty() ||
            !ctx.executor.confirmNow(ctx.controller,
                                     "Zaun Punk: kill a friendly gear as an additional cost"))
            return {};
        const GameObjectId gear = ctx.executor.chooseObjectNow(ctx.controller, mine);
        if (gear == kInvalidId) return {};
        ctx.executor.killObject(gear);
        me.card_counters[kPaid] = 1;
        ctx.events.logTrace("ZAUN PUNK: killed a friendly gear (additional cost)");
        return {};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, if you paid the additional cost, kill a gear."
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId, TriggerType,
                          GameObjectId) const override {
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find(kPaid);
        return it != c.end() && it->second == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId gear = pickTarget(ctx, "Zaun Punk: kill a gear",
                                             gearOnBoard(ctx.state, ctx.controller, false));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).card_counters[kPaid] = 0;
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        if (!ctx.state.getObject(gear).location.has_value()) return;
        ctx.executor.killObject(gear);
        ctx.events.logTrace("ZAUN PUNK: paid the additional cost -> killed a gear");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 481;
        d.def_id = R"RB(sfd-160-221)RB";
        d.name = R"RB(Zaun Punk)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-160/221)RB";
        d.collector_number = 160;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(You may kill a friendly gear as an additional cost to play me.
When you play me, if you paid the additional cost, kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af4eec8bb065708bf790b940fa065ea3e735afa0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_481(CardRegistry& r) {
    r.registerCard(481, std::make_unique<ZaunPunk>());
}

} // namespace riftbound
