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
//  the same controller." The enemy unit is the target; the destination is a
//  choice among the locations holding ANOTHER unit of its controller (not a
//  second target, so no Deflect is owed for it). ([Repeat] is engine-handled.)

class Temptation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    /// Where `a` may be moved (int-coded as moveDests): each other location
    /// holding another unit of its controller.
    static std::vector<int> destinations(const GameState& state, GameObjectId a) {
        std::vector<int> out;
        if (!state.objectExists(a)) return out;
        const auto& av = state.getObject(a);
        if (!av.isUnit() || !av.location.has_value()) return out;
        for (int d : moveDests(state, av)) {
            const LocationId loc = d <= 0
                ? LocationId{BaseLocation{av.controller}}
                : LocationId{BattlefieldLocation{static_cast<BattlefieldId>(d - 1)}};
            for (const auto& [id, obj] : state.objects) {
                if (id == a || !obj.isUnit() || obj.controller != av.controller) continue;
                if (!obj.location.has_value() || !(*obj.location == loc)) continue;
                out.push_back(d);
                break;
            }
        }
        return out;
    }
    static std::vector<GameObjectId> movable(const GameState& state, PlayerId me) {
        return chooseableUnits(state, me, [&](const GameObject& o) {
            return o.controller != me && !destinations(state, o.id).empty();
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !movable(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId a = pickTarget(ctx, "Temptation", movable(ctx.state, ctx.controller));
        if (a == kInvalidId) return;         // waiting, nothing to move, or a mistarget
        const auto dests = destinations(ctx.state, a);
        const int n = 1 + static_cast<int>(ctx.state.battlefields.size());
        std::vector<std::string> labels;
        uint32_t mask = 0;
        for (int d = 0; d < n; ++d) labels.push_back(destLabel(d));
        for (int d : dests) if (d >= 0 && d < 32) mask |= (1u << d);
        int dest = pickMode(ctx, "Temptation: move it where?", n, labels, mask);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (dest < 0) return;
        // The destination must still hold another unit of its controller.
        if (std::find(dests.begin(), dests.end(), dest) == dests.end()) return;
        moveTo(ctx, a, dest);
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
