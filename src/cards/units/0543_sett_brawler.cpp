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

class SettBrawler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Fix: fire on play AND conquer (NOT hold — the old impl over-fired via
    // WhenIConquerOrHold).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // "If I don't have a buff, I get a +1 [M] buff." So only buff while
        // unbuffed (a buff is a persistent +1M counter; engine models it via
        // buff_count).
        if (self.buff_count > 0) return;
        ctx.executor.buffUnit(ctx.source);
        ctx.events.logTrace("SETT: buff me (+1M buff)");
    }

    // Activated: "Spend my buff: Give me +4 [M] this turn." Spending a buff
    // is the whole cost; no exhaust / rune cost. Gated on having a buff.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.buff_count <= 0) return;  // no buff to spend
        self.buff_count -= 1;
        ctx.executor.giveTemporaryMight(ctx.source, 4);
        ctx.events.logTrace("SETT: spent a buff for +4M this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 543;
        d.def_id = R"RB(sfd-232-221)RB";
        d.name = R"RB(Sett, Brawler)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-232/221)RB";
        d.collector_number = 232;
        d.artist = R"RB(Andie Tong)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Sett)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When I'm played and when I conquer, buff me. (If I don't have a buff, I get a +1 [M] buff.)
Spend my buff: Give me +4 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a13d5473e205cb9dd8123895c58ed6ac8f19e814-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_543(CardRegistry& r) {
    r.registerCard(543, std::make_unique<SettBrawler>());
}

} // namespace riftbound
