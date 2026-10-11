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

class Charm : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Charm", legal);
        }
        if (picked == kInvalidId) return;   // suspended, or no unit to choose
        const auto& unit = ctx.state.getObject(picked);
        if (!unit.isUnit() || !unit.location.has_value()) return;
        // "Move an enemy unit." — the caster chooses where: the unit's base
        // or any battlefield other than where it is.
        const int dest = pickMoveDest(*this, ctx, "Charm: move it to", unit);
        if (dest < 0) return;               // waiting, or nowhere to move it
        moveTo(ctx, picked, dest);
        ctx.events.logTrace("CHARM: moved enemy " + ctx.state.getObject(picked).name +
                            " to " + destLabel(dest));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 43;
        d.def_id = R"RB(ogn-043-298)RB";
        d.name = R"RB(Charm)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-043/298)RB";
        d.collector_number = 43;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.ability_text = R"RB(Move an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/62ae564505db8fcba70605eac2083ac2d4397b5a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_43(CardRegistry& r) {
    r.registerCard(43, std::make_unique<Charm>());
}

} // namespace riftbound
