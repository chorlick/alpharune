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

class SkywardStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move an enemy unit.
    //  [Level 6][>] [Stun] an enemy unit."
    // Two instructions, each with its own target (the second is not "instead"):
    // the unit to move, where it moves, and — with 6+ XP — the unit to stun,
    // which may be the same unit or another. All chosen as the spell is played.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    static std::vector<GameObjectId> enemies(const GameState& state, PlayerId me) {
        return chooseableUnits(state, me,
                               [me](const GameObject& o) { return o.controller != me; });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !enemies(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const bool level6 = ctx.state.player(ctx.controller).xp >= 6;
        const auto foes = enemies(ctx.state, ctx.controller);
        // Always the pair helper, so the stored answers mean the same thing
        // whatever the XP is when this is re-entered: below Level 6 there is
        // simply no second target to choose.
        auto [mover, stunned] = pickTargetPair(ctx, "Skyward Strike", foes,
            [&](GameObjectId) { return level6 ? foes : std::vector<GameObjectId>{}; });
        if (ctx.state.chain.resuming.has_value() &&
            (ctx.state.chain.resuming->resume_point == 10 ||
             ctx.state.chain.resuming->resume_point == 12)) return;   // waiting for the agent
        int dest = -2;
        if (mover != kInvalidId && ctx.state.objectExists(mover)) {
            const auto dests = moveDests(ctx.state, ctx.state.getObject(mover));
            const int n = 1 + static_cast<int>(ctx.state.battlefields.size());
            std::vector<std::string> labels;
            uint32_t mask = 0;
            for (int d = 0; d < n; ++d) labels.push_back(destLabel(d));
            for (int d : dests) if (d >= 0 && d < 32) mask |= (1u << d);
            dest = pickMode(ctx, "Skyward Strike: move it where?", n, labels, mask);
            if (dest == -1) return;   // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mover != kInvalidId && dest >= 0) {
            moveTo(ctx, mover, dest);
            ctx.events.logTrace("SKYWARD STRIKE: moved an enemy unit");
        }
        if (level6 && stunned != kInvalidId && ctx.state.objectExists(stunned)) {
            ctx.executor.stunUnitBy(stunned, ctx.source);
            ctx.events.logTrace("SKYWARD STRIKE: Level 6 -> stun");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 600;
        d.def_id = R"RB(unl-038-219)RB";
        d.name = R"RB(Skyward Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-038/219)RB";
        d.collector_number = 38;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB(Move an enemy unit.
[Level 6][>] [Stun] an enemy unit. (While you have 6+ XP, get the effect. A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/998740e590fec024f204ad26ed7e2dad2a0c8d66-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_600(CardRegistry& r) {
    r.registerCard(600, std::make_unique<SkywardStrike>());
}

} // namespace riftbound
