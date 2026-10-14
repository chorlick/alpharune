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

class NasusAscended : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // [Deflect 2] is data (def_.deflect_value).
    // "[Empower] [8]"
    EmpowerSpec empowerSpec() const override { return empowerCost(8); }
    // "[Empowered][>] When I conquer, you score 1 point."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    // [Empowered][>] "When I conquer, you score 1 point": Active only while
    // I'm Empowered, and only an Active ability triggers (CR 727.1.c.1):
    // judged at the conquer. Disempowered in response, it still scores.
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        return state.objectExists(source) && state.getObject(source).isEmpowered();
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // CR 194.1.c; scorePoints honours "can't gain points" (Tianna Crownguard).
        if (ctx.executor.scorePoints(ctx.controller, 1) == 0) return;
        const auto& ps = ctx.state.player(ctx.controller);
        RIFT_TRACE(ctx.events, "NASUS, ASCENDED: Empowered conquer -> score 1 point -> " +
                            std::to_string(ps.score));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 833;
        d.def_id = R"RB(ven-046-166)RB";
        d.name = R"RB(Nasus, Ascended)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-046/166)RB";
        d.collector_number = 46;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Dog)RB", R"RB(Shurima)RB", R"RB(Nasus)RB"};
        d.energy_cost = 8;
        d.power_cost = 1;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Deflect);
        d.deflect_value = 2;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or ability.)
[Empower] [8] ([8]: Empower me. Use only if not Empowered.)
[Empowered][>] When I conquer, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d9e9cc17be6aebf33ee1db42b515ad87ba8ee8c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_833(CardRegistry& r) {
    r.registerCard(833, std::make_unique<NasusAscended>());
}

}  // namespace riftbound
