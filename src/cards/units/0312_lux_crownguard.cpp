#include "cards/card.h"
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

class LuxCrownguard : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[E]: [Reaction] — [Add] [2]. Use only to play spells."
    // The Energy is restricted: the engine spends RunePool::spell_energy
    // only on playing a spell.
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool isReactionAbility() const override { return true; }  // [Reaction], not [Action]
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).rune_pool.spell_energy += 2;
        ctx.events.logTrace("LUX CROWNGUARD: [Add] [2] Energy, spells only");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 312;
        d.def_id = R"RB(ogs-014-024)RB";
        d.name = R"RB(Lux, Crownguard)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-014/024)RB";
        d.collector_number = 14;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Lux)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB([E]: [Reaction] — [Add] [2]. Use only to play spells. (Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/17d0793ad495727e67bb1c94ae0e11cd4705870f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_312(CardRegistry& r) {
    r.registerCard(312, std::make_unique<LuxCrownguard>());
}

} // namespace riftbound
