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

class PiercingLight : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [2][R]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 2, .power = 1, .power_domain = Domain::Fury}};
    }
    // "Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit."
    // Two targets of different kinds (testing phase U8 / U9): the first is
    // required and must be at a battlefield — the play-time target; the
    // second is "up to one other unit" (any location, CR 355.13), chosen in
    // the finalize pass with the option of none. Both are chosen as the spell
    // is played (before its cost), and each [Repeat] execution chooses them
    // again the same way (CR 820.2: its choices follow the card's own
    // requirements — the finalize pass, not the engine's generic re-choice,
    // which offered base units first and could not leave the second out). A
    // play by an effect asks them the same way (no "No target" for the first).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // Either unit may be a friendly one (a discount for choosing it counts).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static std::vector<GameObjectId> atBattlefields(const GameState& state, PlayerId controller) {
        return chooseableUnits(state, controller,
                               [](const GameObject& u) { return u.isAtBattlefield(); });
    }
    static std::vector<GameObjectId> others(const GameState& state, PlayerId controller,
                                            GameObjectId first) {
        return chooseableUnits(state, controller,
                               [first](const GameObject& u) { return u.id != first; });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId first = pickTarget(ctx, "Piercing Light: deal 2 to a unit at a battlefield",
                                              atBattlefields(ctx.state, ctx.controller));
        if (first == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        if (first == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->finalizing)
            return;   // no legal first unit: nothing to decide (pickTarget stored it)
        const auto second = selectTargetOrNone(*this, ctx,
            "Piercing Light: deal 2 to up to one other unit",
            others(ctx.state, ctx.controller, first), 7);
        if (!second.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;       // choices end here (CR 355)
        if (first != kInvalidId) ctx.executor.dealDamage(first, 2, ctx.source);
        if (*second != kInvalidId && *second != first &&
            ctx.state.objectExists(*second) && ctx.state.getObject(*second).location.has_value())
            ctx.executor.dealDamage(*second, 2, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 346;
        d.def_id = R"RB(sfd-023-221)RB";
        d.name = R"RB(Piercing Light)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-023/221)RB";
        d.collector_number = 23;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2][R] (You may pay the additional cost to repeat this spell's effect.)
Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7fb3283b11fb8e5b5b08f9ead9b98c695b75bff0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_346(CardRegistry& r) {
    r.registerCard(346, std::make_unique<PiercingLight>());
}

} // namespace riftbound
