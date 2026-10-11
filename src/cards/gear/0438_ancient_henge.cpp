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

class AncientHenge : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool isActionAbility() const override { return false; }   // [Reaction]
    bool isReactionAbility() const override { return true; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "Pay any amount of Energy to [Add] that much [A]." CR 445.2 / 164.2.a:
        // the Energy may be floating or come from exhausting ready runes, so X
        // ranges over everything payable (payableEnergy), not just the pool.
        // pickXAmount publishes one int-coded MakeChoice per X and the action
        // vocabulary has 16 int slots (0..15), so X is capped at 15.
        int max_x = std::min(payableEnergy(ctx.state, ctx.controller), 15);
        int x = pickXAmount(ctx, "Ancient Henge: pay X energy → Add X [A]",
                             0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            RIFT_TRACE(ctx.events, "ANCIENT HENGE: X=0, no conversion");
            return;
        }
        if (!payEnergy(ctx, ctx.controller, x)) return;
        ctx.executor.addFloatingUniversalPower(ctx.controller, x);
        RIFT_TRACE(ctx.events, "ANCIENT HENGE: paid " + std::to_string(x) +
                             " energy → +" + std::to_string(x) + " [A] power");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 438;
        d.def_id = R"RB(sfd-117-221)RB";
        d.name = R"RB(Ancient Henge)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-117/221)RB";
        d.collector_number = 117;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([E]: [Reaction] — Pay any amount of Energy to [Add] that much [A]. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c7c260b77b4174a61aed6243ed90f76594102193-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_438(CardRegistry& r) {
    r.registerCard(438, std::make_unique<AncientHenge>());
}

} // namespace riftbound
