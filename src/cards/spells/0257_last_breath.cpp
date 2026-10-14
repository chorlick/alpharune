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

class LastBreath : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    // "Ready a friendly unit. It deals damage equal to its Might to an enemy
    //  unit at a battlefield." — two targets: the friendly unit, then the
    //  enemy unit (left out when no enemy unit is at a battlefield).
    bool needsPlayTimeTargetPair() const override { return true; }
    // Both targets are required (CR 355.8; testing phase D8): a friendly unit
    // and an enemy unit at a battlefield it can choose.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendlyUnits(state, controller).empty() &&
               !chooseableUnits(state, controller, [&](const GameObject& u) {
                    return u.controller != controller && u.isAtBattlefield();
                }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto [unit, enemy] = pickTargetPair(
            ctx, "Last Breath: friendly unit, then enemy unit at a battlefield",
            friendlyUnits(ctx.state, ctx.controller),
            [&](GameObjectId /*picked*/) {
                auto out = chooseableUnits(ctx.state, ctx.controller,
                    [&](const GameObject& u) {
                        return u.controller != ctx.controller && u.isAtBattlefield();
                    });
                std::sort(out.begin(), out.end());
                return out;
            });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        // Suspended, or the friendly unit is no longer a legal target: its
        // Might is then unknown and nothing is dealt (CR 359.3.e.12).
        if (unit == kInvalidId) return;
        ctx.executor.readyObject(unit);
        if (enemy == kInvalidId) return;
        const auto& u = ctx.state.getObject(unit);
        if (!u.location.has_value()) return;
        ctx.executor.dealDamage(enemy, u.current_might, unit);
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
        d.id = 257;
        d.def_id = R"RB(ogn-260-298)RB";
        d.name = R"RB(Last Breath)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-260/298)RB";
        d.collector_number = 260;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB"};
        d.energy_cost = 3;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Ready a friendly unit. It deals damage equal to its Might to an enemy unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7e6dc840210124825f787095541a66ae5cea178-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_257(CardRegistry& r) {
    r.registerCard(257, std::make_unique<LastBreath>());
}

} // namespace riftbound
