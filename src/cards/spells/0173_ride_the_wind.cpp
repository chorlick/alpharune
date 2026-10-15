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

class RideTheWind : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move a friendly unit and ready it." The unit (CR 355.5) and where it
    // moves (CR 355.4) are chosen as the spell is played, before its cost and
    // before anyone can respond (testing phase D3).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object, [&](const GameObject& u) {
            return !moveDests(state, u).empty();
        });
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal;
        for (auto id : enumerateLegalTargets(ctx.state, ctx.controller))
            if (!moveDests(ctx.state, ctx.state.getObject(id)).empty()) legal.push_back(id);
        const GameObjectId picked = pickTarget(ctx, "Ride the Wind", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;                          // waiting for the agent
        int dest = -2;
        if (picked != kInvalidId && ctx.state.getObject(picked).location.has_value()) {
            // "Move a friendly unit" — to wherever its controller chooses: its
            // base or any battlefield other than where it is.
            dest = pickMoveDest(*this, ctx, "Ride the Wind: move it to", ctx.state.getObject(picked));
            if (dest == -1) return;          // waiting for the destination
        }
        if (finalizingChoices(ctx)) return;  // choices end here (CR 355)
        if (picked == kInvalidId) return;    // nothing chosen, or a mistarget
        const auto& unit = ctx.state.getObject(picked);
        if (!unit.isUnit() || !unit.location.has_value()) return;
        if (dest >= 0) moveTo(ctx, picked, dest);
        ctx.executor.readyObject(picked);
        RIFT_TRACE(ctx.events, "RIDE THE WIND: moved + readied " +
                             ctx.state.getObject(picked).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 173;
        d.def_id = R"RB(ogn-173-298)RB";
        d.name = R"RB(Ride the Wind)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-173/298)RB";
        d.collector_number = 173;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Move a friendly unit and ready it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/864d9f8992db7f9a9e795d9438d941ca564ddde7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_173(CardRegistry& r) {
    r.registerCard(173, std::make_unique<RideTheWind>());
}

} // namespace riftbound
