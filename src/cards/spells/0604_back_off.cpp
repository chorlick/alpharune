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

class BackOff : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice. Draw-1 rider is a
    // partial-fizzle that fires whether or not the stun target is
    // legal at resolve time (matches existing "always-draw" reading).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Back Off", legal);
        // Distinguish "suspending for agent" (resume_point==7) from
        // "definitive no-target" (resume_point>=8). For suspends,
        // return without firing the draw rider.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked != kInvalidId) {
            ctx.executor.stunUnit(picked);
        }
        // "If you played this from your hand, draw 1." Always-draw
        // (more generous reading); rider fires whether or not the
        // stun target was legal.
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 604;
        d.def_id = R"RB(unl-042-219)RB";
        d.name = R"RB(Back Off)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-042/219)RB";
        d.collector_number = 42;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
[Stun] a unit. (It doesn't deal combat damage this turn.)
If you played this from your hand, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0e7f7fa55ae618f94fe48920f92406ddd7e3512-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_604(CardRegistry& r) {
    r.registerCard(604, std::make_unique<BackOff>());
}

} // namespace riftbound
