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

class FlameChompers : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you discard me, you may pay [R] to play me." Fires from the trash.
    TriggerType triggerType() const override { return TriggerType::WhenIAmDiscarded; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto in_trash = [&]() { return ctx.state.getObject(ctx.source).zone == ZoneType::Trash; };
        auto can_pay = [&]() {
            return in_trash() && payableOnePower(ctx.state, ctx.controller, Domain::Fury);
        };
        // No return ahead of the "may": with [R] unpayable confirmOptional
        // stores the "no" on the chain item, so the resolution asks nothing
        // if it has become payable since (CR 383.3.a).
        int conf = confirmOptional(ctx, "Flame Chompers: pay [R] to play me?", can_pay);
        if (conf < 1) return;
        if (costDueNow(ctx) && !payOnePower(ctx, ctx.controller, Domain::Fury)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (!in_trash()) return;              // left the trash in response
        // At resolution: where I am played is a choice of my play, made as I am
        // finalized (CR 355.2, 420.3.b.2; testing phase M5).
        const int where = pickEffectPlayLocation(*this, ctx, "Flame Chompers: play me to",
                                                 ctx.source);
        if (where == -1) return;              // waiting for the agent
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source,
                                      effectPlayLocation(ctx.state, ctx.controller, where));
        RIFT_TRACE(ctx.events, "FLAME CHOMPERS: paid [R] -> play from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 6;
        d.def_id = R"RB(ogn-006-298)RB";
        d.name = R"RB(Flame Chompers)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-006/298)RB";
        d.collector_number = 6;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you discard me, you may pay [R] to play me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1f6f5ebd18e5daac30d62626fddd785c4b457c2b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_6(CardRegistry& r) {
    r.registerCard(6, std::make_unique<FlameChompers>());
}

} // namespace riftbound
