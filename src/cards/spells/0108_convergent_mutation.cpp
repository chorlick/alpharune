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

class ConvergentMutation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a friendly unit. This turn, increase its Might to the Might of
    // another friendly unit." Both units are targets (CR 355.5, 355.10): the
    // first is named in the play intent, the other is chosen in the finalize
    // pass, before the cost and before anyone can respond. A spell needs a
    // valid choice for every target to be played (CR 355.8): two friendly
    // units.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return friendlyUnits(state, controller, kInvalidId).size() >= 2;
    }
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId controller,
                                                   GameObjectId except) {
        return chooseableUnits(state, controller, [&](const GameObject& u) {
            return u.controller == controller && u.id != except;
        });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const GameObjectId chosen = targets.empty() ? kInvalidId : targets[0];
        const auto other = selectTargetOrNone(*this, ctx, "Convergent Mutation (copy Might from)",
                                              friendlyUnits(ctx.state, ctx.controller, chosen),
                                              7, /*allow_none=*/false);
        if (!other.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        // Either target no longer legal: that instruction is ignored (CR
        // 359.3.e.5), and it has nothing else to do.
        if (chosen == kInvalidId || *other == kInvalidId) return;
        if (!ctx.state.objectExists(chosen) || !ctx.state.objectExists(*other)) return;
        const int target_might = ctx.state.getObject(*other).current_might;
        const int cur = ctx.state.getObject(chosen).current_might;
        const int delta = target_might - cur;
        if (delta > 0) ctx.executor.giveTemporaryMight(chosen, delta);
        RIFT_TRACE(ctx.events, "CONVERGENT MUTATION: set Might to " +
                             std::to_string(target_might));
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 108;
        d.def_id = R"RB(ogn-108-298)RB";
        d.name = R"RB(Convergent Mutation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-108/298)RB";
        d.collector_number = 108;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit. This turn, increase its Might to the Might of another friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/61d414fc67ff52b9082e282e628dd0916afd8454-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_108(CardRegistry& r) {
    r.registerCard(108, std::make_unique<ConvergentMutation>());
}

} // namespace riftbound
