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

class BaccaiWitherclaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Empower] [1][A][A]"
    EmpowerSpec empowerSpec() const override {
        return empowerCost(1, 2, Domain::Fury, /*any_domain=*/true);
    }
    // "[Empowered][>] I have +2 [M]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller,
            [](GameObjectId sid, GameObject& self) { auraMight(self, sid, 2); });
    }
    // "[Empowered][>][>>][Deathknell][>] Channel 2 runes exhausted. (When I die
    // while Empowered, get the effect.)"
    // Empowered is cleared once the unit is off the board, so this reads the
    // last-known information the kill paths snapshot (CR 383.2.c.3.b).
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        const bool died_empowered = self.last_known.valid ? self.last_known.empowered > 0
                                                          : self.isEmpowered();
        if (!died_empowered) return;
        ctx.executor.channelRunes(ctx.controller, 2, /*enter_exhausted=*/true);
        RIFT_TRACE(ctx.events, "BACCAI WITHERCLAW: died Empowered -> channel 2 runes exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 865;
        d.def_id = R"RB(ven-078-166)RB";
        d.name = R"RB(Baccai Witherclaw)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-078/166)RB";
        d.collector_number = 78;
        d.artist = R"RB(Will Gist)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Empower] [1][A][A] ([1][A][A]: Empower me. Use only if not Empowered.)
[Empowered][>] I have +2 [M].
[Empowered][>][>>][Deathknell][>] Channel 2 runes exhausted. (When I die while Empowered, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fa41ae964411e63643025f7d8914627849ff57a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_865(CardRegistry& r) {
    r.registerCard(865, std::make_unique<BaccaiWitherclaw>());
}

}  // namespace riftbound
