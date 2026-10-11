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

class ElderDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Any amount of your damage is enough to kill enemy units." (CR 142.4.c)
    bool anyDamageIsLethal() const override { return true; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto opp = opponent(ctx.controller);
        // "at each location": the opponent's base and every battlefield
        // (CR 151 / 144.4 for location vs zone) — the battlefields there are
        // as the ability is finalized (CR 383.3). How many that is is fixed on
        // the first entry and kept on the chain item
        // (resume_data[kLocationsSlot] = that number + 1; 0 = not set yet), so
        // a battlefield added before this resolves (Baron Nashor's Pit) gets
        // no prompt then: nothing was chosen there. Battlefields are only
        // ever appended to state.battlefields.
        constexpr size_t kLocationsSlot = Card::kChoiceStateSlot + 1;
        size_t battlefields = ctx.state.battlefields.size();
        if (auto& ri = ctx.state.chain.resuming; ri.has_value()) {
            auto& rd = ri->resume_data;
            if (rd.size() <= kLocationsSlot) rd.resize(kLocationsSlot + 1, 0);
            if (rd[kLocationsSlot] == 0)
                rd[kLocationsSlot] = static_cast<int32_t>(battlefields) + 1;
            battlefields = std::min(battlefields, static_cast<size_t>(rd[kLocationsSlot] - 1));
        }
        std::vector<LocationId> locations;
        locations.reserve(battlefields + 1);
        locations.push_back(BaseLocation{opp});
        for (size_t i = 0; i < battlefields; ++i)
            locations.push_back(BattlefieldLocation{ctx.state.battlefields[i].id});

        // "choose up to one enemy unit at each location": one prompt per
        // location that has a choice, each with a "None".
        // [Deflect] (CR 809) is paid for every unit chosen, and all of it
        // once the last pick is in: a unit is offered only if its cost can
        // be paid on top of what the picks so far already owe. This is
        // Card::offerableChoices with a running total, so like it (and like
        // commitChosenObject, which pays) it asks ignoresDeflect() — false
        // for this card, whose text does not say so.
        std::vector<GameObjectId> chosen;
        const bool taxed = !ignoresDeflect();
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        int owed = 0;
        size_t slot = kLocationsSlot + 1;   // two per location (selectObject)
        for (const auto& loc : locations) {
            const auto here = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == opp && *u.location == loc &&
                       (!taxed ||
                        ctx.executor.deflectCostFor(ctx.controller, u.id) <= payable - owed);
            });
            const auto pick = selectObject(ctx, "Elder Dragon: choose up to one enemy unit here",
                                           here, slot, /*may_decline=*/true);
            slot += 2;
            if (!pick.has_value()) return;     // waiting for the agent
            if (*pick == kInvalidId) continue;
            chosen.push_back(*pick);
            if (taxed) owed += ctx.executor.deflectCostFor(ctx.controller, *pick);
        }
        // Choosing is what [Deflect] taxes and what "chosen" abilities see:
        // done once, with the choices (CR 355).
        if (costDueNow(ctx))
            for (auto id : chosen) commitChosenObject(ctx, id);
        if (finalizingChoices(ctx)) return;    // choices and costs end here (CR 383.3)

        // "Deal 1 to them." All the damage first, then the deaths: a unit
        // killed mid-loop would put its death trigger ahead of the rest.
        std::vector<GameObjectId> hit;
        for (auto id : chosen) {
            if (!ctx.state.objectExists(id)) continue;
            const auto& u = ctx.state.getObject(id);
            if (!u.isUnit() || !u.location.has_value() || u.controller != opp) continue;
            ctx.executor.dealDamage(id, 1, ctx.source);
            hit.push_back(id);
        }
        // The units this damage kills die together, here: the ones it is
        // lethal for and — "any amount of your damage is enough to kill" —
        // every other one it marked (EffectExecutor::damageKills, the test
        // the cleanup uses too).
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(hit);   // they die at the same time
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 680;
        d.def_id = R"RB(unl-118-219)RB";
        d.name = R"RB(Elder Dragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-118/219)RB";
        d.collector_number = 118;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Demacia)RB"};
        d.energy_cost = 12;
        d.power_cost = 4;
        d.might = 10;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Any amount of your damage is enough to kill enemy units.
When you play me, choose up to one enemy unit at each location. Deal 1 to them.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fcf578e447d4e2785c8b25f3c39928b98e7b44ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_680(CardRegistry& r) {
    r.registerCard(680, std::make_unique<ElderDragon>());
}

} // namespace riftbound
