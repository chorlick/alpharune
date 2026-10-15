#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class TheHarrowing : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play a unit from your trash, ignoring its Energy cost." The trash is a
    // public zone, so the unit is a target (CR 355.10.a), chosen as the spell
    // is played (CR 355.5) — in its finalize pass, before its cost. It is not
    // playable with no unit there (CR 355.8).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // a card in a trash, not a unit on the board
    }
    static std::vector<GameObjectId> trashUnits(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto cid : state.player(controller).trash)
            if (state.objectExists(cid) && state.getObject(cid).isUnit()) out.push_back(cid);
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !trashUnits(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // No unit: pickTarget stores "no target", so nothing is asked later.
        const GameObjectId picked = pickTarget(ctx, "The Harrowing (unit from trash)",
                                               trashUnits(ctx.state, ctx.controller));
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        // A unit no longer in the trash is a mistarget (CR 359.3.e.5).
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // CR 355.2.a: no location is printed, so the unit goes to its
        // controller's base or to a battlefield they control (and may be
        // played to) — chosen as it is played, at resolution (CR 355.10.b: a
        // play location is a permission, not a target; item M5).
        const int where = pickEffectPlayLocation(*this, ctx, "The Harrowing: play it to");
        if (where == -1) return;   // waiting for the agent
        // "ignoring its Energy cost. (You must still pay its Power cost.)"
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        // Paying is optional (CR 445.2); the play takes the unit out of the
        // trash itself, and only once the Power is paid.
        if (ctx.executor.playPayingCost(ctx.controller, picked,
                                        effectPlayLocation(ctx.state, ctx.controller, where),
                                        power_only))
            RIFT_TRACE(ctx.events, "THE HARROWING: played a unit from trash for its Power cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 198;
        d.def_id = R"RB(ogn-198-298)RB";
        d.name = R"RB(The Harrowing)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-198/298)RB";
        d.collector_number = 198;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 6;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Play a unit from your trash, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6914034f56dd50ab37df26e3541c997479bb5a6d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_198(CardRegistry& r) {
    r.registerCard(198, std::make_unique<TheHarrowing>());
}

} // namespace riftbound
