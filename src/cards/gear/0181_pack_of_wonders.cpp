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

class PackOfWonders : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost = ActivationCost{.exhaust = true};
        ab.targets = TargetRequirements{.count = 1};
        ab.needs_activation_time_target = true;  // pickTarget excludes self
        return {ab};
    }
    // Broad gate: any friendly gear/unit on board other than self.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            if (!(obj.isUnit() || obj.isGear())) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;             // "another"
            if (obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!(obj.isUnit() || obj.isGear())) continue;
            legal.push_back(id);
        }
        // (Facedown-card targets unmodeled — see header comment.)
        GameObjectId picked = pickTarget(ctx, "Pack of Wonders: bounce a friendly "
                                              "gear/unit", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.bounceToHand(picked);
        ctx.events.logTrace("PACK OF WONDERS: returned a friendly gear/unit to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 181;
        d.def_id = R"RB(ogn-181-298)RB";
        d.name = R"RB(Pack of Wonders)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-181/298)RB";
        d.collector_number = 181;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([E]: Return another friendly gear, unit, or facedown card to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0830f326c49abe7fb1d3c9787e7ce5d7b776eec-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_181(CardRegistry& r) {
    r.registerCard(181, std::make_unique<PackOfWonders>());
}

} // namespace riftbound
