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

class NoxianEmissary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] [1][Y]"
    EmpowerSpec empowerSpec() const override { return empowerCost(1, 1, Domain::Order); }
    // "[Empowered][>][>>][Deathknell][>] Play two 1 [M] Recruit unit tokens to
    //  your base. (When I die while Empowered, get the effect.)"
    // Empowered is cleared once the unit is off the board, so this reads the
    // last-known information the kill paths snapshot (CR 383.2.c.3.b).
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        const bool was_empowered = self.last_known.valid ? self.last_known.empowered > 0
                                                         : self.isEmpowered();
        if (!was_empowered) return;
        for (int i = 0; i < 2; ++i)
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit", 1,
                                     {"Recruit"}, KeywordSet{},
                                     BaseLocation{ctx.controller}, /*enter_ready=*/false);
        RIFT_TRACE(ctx.events, "NOXIAN EMISSARY: died Empowered -> two 1[M] Recruits to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 915;
        d.def_id = R"RB(ven-128-166)RB";
        d.name = R"RB(Noxian Emissary)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-128/166)RB";
        d.collector_number = 128;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Empower] [1][Y] ([1][Y]: Empower me. Use only if not Empowered.)
[Empowered][>][>>][Deathknell][>] Play two 1 [M] Recruit unit tokens to your base. (When I die while Empowered, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0720c25776670ef32dcc504b14378890991798be-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_915(CardRegistry& r) {
    r.registerCard(915, std::make_unique<NoxianEmissary>());
}

}  // namespace riftbound
