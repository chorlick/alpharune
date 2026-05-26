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

// "[Repeat] [2] ... Move an enemy unit to a location where there's a unit with
//  the same controller." Destination = a location occupied by ANOTHER unit
//  with the moved unit's controller. ([Repeat] is engine-handled.)

class Temptation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        // Need an enemy unit whose controller has a second unit elsewhere.
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
            for (auto& [oid, other] : state.objects) {
                if (oid == id) continue;
                if (!other.isUnit() || other.controller != obj.controller) continue;
                if (!other.location.has_value()) continue;
                if (other.location != obj.location) return true;  // distinct location
            }
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.location.has_value() || obj.untargetable_by_enemy) continue;
            enemies.push_back(id);
        }
        // B = a unit with the SAME controller as A, at a DIFFERENT location
        // (its location is the destination).
        auto dest_units = [&](GameObjectId a) {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(a)) return out;
            auto& av = ctx.state.getObject(a);
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == a) continue;
                if (!obj.isUnit() || obj.controller != av.controller) continue;
                if (!obj.location.has_value() || obj.location == av.location) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Temptation", enemies, dest_units);
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a == kInvalidId || b == kInvalidId ||
            !ctx.state.objectExists(a) || !ctx.state.objectExists(b)) return;
        auto dest = ctx.state.getObject(b).location;
        moveToLocation(ctx.executor, a, dest);
        ctx.events.logTrace("TEMPTATION: moved enemy unit to a location with another of its units");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 450;
        d.def_id = R"RB(sfd-129-221)RB";
        d.name = R"RB(Temptation)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-129/221)RB";
        d.collector_number = 129;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2] (You may pay the additional cost to repeat this spell's effect.)
Move an enemy unit to a location where there's a unit with the same controller.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d5075a540ff5e4a2daec598cb6400fbb6570673-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_450(CardRegistry& r) {
    r.registerCard(450, std::make_unique<Temptation>());
}

} // namespace riftbound
