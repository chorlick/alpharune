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

class GrandmasterAtArms : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1], [E]: Attach a detached Equipment you control to a unit you control."
    // "[E]: Attach an attached Equipment you control to a unit you control."
    // Both choose an Equipment and then a unit, as the ability is activated
    // (CR 355) — pickTargetPair in the finalize pass.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a0;  // [1],[E]: attach a detached Equipment
        a0.cost = ActivationCost{.exhaust = true, .energy = 1};
        ActivatedAbility a1;  // [E]: move an attached Equipment
        a1.cost = ActivationCost{.exhaust = true};
        return {a0, a1};
    }
    // Offered only with something to do: an Equipment of the right kind on
    // the board and a unit to put it on.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!LegendCard::canActivateAbilityAt(state, controller, source, ability_index))
            return false;
        return !equipmentFor(state, controller, ability_index).empty();
    }

    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        const auto [gear, unit] = pickTargetPair(
            ctx, "Grandmaster at Arms: an Equipment, then the unit to attach it to",
            equipmentFor(ctx.state, me, ability_index),
            [&](GameObjectId g) { return unitsFor(ctx.state, me, g); });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (gear == kInvalidId || unit == kInvalidId) return;
        // attachFree has no board check of its own: both ends must be there,
        // and still mine.
        if (!equipEndsOnBoard(ctx.state, gear, unit)) return;
        if (ctx.state.getObject(gear).controller != me ||
            ctx.state.getObject(unit).controller != me) return;
        // Through unattachGear first, so the old bearer's "for as long as this
        // is attached" effects end (attachFree alone would skip the hook).
        ctx.executor.unattachGear(gear);
        attachFree(ctx, gear, unit);
        ctx.events.logTrace("GRANDMASTER AT ARMS: attached " + ctx.state.getObject(gear).name +
                            " to " + ctx.state.getObject(unit).name);
    }
private:
    /// Equipment `me` controls ON THE BOARD (a card in a hand, deck or trash
    /// is not an Equipment you control): detached for ability 0, attached for
    /// ability 1 — with at least one unit it could go on.
    static std::vector<GameObjectId> equipmentFor(const GameState& state, PlayerId me,
                                                  int ability_index) {
        std::vector<GameObjectId> out;
        for (const auto& [id, g] : state.objects) {
            if (g.controller != me || !g.location.has_value() || !isEquipment(g)) continue;
            if (g.attached_to.has_value() != (ability_index == 1)) continue;
            if (!unitsFor(state, me, id).empty()) out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    /// Units `me` controls that `gear` could be attached to: any but the one
    /// it is already on.
    static std::vector<GameObjectId> unitsFor(const GameState& state, PlayerId me,
                                              GameObjectId gear) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(gear)) return out;
        const auto bearer = state.getObject(gear).attached_to;
        for (const auto& [id, u] : state.objects) {
            if (!u.isUnit() || u.controller != me || !u.location.has_value()) continue;
            if (bearer.has_value() && *bearer == id) continue;
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 554;
        d.def_id = R"RB(sfd-245-221)RB";
        d.name = R"RB(Grandmaster at Arms)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-245/221)RB";
        d.collector_number = 245;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Jax)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB([1], [E]: Attach a detached Equipment you control to a unit you control.
[E]: Attach an attached Equipment you control to a unit you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/941f72a422e0143524b3dd0cba1fd87e4286ecb4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_554(CardRegistry& r) {
    r.registerCard(554, std::make_unique<GrandmasterAtArms>());
}

} // namespace riftbound
