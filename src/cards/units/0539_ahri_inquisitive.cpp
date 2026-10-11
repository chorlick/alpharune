#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

// "When I attack or defend, give an enemy unit here -2 [M] this turn, to a
//  minimum of 1 [M]."
class AhriInquisitive_539 : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "an enemy unit here": at my battlefield, and one I may choose.
        std::vector<GameObjectId> legal;
        if (ctx.state.objectExists(ctx.source)) {
            auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
            if (my_bf)
                legal = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                    return u.controller != ctx.controller && u.battlefieldId() == my_bf;
                });
        }
        const GameObjectId t = pickTarget(ctx, "Ahri, Inquisitive: give an enemy unit here -2 [M]",
                                          legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        ctx.executor.giveTemporaryMight(t, -2, /*minimum=*/1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 539;
        d.def_id = R"RB(sfd-227-221)RB";
        d.name = R"RB(Ahri, Inquisitive)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-227/221)RB";
        d.collector_number = 227;
        d.artist = R"RB(Shawn Tan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        // Tags as printed on SFD-227 (cards/raw/gallery_raw.json): the Spiritforged
        // reprint is not a copy of the Origins def — OGN-119 (card 119) also prints Ionia.
        d.tags = {R"RB(Ahri)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When I attack or defend, give an enemy unit here -2 [M] this turn, to a minimum of 1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d4c332e90c97e5e70b996598cd4a0f731b77fdef-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_539(CardRegistry& r) {
    r.registerCard(539, std::make_unique<AhriInquisitive_539>());
}

} // namespace riftbound
