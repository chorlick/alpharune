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

class RibbonDancer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }

    // "When I move to a battlefield, give another friendly unit +1 [M] this turn."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> others;
        for (auto& [id, obj] : ctx.state.objects)
            if (id != ctx.source && obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value())
                others.push_back(id);
        const GameObjectId t = pickTarget(ctx, "Ribbon Dancer: give another friendly unit +1 [M]",
                                          others);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(t, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 361;
        d.def_id = R"RB(sfd-038-221)RB";
        d.name = R"RB(Ribbon Dancer)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-038/221)RB";
        d.collector_number = 38;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.ability_text = R"RB(When I move to a battlefield, give another friendly unit +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92550ab70647046115133c7547423bcf308906f6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_361(CardRegistry& r) {
    r.registerCard(361, std::make_unique<RibbonDancer>());
}

} // namespace riftbound
