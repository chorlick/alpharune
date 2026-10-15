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
    // Each unit is a target (CR 355.5), so all six are chosen as the spell is
    // played: the first as its play-time target, the other five in its
    // finalize pass, before its cost (each one's Deflect is a cost of playing
    // it, CR 809). One Play intent; the units are asked one prompt at a time.
    // Progress lives on the item: resume_point kAwait while a prompt is out,
    // kChosen once the six are known, kDone once dealt; resume_data = the
    // units picked so far.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    static std::vector<GameObjectId> anyUnit(const GameState& state, PlayerId controller) {
        return chooseableUnits(state, controller, [](const GameObject&) { return true; });
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point != kChosen) {
            auto picked_before = [&ri](GameObjectId id) {
                return std::find(ri.resume_data.begin(), ri.resume_data.end(),
                                 static_cast<int32_t>(id.raw())) != ri.resume_data.end();
            };
            // A unit already picked stays available: its Deflect was paid then.
            auto offered = [&]() {
                const auto units = anyUnit(ctx.state, ctx.controller);
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
                if (unit != kInvalidId) {
                    if (!picked_before(unit)) commitChosenObject(ctx, unit);
                    ri.resume_data.push_back(static_cast<int32_t>(unit.raw()));
                }
                ri.resume_point = unit == kInvalidId ? kChosen : kPicking;
            } else if (ri.resume_point != kPicking) {
                // The first unit was chosen as the spell was played (the
                // engine asks for a needsPlayTimeTarget spell's target then).
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
                        std::to_string(ri.resume_data.size() + 1) + " of 6)", PromptKind::ChooseObject);
                    ri.resume_point = kAwait;
                    return;
                }
            }
            ri.resume_point = kChosen;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        ri.resume_point = kDone;
        // Each instance whose unit is no longer a legal target — off the board,
        // no longer one that may be chosen, or a new object since (CR
        // 359.3.e.4) — is a mistarget and deals nothing (CR 359.3.e.5).
        const auto legal = anyUnit(ctx.state, ctx.controller);
        std::vector<GameObjectId> hit;
        for (auto raw_id : ri.resume_data) {
            if (raw_id == kLost) continue;
            const auto id = GameObjectId::fromRaw(raw_id);
            if (std::find(legal.begin(), legal.end(), id) == legal.end() ||
                !ctx.state.stillTheChosenObject(ri, id)) {
                RIFT_TRACE(ctx.events, "TGT_MISTARGET: Icathian Rain - a chosen unit is no longer legal");
                continue;
            }
            ctx.executor.dealDamage(id, 2, ctx.source);
            hit.push_back(id);
        }
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(hit);   // they die at the same time
        }
    }
private:
    static constexpr size_t kInstances = 6;
    enum : int { kPicking = 20, kAwait = 21, kDone = 22, kChosen = 23 };
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
