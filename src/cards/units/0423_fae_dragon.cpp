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

class FaeDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, buff up to four friendly units. (Give each a +1 [M]
    //  buff if it doesn't have one.)"
    // The units are targets, up to four, zero allowed (CR 355.5, 355.13),
    // chosen as the ability is finalized (CR 383.3, 403.2), one at a time
    // with "None". A unit that already has a buff can still be chosen; it is
    // just not buffed again (CR 427.1.c). One that has left as it resolves is
    // a mistarget.
    //
    // "When you spend a buff, play a Gold gear token exhausted." Every spend
    // goes through EffectExecutor::spendBuff, which announces it. Nothing to
    // choose.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouSpendABuff};
    }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.firing_trigger == TriggerType::WhenYouSpendABuff) {
            if (finalizingChoices(ctx)) return;   // nothing to choose
            createGoldExhausted(ctx);
            RIFT_TRACE(ctx.events, "FAE DRAGON: a buff was spent -> Gold token (exhausted)");
            return;
        }
        const auto friendly = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& u) { return u.controller == ctx.controller; });
        const auto chosen = selectSeveralTargets(*this, ctx,
            "Fae Dragon: buff a friendly unit (up to four)", friendly, /*max_picks=*/4);
        if (!chosen.has_value()) return;          // waiting for the agent
        if (finalizingChoices(ctx)) return;       // choices end here (CR 383.3)
        int buffed = 0;
        for (auto id : *chosen)
            if (ctx.executor.buffUnit(id)) ++buffed;
        RIFT_TRACE(ctx.events, "FAE DRAGON: buffed " + std::to_string(buffed) +
                            " friendly units (up to 4)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 423;
        d.def_id = R"RB(sfd-101-221)RB";
        d.name = R"RB(Fae Dragon)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-101/221)RB";
        d.collector_number = 101;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Fae)RB", R"RB(Dragon)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, buff up to four friendly units. (Give each a +1 [M] buff if it doesn't have one.)
When you spend a buff, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/76aeda6bf59acee92b512f4a0272892673cfce90-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_423(CardRegistry& r) {
    r.registerCard(423, std::make_unique<FaeDragon>());
}

} // namespace riftbound
