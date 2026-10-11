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

class LeonaDetermined : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }

    // "When I attack, stun an enemy unit here."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const PlayerId me = ctx.controller;
        const auto here = ctx.state.getObject(ctx.source).battlefieldId();
        std::vector<GameObjectId> enemies_here;
        if (here.has_value())
            enemies_here = chooseableUnits(ctx.state, me, [&](const GameObject& o) {
                return o.controller != me && o.battlefieldId() == here;
            });
        std::sort(enemies_here.begin(), enemies_here.end());   // stable option order, not hash order
        const GameObjectId t = pickTarget(ctx, "Leona, Determined: stun an enemy unit here",
                                          enemies_here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.stunUnitBy(t, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 238;
        d.def_id = R"RB(ogn-238-298)RB";
        d.name = R"RB(Leona, Determined)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-238/298)RB";
        d.collector_number = 238;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Leona)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
When I attack, stun an enemy unit here. (It doesn't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/39a1e121031cfb4bdb009b071ed5d70411966cd9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_238(CardRegistry& r) {
    r.registerCard(238, std::make_unique<LeonaDetermined>());
}

} // namespace riftbound
