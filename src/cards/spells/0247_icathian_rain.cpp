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

class IcathianRain : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 2 to a unit." six times: six separately chosen units (the same
    // unit may be chosen more than once), each its own instance of damage.
    // One Play intent; the units are asked for one prompt at a time.
    // Progress lives on the resolving item: resume_point kAwait while a prompt
    // is out, resume_data = the units picked so far.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        auto picked_before = [&ri](GameObjectId id) {
            return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                             static_cast<int32_t>(id.raw())) != ri.resume_data.end();
        };
        // A unit already picked stays available: its Deflect was paid then.
        auto offered = [&]() {
            auto units = chooseableUnits(ctx.state, ctx.controller,
                                         [](const GameObject&) { return true; });
            std::sort(units.begin(), units.end());
            std::vector<GameObjectId> fresh, out;
            for (auto id : units) if (!picked_before(id)) fresh.push_back(id);
            fresh = offerableChoices(ctx, fresh);
            for (auto id : units)
                if (picked_before(id) ||
                    std::find(fresh.begin(), fresh.end(), id) != fresh.end())
                    out.push_back(id);
            return out;
        };
        if (ri.resume_point == kAwait) {
            const auto can_pick = offered();
            GameObjectId unit = takenObject(ctx);
            if (std::find(can_pick.begin(), can_pick.end(), unit) == can_pick.end())
                unit = can_pick.empty() ? kInvalidId : can_pick.front();
            if (unit == kInvalidId) {
                ri.resume_point = kDone;   // nothing left to choose
            } else {
                if (!picked_before(unit)) commitChosenObject(ctx, unit);
                ri.resume_data.push_back(static_cast<int32_t>(unit.raw()));
                ri.resume_point = kPicking;
            }
        } else if (ri.resume_point != kPicking) {
            // The first unit was chosen as the spell was played (the engine
            // asks for a needsPlayTimeTarget spell's target then).
            // If that unit is no longer a legal choice the engine has dropped
            // it from `targets`: that "Deal 2" is lost (CR 359.3.e.5), it is
            // not chosen again. kLost keeps its place in the count of six.
            ri.resume_data.clear();
            if (const auto locked = lockedPlayTarget(ctx, targets))
                ri.resume_data.push_back(*locked == kInvalidId ? kLost
                                                               : static_cast<int32_t>((*locked).raw()));
            ri.resume_point = kPicking;
        }
        if (ri.resume_point == kPicking && ri.resume_data.size() < kInstances) {
            const auto can_pick = offered();
            if (!can_pick.empty()) {
                std::vector<Intent> options;
                for (auto id : can_pick) options.push_back(objChoice(ctx.controller, id));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Icathian Rain: deal 2 to a unit (" +
                    std::to_string(ri.resume_data.size() + 1) + " of 6)");
                ri.resume_point = kAwait;
                return;
            }
        }
        ri.resume_point = kDone;
        // The executor skips a unit that has left the board (CR 359.3.e.5).
        for (auto id : ri.resume_data)
            if (id != kLost) ctx.executor.dealDamage(GameObjectId::fromRaw(id), 2, ctx.source);
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(objectIdsFromRaw(ri.resume_data));   // they die at the same time
        }
    }
private:
    static constexpr size_t kInstances = 6;
    enum : int { kPicking = 20, kAwait = 21, kDone = 22 };
    // resume_data entry for an instance whose play-time unit became illegal.
    static constexpr int32_t kLost = static_cast<int32_t>(kInvalidId.raw());
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 247;
        d.def_id = R"RB(ogn-248-298)RB";
        d.name = R"RB(Icathian Rain)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-248/298)RB";
        d.collector_number = 248;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Mind};
        d.tags = {R"RB(Kai'Sa)RB"};
        d.energy_cost = 7;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.
Deal 2 to a unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8df37f633d3da734f3f2d85808a7cb9eadec1b04-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_247(CardRegistry& r) {
    r.registerCard(247, std::make_unique<IcathianRain>());
}

} // namespace riftbound
