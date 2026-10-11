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

class RenektonBrute : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1]: Give me +1 [M] this turn."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.energy = 1;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 1);
        RIFT_TRACE(ctx.events, "RENEKTON, BRUTE: [1] -> +1 [M] this turn");
    }
    // "When my Might becomes 10 or more, empower me." The engine fires
    // WhenMyMightReachesThreshold each time his Might goes from below
    // mightThreshold() to that or more, whatever changed it.
    int mightThreshold() const override { return 10; }
    TriggerType triggerType() const override {
        return TriggerType::WhenMyMightReachesThreshold;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // "(I become Empowered if I'm not already.)"
        if (!self.location.has_value() || self.isEmpowered()) return;
        if (ctx.executor.empower(ctx.source))
            RIFT_TRACE(ctx.events, "RENEKTON, BRUTE: Might 10 or more -> Empowered");
    }
    // "[Empowered][>] I have [Deflect] and [Ganking]." (No printed [Empower]:
    // he is Empowered by other effects.)
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller,
            [](GameObjectId sid, GameObject& self) {
                auraKeyword(self, sid, Keyword::Deflect, 1);
                auraKeyword(self, sid, Keyword::Ganking);
            });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 879;
        d.def_id = R"RB(ven-092-166)RB";
        d.name = R"RB(Renekton, Brute)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-092/166)RB";
        d.collector_number = 92;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Shurima)RB", R"RB(Renekton)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([1]: Give me +1 [M] this turn.
When my Might becomes 10 or more, empower me. (I become Empowered if I'm not already.)
[Empowered][>] I have [Deflect] and [Ganking]. (Opponents must pay [A] to choose me with a spell or ability. I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0b4340e30e46f1784b31b9a110903825b7428a94-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_879(CardRegistry& r) {
    r.registerCard(879, std::make_unique<RenektonBrute>());
}

}  // namespace riftbound
