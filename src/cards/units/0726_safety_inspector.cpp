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
// Each player chooses among their own units as the ability resolves (nothing
// is targeted, CR 355.10.e) — the controller first, then the opponent — and
// the chosen units die together.
// ENGINE-GAP: the 3 XP is an additional cost of PLAYING the card, but
// Card::OptionalAdditionalCost has no XP cost. It is asked and paid when the
// play trigger is finalized (before anyone can respond), which is as close as
// a card can get.

class SafetyInspector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        auto& ri = ctx.state.chain.resuming;
        // "Paid" is remembered on the chain item so the XP is spent once,
        // however often the ability is re-entered.
        bool paid_xp = ri.has_value() && ri->resume_data.size() > kPaidSlot &&
                       ri->resume_data[kPaidSlot] == 1;

        int pay = confirmOptional(ctx, "Safety Inspector: spend 3 XP to skip your kill?",
            [&]() { return paid_xp || ps.xp >= 3; });
        if (pay == -1) return;  // waiting on agent
        if (pay == 1 && !paid_xp && costDueNow(ctx) && ps.xp >= 3) {
            ps.xp -= 3;
            paid_xp = true;
            if (ri.has_value()) {
                while (ri->resume_data.size() <= kPaidSlot) ri->resume_data.push_back(0);
                ri->resume_data[kPaidSlot] = 1;
            }
            ctx.events.logTrace("SAFETY INSPECTOR: paid 3 XP -> skip own kill");
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)

        auto units_of = [&](PlayerId p) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == p && obj.location.has_value())
                    out.push_back(id);
            return out;
        };
        const PlayerId opp = opponent(ctx.controller);
        GameObjectId mine = kInvalidId;
        if (!paid_xp) {
            auto pick = selectObjectAs(ctx, ctx.controller,
                                       "Safety Inspector: kill one of your units",
                                       units_of(ctx.controller), kOwnPickSlot);
            if (!pick.has_value()) return;   // waiting on the controller
            mine = *pick;
        }
        auto theirs = selectObjectAs(ctx, opp, "Safety Inspector: kill one of your units",
                                     units_of(opp), kOppPickSlot);
        if (!theirs.has_value()) return;     // waiting on the opponent
        auto still_theirs = [&](GameObjectId id, PlayerId p) {
            if (id == kInvalidId || !ctx.state.objectExists(id)) return false;
            const auto& o = ctx.state.getObject(id);
            return o.isUnit() && o.location.has_value() && o.controller == p;
        };
        const bool kill_mine = still_theirs(mine, ctx.controller);
        const bool kill_theirs = still_theirs(*theirs, opp);
        if (kill_mine) ctx.executor.killObject(mine);
        if (kill_theirs) ctx.executor.killObject(*theirs);
        ctx.events.logTrace("SAFETY INSPECTOR: each player killed one of their units");
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
