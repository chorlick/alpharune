#include "cards/card.h"
#include "cards/card_helpers.h"
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

class Rampage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    // "As you play this, you may pay [O] as an additional cost. Choose a
    //  friendly unit and an enemy unit. If you paid the additional cost, give
    //  the friendly unit +2 [M] this turn. They deal damage equal to their
    //  Mights to each other."
    static constexpr const char* kPaidFlag = "__rampage_paid";
    OptionalAdditionalCost optionalAdditionalCost() const override {
        OptionalAdditionalCost c;
        c.valid = true;
        c.power = 1;
        c.power_domain = Domain::Body;
        c.paid_flag = kPaidFlag;
        return c;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    static std::vector<GameObjectId> units(const GameState& state, PlayerId me, bool friendly) {
        return chooseableUnits(state, me, [me, friendly](const GameObject& o) {
            return (o.controller == me) == friendly;
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !units(state, controller, true).empty() &&
               !units(state, controller, false).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto [friendly, enemy] = pickTargetPair(
            ctx, "Rampage", units(ctx.state, me, true),
            [&](GameObjectId) { return units(ctx.state, me, false); });
            if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if ((friendly == kInvalidId || enemy == kInvalidId) &&
            ctx.state.chain.resuming.has_value() &&
            (ctx.state.chain.resuming->resume_point == 10 ||
             ctx.state.chain.resuming->resume_point == 12)) return;  // waiting for the agent

        bool paid = false;
        if (ctx.state.objectExists(ctx.source)) {
            auto& counters = ctx.state.getObject(ctx.source).card_counters;
            auto it = counters.find(kPaidFlag);
            paid = it != counters.end() && it->second == 1;
            if (it != counters.end()) counters.erase(it);  // one play, one payment
        }
        const bool has_friendly = friendly != kInvalidId && ctx.state.objectExists(friendly);
        const bool has_enemy = enemy != kInvalidId && ctx.state.objectExists(enemy);
        if (paid && has_friendly) ctx.executor.giveTemporaryMight(friendly, 2);
        if (!has_friendly || !has_enemy) return;

        // Snapshot both Mights (after the +2) — the damage is simultaneous.
        const int friendly_might = ctx.state.getObject(friendly).current_might;
        const int enemy_might = ctx.state.getObject(enemy).current_might;
        ctx.executor.dealDamage(enemy, friendly_might, ctx.source);
        ctx.executor.dealDamage(friendly, enemy_might, ctx.source);
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, std::vector<GameObjectId>{enemy, friendly}, DeathBatch::LethalOnly);   // they die at the same time
            for (auto id : {enemy, friendly})
                if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                    ctx.executor.killObject(id);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 870;
        d.def_id = R"RB(ven-083-166)RB";
        d.name = R"RB(Rampage)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-083/166)RB";
        d.collector_number = 83;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(As you play this, you may pay [O] as an additional cost.
Choose a friendly unit and an enemy unit. If you paid the additional cost, give the friendly unit +2 [M] this turn. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c85ae2c6bd5b2644651564fad0f86ad19c5709a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_870(CardRegistry& r) {
    r.registerCard(870, std::make_unique<Rampage>());
}

}  // namespace riftbound
