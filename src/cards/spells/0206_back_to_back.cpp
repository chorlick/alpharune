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

class BackToBack : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // "Give two friendly units each +2 [M] this turn." — two different units.
    bool needsPlayTimeTargetPair() const override { return true; }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return friendlyUnits(state, controller).size() >= 2;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto [a, b] = pickTargetPair(
            ctx, "Back to Back: two friendly units",
            friendlyUnits(ctx.state, ctx.controller),
            [&](GameObjectId picked_a) {
                auto rest = friendlyUnits(ctx.state, ctx.controller);
                rest.erase(std::remove(rest.begin(), rest.end(), picked_a), rest.end());
                return rest;
            });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (a == kInvalidId && b == kInvalidId) return;   // suspended, or no unit
        // Each unit is its own target: one that left is skipped (CR 359.3.e.5).
        if (a != kInvalidId) ctx.executor.giveTemporaryMight(a, 2);
        if (b != kInvalidId) ctx.executor.giveTemporaryMight(b, 2);
    }
private:
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId controller) {
        auto out = chooseableUnits(state, controller,
            [controller](const GameObject& u) { return u.controller == controller; });
        std::sort(out.begin(), out.end());
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 206;
        d.def_id = R"RB(ogn-206-298)RB";
        d.name = R"RB(Back to Back)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-206/298)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give two friendly units each +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7f5585dcd3e9c7866e3b99cb169a0404ea4c010f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_206(CardRegistry& r) {
    r.registerCard(206, std::make_unique<BackToBack>());
}

} // namespace riftbound
