#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class HextechAnomaly : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    // [Add]: it resolves as it is activated — no chain item, no priority,
    // Focus stays (CR 430.2, 401.2; testing phase D2).
    bool abilityAddsResources() const override { return true; }
    bool isReactionAbility() const override { return true; }   // "[E]: [Reaction] —"
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "Pay any amount of [A] to [Add] that much Energy." [A] is paid the
        // way the engine pays it (CR 164.2.b): floating Power, then recycled
        // runes, ready or exhausted — so X ranges over payableAnyPower.
        // pickXAmount publishes one int-coded MakeChoice per X and the action
        // vocabulary has 16 int slots (0..15), so X is capped at 15.
        const int max_x = std::min(ctx.executor.payableAnyPower(ctx.controller), 15);
        const int x = pickXAmount(ctx, "Hextech Anomaly: pay X [A] → Add X energy", 0, max_x);
        if (x < 0) return;        // pending choice
        if (x == 0) { RIFT_TRACE(ctx.events, "HEXTECH ANOMALY: X=0"); return; }
        if (!ctx.executor.payAnyPower(ctx.controller, x)) return;
        ctx.executor.addFloatingEnergy(ctx.controller, x);
        RIFT_TRACE(ctx.events, "HEXTECH ANOMALY: paid " + std::to_string(x) +
                             " [A] → +" + std::to_string(x) + " energy");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 405;
        d.def_id = R"RB(sfd-083-221)RB";
        d.name = R"RB(Hextech Anomaly)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-083/221)RB";
        d.collector_number = 83;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([E]: [Reaction] — Pay any amount of [A] to [Add] that much Energy. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/32a46943778755e2b6210e5adb4507780f08e2c8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_405(CardRegistry& r) {
    r.registerCard(405, std::make_unique<HextechAnomaly>());
}

} // namespace riftbound
