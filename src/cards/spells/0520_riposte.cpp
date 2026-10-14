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

// "[Reaction] Choose a friendly unit and a spell. Counter that spell and give
// that unit +[M] equal to that spell's Energy cost this turn."

class Riposte : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit and a spell. Counter that spell and give that
    // unit +[M] equal to that spell's Energy cost this turn." Both are targets
    // chosen as Riposte is played (CR 355.5, 355.9.a.2): the unit first, then
    // any finalized spell on the chain.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    static bool qualifies(const ChainItem& ci) { return ci.is_spell; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return anyChainItem(state, qualifies) && !enumerateLegalTargets(state, controller).empty();
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        return chooseableUnits(state, controller,
                               [&](const GameObject& o) { return o.controller == controller; });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId unit = pickTarget(ctx, "Riposte: choose a friendly unit",
                                             enumerateLegalTargets(ctx.state, ctx.controller));
        if (unit == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        if (unit == kInvalidId && finalizingChoices(ctx)) return;   // nothing to choose
        const auto spell = pickChainItem(ctx, "Riposte: choose a spell",
                                         chainItemChoices(ctx, qualifies));
        if (!spell.has_value()) return;
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        // The countered spell's Energy cost, read before it leaves the chain.
        int cost = 0;
        if (const ChainItem* item = *spell != kInvalidId ? chainItem(ctx.state, *spell) : nullptr)
            if (item->card_def_id != kInvalidId)
                cost = ctx.executor.cardDB().get(item->card_def_id).energy_cost;
        if (*spell != kInvalidId) counterChosenItem(ctx, *spell);
        // A unit that left the board only loses its own part (CR 359.3.e.5).
        if (unit != kInvalidId && ctx.state.objectExists(unit) &&
            ctx.state.getObject(unit).location.has_value() && cost > 0) {
            ctx.executor.giveTemporaryMight(unit, cost);
            RIFT_TRACE(ctx.events, "RIPOSTE: +" + std::to_string(cost) + " [M] this turn");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 520;
        d.def_id = R"RB(sfd-206-221)RB";
        d.name = R"RB(Riposte)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-206/221)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Fiora)RB"};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit and a spell. Counter that spell and give that unit +[M] equal to that spell's Energy cost this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/07af52eada661904b467ca118c2715435f0a3b00-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_520(CardRegistry& r) {
    r.registerCard(520, std::make_unique<Riposte>());
}

} // namespace riftbound
