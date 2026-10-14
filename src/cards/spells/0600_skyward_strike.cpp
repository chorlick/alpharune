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
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }
    static std::vector<GameObjectId> enemies(const GameState& state, PlayerId me) {
        return chooseableUnits(state, me,
                               [me](const GameObject& o) { return o.controller != me; });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !enemies(state, controller).empty();
    }
    static constexpr size_t kLevelSlot = Card::kChoiceStateSlot + 3;
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const bool level6 = ctx.state.player(ctx.controller).xp >= 6;
        const auto foes = enemies(ctx.state, ctx.controller);
        // Two instructions, each with its own target: the unit to move, and
        // — at Level 6 — the unit to stun (any enemy unit, the same one too).
        const GameObjectId mover = pickTarget(ctx, "Skyward Strike: move an enemy unit", foes);
        if (mover == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        // Whether a unit to stun is chosen is fixed as the spell is played
        // (the choices are made then, CR 355.5); the stun itself needs Level 6
        // as it resolves.
        bool choose_stun = level6;
        if (ctx.state.chain.resuming.has_value()) {
            auto& rd = ctx.state.chain.resuming->resume_data;
            if (rd.size() <= kLevelSlot) rd.resize(kLevelSlot + 1, 0);
            if (rd[kLevelSlot] == 0) rd[kLevelSlot] = level6 ? 2 : 1;
            choose_stun = rd[kLevelSlot] == 2;
        }
        GameObjectId stunned = kInvalidId;
        if (choose_stun) {
            const auto s = selectTargetOrNone(*this, ctx, "Skyward Strike: stun an enemy unit",
                                              foes, Card::kChoiceStateSlot + 1,
                                              /*allow_none=*/false);
            if (!s.has_value()) return;   // waiting for the agent
            stunned = *s;
        }
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
            RIFT_TRACE(ctx.events, "SKYWARD STRIKE: moved an enemy unit");
        }
        if (level6 && stunned != kInvalidId && ctx.state.objectExists(stunned)) {
            ctx.executor.stunUnitBy(stunned, ctx.source);
            RIFT_TRACE(ctx.events, "SKYWARD STRIKE: Level 6 -> stun");
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
