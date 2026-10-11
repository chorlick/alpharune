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
        // (CR 151 / 144.4 for location vs zone).
        std::vector<LocationId> locations;
        locations.reserve(ctx.state.battlefields.size() + 1);
        locations.push_back(BaseLocation{opp});
        for (auto& bf : ctx.state.battlefields)
            locations.push_back(BattlefieldLocation{bf.id});

        // "choose up to one enemy unit at each location": one prompt per
        // location that has a choice, each with a "None".
        // [Deflect] (CR 809) is paid for every unit chosen, and all of it
        // once the last pick is in: a unit is offered only if its cost can
        // be paid on top of what the picks so far already owe.
        std::vector<GameObjectId> chosen;
        const int payable = ctx.executor.payableAnyPower(ctx.controller);
        int owed = 0;
        size_t slot = 7;
        for (const auto& loc : locations) {
            const auto here = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller == opp && *u.location == loc &&
                       ctx.executor.deflectCostFor(ctx.controller, u.id) <= payable - owed;
            });
            const auto pick = selectObject(ctx, "Elder Dragon: choose up to one enemy unit here",
                                           here, slot, /*may_decline=*/true);
            slot += 2;
            if (!pick.has_value()) return;     // waiting for the agent
            if (*pick == kInvalidId) continue;
            chosen.push_back(*pick);
            owed += ctx.executor.deflectCostFor(ctx.controller, *pick);
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
        // Units this 1 damage is lethal for by itself. The rest die in the
        // cleanup that follows, where "any amount of your damage is enough"
        // applies (GameEngine::processLethalDamage).
        DeathBatch together(ctx.executor, hit);   // they die at the same time
        for (auto id : hit)
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
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
