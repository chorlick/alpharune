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

class MelNewlyAwakened : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play me, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "MEL, NEWLY AWAKENED: played -> draw 1");
    }
    // "[Empower] [3]"
    EmpowerSpec empowerSpec() const override { return empowerCost(3); }
    // "[Empowered][>] Your spells and abilities can't be countered. If a spell
    // or ability you control would give -[M] to a unit it chooses, it gives an
    // additional -1 [M]."
    // The -[M] half is applied by EffectExecutor::giveTemporaryMight (cards
    // must not add extra_minus_might themselves); the counter half is honoured
    // by card_helpers.h isUncounterable(), which every counter effect asks
    // (counterChosenItem, Defy, Hard Bargain, Flurry of Feathers, Abandon).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller,
            [&](GameObjectId, GameObject&) {
                auto& ps = state.player(controller);
                ps.spells_cant_be_countered = true;
                ps.extra_minus_might += 1;
            });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 856;
        d.def_id = R"RB(ven-069-166)RB";
        d.name = R"RB(Mel, Newly Awakened)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-069/166)RB";
        d.collector_number = 69;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Noxus)RB", R"RB(Mel)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB(When you play me, draw 1.
[Empower] [3] ([3]: Empower me. Use only if not Empowered.)
[Empowered][>] Your spells and abilities can't be countered. If a spell or ability you control would give -[M] to a unit it chooses, it gives an additional -1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7d794234d8efef9ad647046d1e2cd3736441899f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_856(CardRegistry& r) {
    r.registerCard(856, std::make_unique<MelNewlyAwakened>());
}

}  // namespace riftbound
