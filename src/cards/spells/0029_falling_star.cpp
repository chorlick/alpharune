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

class FallingStar : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto all_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isUnit() && obj.location.has_value()) out.push_back(id);
            }
            return out;
        };

        auto [a, b] = pickTargetPair(
            ctx, "Falling Star",
            all_units(),
            [&](GameObjectId /*picked_a*/) { return all_units(); });

        // Suspend detection (mirrors MStarCrossed): if a pick is invalid AND
        // we're parked at a prompt-publish resume_point (10 or 12), the chain
        // manager will re-enter after the agent decides — return now without
        // applying anything.
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;

        auto hit = [&](GameObjectId t) {
            if (t == kInvalidId || !ctx.state.objectExists(t)) return;
            ctx.executor.dealDamage(t, 3, ctx.source);
            if (ctx.state.objectExists(t) &&
                ctx.state.getObject(t).hasLethalDamage()) {
                ctx.executor.killObject(t);
            }
        };
        hit(a);
        hit(b);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 29;
        d.def_id = R"RB(ogn-029-298)RB";
        d.name = R"RB(Falling Star)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-029/298)RB";
        d.collector_number = 29;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(Deal 3 to a unit.
Deal 3 to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9cf2d2e59e1bf839cdf5c2a77e95f5d1e871788f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_29(CardRegistry& r) {
    r.registerCard(29, std::make_unique<FallingStar>());
}

} // namespace riftbound
