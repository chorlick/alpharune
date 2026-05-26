#include "cards/card.h"
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

class GrandmasterAtArms : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a0;  // [1],[E]: attach a detached Equipment
        a0.cost = ActivationCost{.exhaust = true, .energy = 1};
        a0.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        a0.needs_activation_time_target = true;
        ActivatedAbility a1;  // [E]: move an attached Equipment
        a1.cost = ActivationCost{.exhaust = true};
        a1.targets = TargetRequirements{.count = 1, .must_be_unit = true};
        a1.needs_activation_time_target = true;
        return {a0, a1};
    }

    static bool isEquipment(const GameObject& g) {
        if (!g.isGear()) return false;
        for (const auto& t : g.tags) if (t == "Equipment") return true;
        return false;
    }

    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId me = ctx.controller;
        // Find an eligible friendly Equipment: detached (idx0) or attached (idx1).
        GameObjectId gear = kInvalidId;
        for (auto& [id, g] : ctx.state.objects) {
            if (g.controller != me || !isEquipment(g)) continue;
            bool attached = g.attached_to.has_value();
            if (ability_index == 0 && attached) continue;   // need detached
            if (ability_index == 1 && !attached) continue;  // need attached
            gear = id; break;
        }
        if (gear == kInvalidId) return;
        // Pick a friendly unit to attach it to.
        std::vector<GameObjectId> units;
        for (auto& [id, u] : ctx.state.objects) {
            if (u.isUnit() && u.controller == me && u.location.has_value())
                units.push_back(id);
        }
        GameObjectId unit = pickTarget(ctx, "Grandmaster: attach Equipment to unit",
                                       units);
        if (unit == kInvalidId || !ctx.state.objectExists(unit) ||
            !ctx.state.objectExists(gear)) return;
        auto& g = ctx.state.getObject(gear);
        // Detach from current bearer first (refund its might bonus).
        if (g.attached_to.has_value()) {
            ctx.executor.unattachGear(gear);
        }
        // Attach to the chosen unit (mirror Akshan's inline attach).
        auto& tgt = ctx.state.getObject(unit);
        g.attached_to = unit;
        tgt.attachments.push_back(gear);
        g.location = tgt.location;
        g.zone = tgt.zone;
        g.controller = me;
        tgt.attachment_might_bonus += g.might_bonus;
        tgt.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{gear, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        ctx.events.logTrace("GRANDMASTER AT ARMS: attached " + g.name +
                            " to " + tgt.name);
    }
private:
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
