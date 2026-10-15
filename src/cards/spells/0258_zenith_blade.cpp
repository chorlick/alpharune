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

class ZenithBlade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    // "Stun an enemy unit at a battlefield. You may move a friendly unit to
    // that enemy unit's battlefield." Both units are targets chosen as the
    // spell is played: the enemy as its play-time target, the friendly unit
    // in its finalize pass — "may perform an action on objects": the objects
    // are targets, chosen independently of the decision (CR 355.12). The
    // "may" is the spell's, decided as it resolves (CR 355.12, 355.18).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static std::vector<GameObjectId> enemiesAtBattlefields(const GameState& state,
                                                           PlayerId controller) {
        return chooseableUnits(state, controller, [&](const GameObject& u) {
            return u.controller != controller && u.isAtBattlefield();
        });
    }
    /// Friendly units that could move to `bf`: on the board, not already there.
    static std::vector<GameObjectId> movers(const GameState& state, PlayerId controller,
                                            std::optional<BattlefieldId> bf) {
        if (!bf.has_value()) return {};
        return chooseableUnits(state, controller, [&](const GameObject& u) {
            return u.controller == controller && u.battlefieldId() != bf;
        });
    }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId stunned = pickTarget(ctx, "Zenith Blade: stun enemy unit",
                                                enemiesAtBattlefields(ctx.state, ctx.controller));
        if (stunned == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        // "that enemy unit's battlefield": where it is now (none once it is no
        // longer a legal target: a mistarget, CR 359.3.e.12).
        std::optional<BattlefieldId> dest;
        if (stunned != kInvalidId && ctx.state.objectExists(stunned))
            dest = ctx.state.getObject(stunned).battlefieldId();
        // Chosen when there is one (it is not optional, CR 355.12); with
        // none the spell still stuns — the "may" has nothing to act on.
        const auto candidates = movers(ctx.state, ctx.controller, dest);
        const auto mover = selectTargetOrNone(*this, ctx,
            "Zenith Blade: friendly unit that may move to that battlefield",
            candidates, 7, /*allow_none=*/candidates.empty());
        if (!mover.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 355)
        if (stunned == kInvalidId || !dest.has_value()) return;
        // The "may" before anything is done: this callback is re-entered from
        // the top with the answer.
        int go = 0;
        if (*mover != kInvalidId) {
            go = confirmOptional(ctx, "Zenith Blade: move that friendly unit to that battlefield?",
                [&]() {
                    const auto now = movers(ctx.state, ctx.controller, dest);
                    return std::find(now.begin(), now.end(), *mover) != now.end();
                });
            if (go == -1) return;                // waiting for the agent
        }
        ctx.executor.stunUnitBy(stunned, ctx.source);
        if (go != 1) return;                     // declined / nothing to move
        ctx.executor.moveToBattlefield(*mover, *dest);
        RIFT_TRACE(ctx.events, "ZENITH BLADE: stunned enemy + moved a friendly "
                            "unit to that battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 258;
        d.def_id = R"RB(ogn-262-298)RB";
        d.name = R"RB(Zenith Blade)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-262/298)RB";
        d.collector_number = 262;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Leona)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Stun an enemy unit at a battlefield. You may move a friendly unit to that enemy unit's battlefield. (A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a572f0bc3ed1bd47c5759d36f5b951b15338e57e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_258(CardRegistry& r) {
    r.registerCard(258, std::make_unique<ZenithBlade>());
}

} // namespace riftbound
