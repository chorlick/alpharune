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

class DangerousDuo : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Legion] — When you play me, give a unit +2 [M] this turn." The unit
    // is chosen as the ability goes on the chain (CR 383.3); without Legion
    // (CR 812: another card played this turn) there is nothing to choose.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // [Legion] — the ability is Active only if another card was played this
    // turn, and only an Active ability triggers (CR 727.1.c.1, 812.1.c):
    // judged as I am played. Off, nothing goes on the chain (finding G9).
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return legionActive(state, controller);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> units = chooseableUnits(ctx.state, ctx.controller,
                                                          [](const GameObject&) { return true; });
        std::sort(units.begin(), units.end());
        const GameObjectId t = pickTarget(ctx, "Dangerous Duo: give a unit +2 [M]", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(t, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 16;
        d.def_id = R"RB(ogn-016-298)RB";
        d.name = R"RB(Dangerous Duo)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-016/298)RB";
        d.collector_number = 16;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, give a unit +2 [M] this turn. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c02ea9e438d407c739276b788e015ac93843651-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_16(CardRegistry& r) {
    r.registerCard(16, std::make_unique<DangerousDuo>());
}

} // namespace riftbound
