#include "cards/card.h"
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

class CrescentGuardian : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you've played a spell this turn, you may pay [P] as an additional cost
    //  to play me. If you do, I enter ready." [P] = Chaos power. The cost is
    //  offered only when the condition holds.
    OptionalAdditionalCost optionalAdditionalCost(const GameState& state,
                                                  PlayerId player) const override {
        if (!state.player(player).played_spell_this_turn) return {};
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Chaos,
                /*any_domain=*/false, /*paid_flag=*/"__crescent_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // The engine sets the flag when the cost is paid and never clears it:
        // consume it, or a later replay without paying would still count.
        const auto flag = self.card_counters.find("__crescent_paid");
        const bool paid = flag != self.card_counters.end() && flag->second == 1;
        self.card_counters.erase("__crescent_paid");
        if (!paid) return;
        ctx.executor.readyObject(ctx.source);   // no-op if I left the board
        ctx.events.logTrace("CRESCENT GUARDIAN: paid [P] after a spell -> I enter ready");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 684;
        d.def_id = R"RB(unl-122-219)RB";
        d.name = R"RB(Crescent Guardian)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-122/219)RB";
        d.collector_number = 122;
        d.artist = R"RB(Aron Elekes)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.ability_text = R"RB(If you've played a spell this turn, you may pay [P] as an additional cost to play me. If you do, I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0ff22e9a517d029fd17cfbbbc93ca2f90c2c676c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_684(CardRegistry& r) {
    r.registerCard(684, std::make_unique<CrescentGuardian>());
}

} // namespace riftbound
