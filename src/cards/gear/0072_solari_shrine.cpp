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

class SolariShrine : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you kill a stunned enemy unit, you may exhaust this to draw 1."
    // The dying unit is the trigger's subject and is still marked stunned when
    // it dies, so "stunned" is checked as the trigger condition.
    // APPROXIMATION: who killed it is not tracked — any death of a stunned
    // enemy unit counts as "you kill".
    TriggerType triggerType() const override { return TriggerType::WhenAnEnemyUnitDies; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/,
                          PlayerId /*controller*/, TriggerType /*trigger*/,
                          GameObjectId subject) const override {
        return subject != kInvalidId && state.objectExists(subject) &&
               state.getObject(subject).is_stunned;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() {
            return ctx.state.objectExists(ctx.source) &&
                   !ctx.state.getObject(ctx.source).is_exhausted;
        };
        if (!costAlreadyPaid(ctx) && !still_legal()) return;
        int conf = confirmOptional(ctx, "Solari Shrine: exhaust to draw 1?",
                                   still_legal);
        if (conf < 1) return;
        if (costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SOLARI SHRINE: exhaust -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 72;
        d.def_id = R"RB(ogn-072-298)RB";
        d.name = R"RB(Solari Shrine)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-072/298)RB";
        d.collector_number = 72;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you kill a stunned enemy unit, you may exhaust this to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/139a6d1c62a717bab4cdd75926e7f4aba6efb15d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_72(CardRegistry& r) {
    r.registerCard(72, std::make_unique<SolariShrine>());
}

} // namespace riftbound
