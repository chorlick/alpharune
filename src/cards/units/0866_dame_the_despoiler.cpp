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

class DameTheDespoiler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "[Empower] [5][O]"
    EmpowerSpec empowerSpec() const override { return empowerCost(5, 1, Domain::Body); }
    // "[Empowered][>] When I attack or defend, choose a unit here. Increase my
    // Might to its Might this turn, then give me +1 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    // "[Empowered][>]": I have the ability only while I have the status (CR
    // 828.1.b.1), so it triggers only for an Empowered Dame — no ability goes
    // on the chain otherwise.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).isEmpowered();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf = ctx.state.getObject(ctx.source).battlefieldId();
        // Empowered is not asked again here: it was the trigger condition
        // (triggerCondition), and an ability that is on the chain resolves
        // even if I have been disempowered since (CR 383.2.a.1: "it will
        // still resolve"). Not at a battlefield: no unit is "here" (CR
        // 359.3.f.2). No return ahead of pickTarget — it stores "no target"
        // on the chain item.
        std::vector<GameObjectId> here;
        if (bf)
            here = chooseableUnits(ctx.state, ctx.controller,
                [&](const GameObject& o) {
                    auto obf = o.battlefieldId();
                    return obf.has_value() && *obf == *bf;
                });
        std::sort(here.begin(), here.end());
        auto picked = pickTarget(ctx, "Dame the Despoiler: choose a unit here", here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        const int theirs = ctx.state.getObject(picked).current_might;
        const int mine = ctx.state.getObject(ctx.source).current_might;
        if (theirs > mine) ctx.executor.giveTemporaryMight(ctx.source, theirs - mine);
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "DAME THE DESPOILER: Might raised to " + std::to_string(std::max(theirs, mine)) +
                            " then +1 this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 866;
        d.def_id = R"RB(ven-079-166)RB";
        d.name = R"RB(Dame the Despoiler)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-079/166)RB";
        d.collector_number = 79;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [5][O] ([5][O]: Empower me. Use only if not Empowered.)
[Empowered][>] When I attack or defend, choose a unit here. Increase my Might to its Might this turn, then give me +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7c45dc79e72efc8ddf8c5b20d83474e23dc66e5e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_866(CardRegistry& r) {
    r.registerCard(866, std::make_unique<DameTheDespoiler>());
}

}  // namespace riftbound
