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

class SmokeAndMirrors : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a unit you control and another unit you control at a different
    //  location. If at least one of them has [Temporary], move each to the
    //  other's location. Draw 1."
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    // {first unit, second unit} for every ordered pair of friendly units at
    // different locations. (Ordered: played from facedown, the first must be
    // at that battlefield and the second cannot be, CR 811.1.d.4.)
    std::optional<std::vector<std::vector<GameObjectId>>> enumerateLegalTargetSets(
        const GameState& state, PlayerId controller) const override {
        const auto units = enumerateLegalTargets(state, controller);
        std::vector<std::vector<GameObjectId>> sets;
        for (auto a : units)
            for (auto b : units)
                if (a != b && state.getObject(a).location != state.getObject(b).location)
                    sets.push_back({a, b});
        return sets;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        auto sets = enumerateLegalTargetSets(state, controller);
        return sets.has_value() && !sets->empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // The swap needs both chosen units to still be legal targets; the
        // draw does not depend on them.
        if (targets.size() >= 2 && ctx.state.objectExists(targets[0]) &&
            ctx.state.objectExists(targets[1])) {
            auto& a = ctx.state.getObject(targets[0]);
            auto& b = ctx.state.getObject(targets[1]);
            auto la = a.location;
            auto lb = b.location;
            // "If at least one of them has [Temporary]" — gate the move.
            bool has_temp = a.hasKeyword(Keyword::Temporary) ||
                            b.hasKeyword(Keyword::Temporary);
            if (has_temp && la != lb) {
                moveToLocation(ctx.executor, targets[0], lb);
                moveToLocation(ctx.executor, targets[1], la);
                ctx.events.logTrace("SMOKE AND MIRRORS: swapped two friendly units (Temporary gate met)");
            } else {
                ctx.events.logTrace("SMOKE AND MIRRORS: no swap (Temporary gate not met / same loc)");
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 645;
        d.def_id = R"RB(unl-083-219)RB";
        d.name = R"RB(Smoke and Mirrors)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-083/219)RB";
        d.collector_number = 83;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Choose a unit you control and another unit you control at a different location. If at least one of them has [Temporary], move each to the other's location. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/38886634ee8646707d9c26020f977f14c934a4c0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_645(CardRegistry& r) {
    r.registerCard(645, std::make_unique<SmokeAndMirrors>());
}

} // namespace riftbound
