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

class RocketBarrage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [4][B]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 4, .power = 1, .power_domain = Domain::Mind}};
    }
    // "Choose one — Deal 4 to a unit in a base. / Kill a gear."
    // The mode and then that mode's target are chosen as the spell is played
    // (CR 355), and again when the Repeat executes ("may make different
    // choices"). Each mode offers only the objects it can affect.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // "A unit in a base."
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
            [&](const GameObject& u) {
                return u.isAtBase();
            });
    }
    static std::vector<GameObjectId> unitsInBases(const GameState& state, PlayerId me) {
        return chooseableUnits(state, me, [](const GameObject& o) { return o.isAtBase(); });
    }
    static std::vector<GameObjectId> gearOnBoard(const GameState& state, PlayerId me) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isGear() || !obj.location.has_value()) continue;
            if (obj.controller != me && obj.untargetable_by_enemy) continue;
            out.push_back(id);
        }
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !unitsInBases(state, controller).empty() ||
               !gearOnBoard(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto units = unitsInBases(ctx.state, ctx.controller);
        const auto gear = gearOnBoard(ctx.state, ctx.controller);
        const uint32_t legal = (units.empty() ? 0u : 1u) | (gear.empty() ? 0u : 2u);
        int mode = pickMode(ctx, "Rocket Barrage", 2,
                            {"Deal 4 to a unit in a base", "Kill a gear"}, legal);
        if (mode < 0) return;  // -1 waiting, -2 no legal mode
        GameObjectId picked = pickTarget(ctx, "Rocket Barrage", mode == 0 ? units : gear);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (mode == 0) {
            ctx.events.logTrace("ROCKET BARRAGE: deal 4 to " + ctx.state.getObject(picked).name);
            ctx.executor.dealDamage(picked, 4, ctx.source);
        } else {
            ctx.events.logTrace("ROCKET BARRAGE: kill gear " + ctx.state.getObject(picked).name);
            ctx.executor.killObject(picked);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 400;
        d.def_id = R"RB(sfd-077-221)RB";
        d.name = R"RB(Rocket Barrage)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-077/221)RB";
        d.collector_number = 77;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [4][B] (You may pay the additional cost to repeat this spell's effect, and may make different choices.)
Choose one —
Deal 4 to a unit in a base.Kill a gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0378f9adf9df08fed264fdd217ce0e94c3f611cb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_400(CardRegistry& r) {
    r.registerCard(400, std::make_unique<RocketBarrage>());
}

} // namespace riftbound
