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

class LoyalPup : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you defend at a battlefield, you may move me there."
    // Wired via WhenYouDefendAtABattlefield (TriggerManager::onCombatStarted
    // fires this on the defending player's on-board cards and stashes the
    // defended BF id in card_counters["__defend_bf"]). We may move there.
    TriggerType triggerType() const override {
        return TriggerType::WhenYouDefendAtABattlefield;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        auto it = me.card_counters.find("__defend_bf");
        if (it == me.card_counters.end()) return;
        BattlefieldId bf = static_cast<BattlefieldId>(it->second);
        auto already_here = [&]() {
            auto loc = ctx.state.getObject(ctx.source).battlefieldId();
            return loc && *loc == bf;
        };
        auto still_legal = [&]() { return ctx.state.objectExists(ctx.source) &&
                                          !already_here(); };
        int conf = confirmOptional(ctx, "Loyal Pup: move me to the defended battlefield?",
                                   still_legal);
        if (conf == -1) return;   // waiting for agent
        if (conf < 1) return;     // declined / already there
        ctx.executor.moveToBattlefield(ctx.source, bf);
        ctx.events.logTrace("LOYAL PUP: moved to the defended battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 447;
        d.def_id = R"RB(sfd-126-221)RB";
        d.name = R"RB(Loyal Pup)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-126/221)RB";
        d.collector_number = 126;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Dog)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When you defend at a battlefield, you may move me there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2f15c4508900636e1560fa0e8832aff6b3160d0f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_447(CardRegistry& r) {
    r.registerCard(447, std::make_unique<LoyalPup>());
}

} // namespace riftbound
