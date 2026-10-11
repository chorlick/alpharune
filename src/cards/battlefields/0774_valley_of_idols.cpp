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

class ValleyOfIdols : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When a player plays a unit here, they may pay [1] to [Buff] it."
    // The battlefield WhenYouPlayAUnit trigger fires (per TriggerManager) only
    // on the BF where the unit landed, with ctx.controller = the playing
    // player — so this covers BOTH players' plays here.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "it" is the unit that was played: the trigger's subject.
        const GameObjectId played = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        auto still_there = [&]() {
            if (played == kInvalidId || !ctx.state.objectExists(played)) return false;
            const auto& u = ctx.state.getObject(played);
            return u.isUnit() && u.location.has_value();
        };
        if (!still_there()) return;
        int decision = confirmOptional(ctx, "Valley of Idols: pay [1] to [Buff] the unit?",
            [&]() { return still_there() && payableEnergy(ctx.state, ctx.controller) >= 1; });
        if (decision < 1) return;   // waiting on the agent, declined or not legal

        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        // buffUnit gives the +1 [M] buff only to a unit without one (CR 427.1.b).
        if (ctx.executor.buffUnit(played))
            ctx.events.logTrace("VALLEY OF IDOLS: paid [1] -> [Buff] the played unit");
        else
            ctx.events.logTrace("VALLEY OF IDOLS: paid [1] but unit already has a buff");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 774;
        d.def_id = R"RB(unl-218-219)RB";
        d.name = R"RB(Valley of Idols)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-218/219)RB";
        d.collector_number = 218;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a player plays a unit here, they may pay [1] to [Buff] it. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4191aa2fda9e754a7f5421edc94bd829f5795650-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_774(CardRegistry& r) {
    r.registerCard(774, std::make_unique<ValleyOfIdols>());
}

} // namespace riftbound
