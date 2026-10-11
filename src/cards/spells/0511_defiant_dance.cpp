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

class DefiantDance : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // chooseableUnits honours "can't be chosen by enemy spells and
        // abilities" for both picks.
        auto legal_a = chooseableUnits(ctx.state, ctx.controller,
                                       [](const GameObject&) { return true; });
        auto pair = pickTargetPair(ctx, "Defiant Dance", legal_a,
            [&](GameObjectId picked_a) {
                // "another unit"
                return chooseableUnits(ctx.state, ctx.controller,
                    [picked_a](const GameObject& o) { return o.id != picked_a; });
            });
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (ctx.state.chain.resuming.has_value() &&
            (ctx.state.chain.resuming->resume_point == 10 ||
             ctx.state.chain.resuming->resume_point == 12)) return;   // waiting for the agent
        // Each target is handled by itself: one that is no longer legal is
        // unaffected, the other still gets its part (CR 359.3.e.5).
        if (pair.first != kInvalidId && ctx.state.objectExists(pair.first)) {
            ctx.executor.giveTemporaryMight(pair.first, 2);
        }
        if (pair.second != kInvalidId && ctx.state.objectExists(pair.second)) {
            ctx.executor.giveTemporaryMight(pair.second, -2);
            // -2 M may make the second unit lethal-might-zero; cleanup handles
            // 0-might deaths. No explicit kill needed (might debuff, not damage).
        }
        RIFT_TRACE(ctx.events, "DEFIANT DANCE: +2M / -2M this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 511;
        d.def_id = R"RB(sfd-196-221)RB";
        d.name = R"RB(Defiant Dance)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-196/221)RB";
        d.collector_number = 196;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Irelia)RB"};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a unit +2 [M] this turn and another unit -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f5bc5c9e321830337998a2b85e4fec3cd8251c9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_511(CardRegistry& r) {
    r.registerCard(511, std::make_unique<DefiantDance>());
}

} // namespace riftbound
