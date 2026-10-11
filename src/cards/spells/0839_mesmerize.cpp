#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Mesmerize : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose one — Return a friendly unit to its owner's hand. /
    //  Give an enemy unit -2 [M] this turn."
    // The mode, then a unit that mode can affect, are chosen as the spell is
    // played (CR 355) — never a unit first and a mode that cannot use it.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It can choose any unit its controller has on the board (the play gate).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !chooseableUnits(state, controller,
                                [](const GameObject&) { return true; }).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        auto friendly = chooseableUnits(ctx.state, me,
                                        [me](const GameObject& o) { return o.controller == me; });
        auto enemy = chooseableUnits(ctx.state, me,
                                     [me](const GameObject& o) { return o.controller != me; });
        uint32_t mask = (friendly.empty() ? 0u : 1u) | (enemy.empty() ? 0u : 2u);
        int mode = pickMode(ctx, "Mesmerize", 2,
                            {"Return a friendly unit to its owner's hand",
                             "Give an enemy unit -2 [M] this turn"},
                            mask);
        if (mode < 0) return;   // -1 waiting, -2 no legal mode
        GameObjectId picked = pickTarget(ctx, "Mesmerize", mode == 0 ? friendly : enemy);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (mode == 0) ctx.executor.bounceToHand(picked);
        else ctx.executor.giveTemporaryMight(picked, -2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 839;
        d.def_id = R"RB(ven-052-166)RB";
        d.name = R"RB(Mesmerize)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-052/166)RB";
        d.collector_number = 52;
        d.artist = R"RB(小牛设计)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose one —
Return a friendly unit to its owner's hand.Give an enemy unit -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/854ceafc3f20c7e2d38c83170cb73232116f3d58-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_839(CardRegistry& r) {
    r.registerCard(839, std::make_unique<Mesmerize>());
}

}  // namespace riftbound
