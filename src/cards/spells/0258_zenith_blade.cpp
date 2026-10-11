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

class ZenithBlade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }

    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemy_at_bf;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            if (obj.untargetable_by_enemy) continue;
            enemy_at_bf.push_back(id);
        }
        std::sort(enemy_at_bf.begin(), enemy_at_bf.end());
        GameObjectId stunned = pickTarget(ctx, "Zenith Blade: stun enemy unit",
                                          enemy_at_bf);
        if (stunned == kInvalidId) return;  // suspended, or no legal target — fizzle
        if (!ctx.state.objectExists(stunned)) return;
        // The stun comes after every choice below: each one suspends and this
        // callback is re-entered from the top with the answer, so anything
        // done before them would be done once per pass.
        std::optional<LocationId> dest = ctx.state.getObject(stunned).location;

        // "You may move a friendly unit to that enemy unit's battlefield."
        auto movable = [&]() {
            std::vector<GameObjectId> out;
            if (!dest.has_value() ||
                !std::holds_alternative<BattlefieldLocation>(*dest)) return out;
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.location == dest) continue;  // already there
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int conf = confirmOptional(ctx,
            "Zenith Blade: move a friendly unit to that battlefield?",
            [&]() { return !movable().empty(); });
        if (conf == -1) return;  // waiting on agent

        // Which friendly unit moves is the controller's choice (asked only
        // when there is more than one). resume_point kAwait marks that prompt.
        GameObjectId mover = kInvalidId;
        if (conf == 1) {
            const auto units = movable();
            auto* ri = ctx.state.chain.resuming.has_value() ? &*ctx.state.chain.resuming : nullptr;
            if (ri && ri->resume_point == kAwait) {
                mover = takenObject(ctx);
                if (std::find(units.begin(), units.end(), mover) == units.end())
                    mover = units.empty() ? kInvalidId : units.front();
                ri->resume_point = kDone;
            } else if (ri && ri->resume_point == kDone) {
                return;
            } else if (ri && units.size() > 1) {
                std::vector<Intent> options;
                for (auto id : units) options.push_back(objChoice(ctx.controller, id));
                ctx.executor.requestChoice(ctx.controller, std::move(options),
                    "Zenith Blade: friendly unit to move to that battlefield");
                ri->resume_point = kAwait;
                return;
            } else if (!units.empty()) {
                mover = units.front();
            }
        }
        ctx.executor.stunUnitBy(stunned, ctx.source);
        if (mover == kInvalidId) return;   // declined / nothing to move
        commitChosenObject(ctx, mover);
        ctx.executor.moveToBattlefield(mover, std::get<BattlefieldLocation>(*dest).id);
        RIFT_TRACE(ctx.events, "ZENITH BLADE: stunned enemy + moved a friendly "
                            "unit to that battlefield");
    }
private:
    enum : int { kAwait = 21, kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 258;
        d.def_id = R"RB(ogn-262-298)RB";
        d.name = R"RB(Zenith Blade)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-262/298)RB";
        d.collector_number = 262;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Order};
        d.tags = {R"RB(Leona)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Stun an enemy unit at a battlefield. You may move a friendly unit to that enemy unit's battlefield. (A stunned unit doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a572f0bc3ed1bd47c5759d36f5b951b15338e57e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_258(CardRegistry& r) {
    r.registerCard(258, std::make_unique<ZenithBlade>());
}

} // namespace riftbound
