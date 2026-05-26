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

class ShadowSCall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    // Phase 6q proof-of-concept (single friendly-unit target).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Shadow's Call", legal);
        // pickTarget returns kInvalidId in TWO cases. Distinguish via
        // resume_point: == 7 means "just published prompt, suspending —
        // chain manager will re-enter"; >= 8 (or no chain at all) means
        // "committed pick (possibly nothing legal)". For Shadow's Call,
        // the "draw 2" rider fires even when the targeted part has no
        // legal pick (the test fixture DrawsTwoEvenWithoutTarget pins
        // this behavior — partial-fizzle, riders always resolve).
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.giveTemporaryKeyword(picked, Keyword::Temporary, 0);
        }
        ctx.executor.drawCards(ctx.controller, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 727;
        d.def_id = R"RB(unl-165-219)RB";
        d.name = R"RB(Shadow's Call)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-165/219)RB";
        d.collector_number = 165;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB(Choose a friendly unit without [Temporary]. Give it [Temporary]. Draw 2. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5a9a81d353131fc275313e737d55ca6e2661dce6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_727(CardRegistry& r) {
    r.registerCard(727, std::make_unique<ShadowSCall>());
}

} // namespace riftbound
