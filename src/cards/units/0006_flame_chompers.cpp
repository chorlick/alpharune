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
    // "When you discard me, you may pay [R] to play me."
    // ENGINE GAP: the engine does not currently dispatch WhenYouDiscard
    // triggers (no CardsDiscardedEvent subscriber fires them), so this
    // onTrigger is forward-compatible but inert until that wiring exists.
    TriggerType triggerType() const override { return TriggerType::WhenYouDiscard; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        int fury = static_cast<int>(Domain::Fury);
        auto can_pay = [&]() {
            return ps.rune_pool.power[fury] + ps.rune_pool.universal_power >= 1;
        };
        if (!can_pay()) return;
        int conf = confirmOptional(ctx, "Flame Chompers: pay [R] to play me?",
                                   can_pay);
        if (conf < 1) return;
        // Pay [R]: prefer Fury power, fall back to universal.
        if (ps.rune_pool.power[fury] >= 1) ps.rune_pool.power[fury] -= 1;
        else ps.rune_pool.universal_power -= 1;
        ctx.executor.playIgnoringCost(ctx.controller, ctx.source);
        ctx.events.logTrace("FLAME CHOMPERS: paid [R] -> play from trash");
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
