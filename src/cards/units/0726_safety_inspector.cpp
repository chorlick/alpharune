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

// "You may spend 3 XP as an additional cost to play me.
//  When you play me, each player must kill one of their units. If you paid my
//  additional cost, you don't kill a unit this way."
//
// The 3 XP is an optional additional cost (CR 356.2.b): chosen and paid as I
// am played, before my cost and before I enter the board. kPaid records it for
// the play trigger ("If you paid my additional cost").
// Each player chooses among their own units as the ability resolves (nothing
// is targeted, CR 355.10.e) — the controller first, then the opponent — and
// the chosen units die together.

class SafetyInspector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    static constexpr const char* kPaid = kAdditionalCostPaid;   // forgotten by the engine as each play begins
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.payable = state.player(player).xp >= 3;
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        (void)affordable;   // it takes nothing off my cost
        auto& me = ctx.state.getObject(ctx.source);
        me.card_counters[kPaid] = 0;
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.xp < 3 ||
            !ctx.executor.confirmNow(ctx.controller,
                                     "Safety Inspector: spend 3 XP to skip your kill"))
            return {};
        ps.xp -= 3;
        me.card_counters[kPaid] = 1;
        RIFT_TRACE(ctx.events, "SAFETY INSPECTOR: paid 3 XP -> skip own kill");
        return {};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming;
        // Whether the additional cost was paid is read once, as the ability
        // starts resolving, and kept on the chain item for the re-entries
        // (1 = paid, 2 = not paid); the flag on me is cleared so that a later
        // play of the same card starts clean.
        auto take_paid_flag = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            auto& counters = ctx.state.getObject(ctx.source).card_counters;
            const auto it = counters.find(kPaid);
            const bool paid = it != counters.end() && it->second == 1;
            counters[kPaid] = 0;
            return paid;
        };
        bool paid_xp = false;
        if (ri.has_value()) {
            while (ri->resume_data.size() <= kPaidSlot) ri->resume_data.push_back(0);
            if (ri->resume_data[kPaidSlot] == 0)
                ri->resume_data[kPaidSlot] = take_paid_flag() ? 1 : 2;
            paid_xp = ri->resume_data[kPaidSlot] == 1;
        } else {
            paid_xp = take_paid_flag();   // direct invocation: no item to resume
        }

        auto units_of = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard())
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            return out;
        };
        const PlayerId opp = opponent(ctx.controller);
        GameObjectId mine = kInvalidId;
        if (!paid_xp) {
            // At resolution: each player chooses (CR 355.10.e; batch M1).
            auto pick = selectObjectAs(ctx, ctx.controller,
                                       "Safety Inspector: kill one of your units",
                                       units_of(ctx.controller), kOwnPickSlot);
            if (!pick.has_value()) return;   // waiting on the controller
            mine = *pick;
        }
        // At resolution: each player chooses (CR 355.10.e; batch M1).
        auto theirs = selectObjectAs(ctx, opp, "Safety Inspector: kill one of your units",
                                     units_of(opp), kOppPickSlot);
        if (!theirs.has_value()) return;     // waiting on the opponent
        auto still_theirs = [&](GameObjectId id, PlayerId p) {
            if (id == kInvalidId || !ctx.state.objectExists(id)) return false;
            const auto& o = ctx.state.getObject(id);
            return o.isUnit() && o.location.has_value() && o.controller == p;
        };
        // The two units die at the same time (CR 370.1.a.2; testing phase R3):
        // one death batch, so neither sees the other die (CR 383.2.c.2).
        std::vector<GameObjectId> doomed;
        if (still_theirs(mine, ctx.controller)) doomed.push_back(mine);
        if (still_theirs(*theirs, opp)) doomed.push_back(*theirs);
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, doomed);   // they die at the same time
            for (auto id : doomed) ctx.executor.killObject(id);
        }
        RIFT_TRACE(ctx.events, "SAFETY INSPECTOR: each player killed one of their units");
    }
private:
    // resume_data slots (0..6 belong to the Card::pick* helpers).
    static constexpr size_t kOwnPickSlot = 7;
    static constexpr size_t kOppPickSlot = 9;
    static constexpr size_t kPaidSlot = 11;
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 726;
        d.def_id = R"RB(unl-164-219)RB";
        d.name = R"RB(Safety Inspector)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-164/219)RB";
        d.collector_number = 164;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(You may spend 3 XP as an additional cost to play me.
When you play me, each player must kill one of their units. If you paid my additional cost, you don't kill a unit this way.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/499e65eb2d35e2bd774ea7fe2a70672234f71e90-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_726(CardRegistry& r) {
    r.registerCard(726, std::make_unique<SafetyInspector>());
}

} // namespace riftbound
