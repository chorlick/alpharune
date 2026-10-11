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

class FoxFire : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Kill any number of units at a battlefield with total Might 4 or less."
    // The controller chooses the units, one prompt at a time: the first names
    // the battlefield, each later one adds a unit there that still fits the
    // total, or stops. Progress lives on the resolving item: resume_point
    // kAwait while a prompt is out, resume_data = the units picked so far.
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true, .max_might = 4};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto picked_before = [&ri](GameObjectId id) {
            return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                             static_cast<int32_t>(id)) != ri.resume_data.end();
        };
        auto candidates = [&]() {
            int budget = kTotalMight;
            std::optional<BattlefieldId> bf;
            for (auto id : ri.resume_data) {
                const auto& u = ctx.state.getObject(static_cast<GameObjectId>(id));
                budget -= std::max(0, u.current_might);
                bf = u.battlefieldId();
            }
            auto units = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.isAtBattlefield() && (!bf.has_value() || u.battlefieldId() == bf) &&
                       u.current_might <= budget && !picked_before(u.id);
            });
            std::sort(units.begin(), units.end());
            return offerableChoices(ctx, units);
        };
        bool stop = false;
        if (ri.resume_point == kAwait) {
            const auto can_pick = candidates();
            const GameObjectId unit = takenObject(ctx);
            if (std::find(can_pick.begin(), can_pick.end(), unit) == can_pick.end()) {
                stop = true;   // "stop here", or the unit is no longer available
            } else {
                commitChosenObject(ctx, unit);
                ri.resume_data.push_back(static_cast<int32_t>(unit));
            }
        } else if (ri.resume_point != kPicking) {
            // The first unit was chosen as the spell was played (the engine
            // asks for a needsPlayTimeTarget spell's target then).
            ri.resume_data.clear();
            if (!targets.empty()) ri.resume_data.push_back(static_cast<int32_t>(targets[0]));
        }
        ri.resume_point = kPicking;
        if (!stop) {
            const auto can_pick = candidates();
            if (!can_pick.empty()) {
                std::vector<Intent> options;
                // "any number": after the first unit the controller may stop.
                if (!ri.resume_data.empty())
                    options.push_back(valChoice(ctx.controller, 0, "Stop here"));
                for (auto id : can_pick) options.push_back(objChoice(ctx.controller, id));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Fox-Fire: kill a unit at a battlefield (total Might 4 or less)");
                ri.resume_point = kAwait;
                return;
            }
        }
        ri.resume_point = kDone;
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, std::vector<GameObjectId>(ri.resume_data.begin(), ri.resume_data.end()));   // they die at the same time
            for (auto id : ri.resume_data)
                ctx.executor.killObject(static_cast<GameObjectId>(id));
        }
        ctx.events.logTrace("FOX-FIRE: killed " + std::to_string(ri.resume_data.size()) +
                            " chosen units (total Might <= 4) at one battlefield");
    }
private:
    static constexpr int kTotalMight = 4;
    // 0-Might units fit any total: "any number" is every unit on offer.
    enum : int { kPicking = 20, kAwait = 21, kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 253;
        d.def_id = R"RB(ogn-256-298)RB";
        d.name = R"RB(Fox-Fire)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-256/298)RB";
        d.collector_number = 256;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ahri)RB"};
        d.energy_cost = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Kill any number of units at a battlefield with total Might 4 or less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7ad9d6a46a1c54080d54950a0044da3a82e32b45-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_253(CardRegistry& r) {
    r.registerCard(253, std::make_unique<FoxFire>());
}

} // namespace riftbound
