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

class ForgottenSignpost : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Action][>] Exhaust a unit you control, [E]: Move a different unit you
    //  control to the location of the unit you exhausted to pay for this
    //  ability."
    // [>]/[E] = exhaust the gear. The target is the friendly unit to exhaust as
    // an additional cost; onActivate then moves a DIFFERENT friendly unit to
    // that unit's location.
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                          .must_be_friendly = true},
            .is_action = true, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        // Eligible exhaust targets: ready friendly units; also require a second
        // friendly unit to exist (the one to move).
        std::vector<GameObjectId> out;
        int friendly_units = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            friendly_units++;
        }
        if (friendly_units < 2) return out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value()) continue;
            if (obj.is_exhausted) continue;  // can only exhaust a ready unit
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId to_exhaust = kInvalidId;
        if (!targets.empty()) to_exhaust = targets[0];
        else to_exhaust = pickTarget(ctx, "Forgotten Signpost: exhaust a friendly unit",
                                     enumerateLegalTargets(ctx.state, ctx.controller, 0));
        if (to_exhaust == kInvalidId || !ctx.state.objectExists(to_exhaust)) return;
        auto dest = ctx.state.getObject(to_exhaust).location;
        if (!dest) return;
        // Pay the additional cost: exhaust the chosen unit.
        ctx.executor.exhaustObject(to_exhaust);

        // "Move a different unit you control to the location of the exhausted unit."
        std::vector<GameObjectId> movers;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == to_exhaust) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller || !obj.location.has_value()) continue;
            if (obj.location == *dest) continue;  // already there
            movers.push_back(id);
        }
        if (movers.empty()) return;
        GameObjectId mover = pickTarget(ctx, "Forgotten Signpost: move a different unit", movers);
        if (mover == kInvalidId || !ctx.state.objectExists(mover)) return;
        moveToLocation(ctx.executor, mover, dest);
        ctx.events.logTrace("FORGOTTEN SIGNPOST: moved a friendly unit to the exhausted unit's location");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 607;
        d.def_id = R"RB(unl-045-219)RB";
        d.name = R"RB(Forgotten Signpost)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-045/219)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action][>] Exhaust a unit you control, [E]: Move a different unit you control to the location of the unit you exhausted to pay for this ability.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/edf248974d1d97ff5ff1c6745f663fa059dbc82d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_607(CardRegistry& r) {
    r.registerCard(607, std::make_unique<ForgottenSignpost>());
}

} // namespace riftbound
