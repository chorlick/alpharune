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

class TemporalPortal : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[A], [E]: Give the next spell you play this turn [Repeat] equal to its
    // cost." The engine's one-shot grant (PlayerState::
    // grant_repeat_base_to_next_spell, also The Academy's) gives the next
    // spell without a printed [Repeat] a Repeat cost equal to its base cost
    // (Energy and Power both, the Power payable in the spell's own Domains);
    // it is used up by the next spell played and cleared at end of turn.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [A] = one power of any domain, plus [E] = exhaust this gear.
        ActivationCost c;
        c.exhaust = true;
        c.power = 1;
        c.power_any_domain = true;
        return c;
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).grant_repeat_base_to_next_spell = true;
        ctx.events.logTrace("TEMPORAL PORTAL: next spell this turn has [Repeat] "
                            "equal to its cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 401;
        d.def_id = R"RB(sfd-078-221)RB";
        d.name = R"RB(Temporal Portal)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-078/221)RB";
        d.collector_number = 78;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB([A], [E]: Give the next spell you play this turn [Repeat] equal to its cost. (You may pay the additional cost to repeat the spell's effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a6b8167486d552cc41d10cb9321bae4cef338fa7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_401(CardRegistry& r) {
    r.registerCard(401, std::make_unique<TemporalPortal>());
}

} // namespace riftbound
