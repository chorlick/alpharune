#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class BattleMistress : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // Two triggered abilities:
    //   "When you recycle a rune, you may exhaust me to play a Gold gear
    //    token exhausted."
    //   "When one or more enemy units die, ready me."
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouRecycleARune, TriggerType::WhenAnEnemyUnitDies};
    }
    // "When ONE OR MORE enemy units die": once for units that die together.
    bool triggersOncePerBatch(TriggerType trigger) const override {
        return trigger == TriggerType::WhenAnEnemyUnitDies;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenYouRecycleARune) {
            auto still_legal = [&]() {
                return ctx.state.objectExists(ctx.source) &&
                       !ctx.state.getObject(ctx.source).is_exhausted;
            };
            const int go = confirmOptional(
                ctx, "Battle Mistress: exhaust me to play a Gold token exhausted?", still_legal);
            if (go == 1 && costDueNow(ctx)) ctx.executor.exhaustObject(ctx.source);
            if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
            if (go != 1) return;
            createGoldExhausted(ctx);
            RIFT_TRACE(ctx.events, "BATTLE MISTRESS: recycled a rune -> Gold token (exhausted)");
            return;
        }
        // "When one or more enemy units die, ready me."
        if (finalizingChoices(ctx)) return;       // no choices in this ability
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "BATTLE MISTRESS: enemy unit died -> ready me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 517;
        d.def_id = R"RB(sfd-203-221)RB";
        d.name = R"RB(Battle Mistress)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-203/221)RB";
        d.collector_number = 203;
        d.artist = R"RB(Zhongqi Li)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Sivir)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you recycle a rune, you may exhaust me to play a Gold gear token exhausted.
When one or more enemy units die, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fd060882c32a8deac04aea4241c6ab7b97236a05-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_517(CardRegistry& r) {
    r.registerCard(517, std::make_unique<BattleMistress>());
}

} // namespace riftbound
