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
        // "Pay any amount of Energy to [Add] that much [A]." Spend X energy
        // from the rune pool, then Add X universal power.
        auto& ps = ctx.state.player(ctx.controller);
        int max_x = ps.rune_pool.energy;
        int x = pickXAmount(ctx, "Ancient Henge: pay X energy → Add X [A]",
                             0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            ctx.events.logTrace("ANCIENT HENGE: X=0, no conversion");
            return;
        }
        ps.rune_pool.energy -= x;
        ctx.executor.addFloatingUniversalPower(ctx.controller, x);
        ctx.events.logTrace("ANCIENT HENGE: paid " + std::to_string(x) +
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
        d.keywords.set(Keyword::Reaction);
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
