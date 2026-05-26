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
    // cost."
    // The activation cost ([A] = 1 power + [E] = exhaust) is fully modeled. The
    // EFFECT — granting the *next* spell played this turn the Repeat keyword
    // with a magnitude equal to its own cost — has no engine primitive: there
    // is no "grant Repeat to the next spell" hook, and the play-action
    // generator does not emit Repeat-cost variants for player choice
    // (game_state.h notes "the play-action generator does NOT emit Repeat
    // intents"). Granting it here would be a no-op, so we surface the gap
    // rather than fake it.
    // ESCALATE(next-spell-Repeat-grant): need a per-player "next spell played
    // this turn gains [Repeat] with magnitude = its cost" deferred modifier
    // plus action-generator support for paying the optional Repeat cost.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [A] = one power of any domain (encoded as the card's domain, Mind)
        // plus [E] = exhaust this gear.
        return {.exhaust = true, .power = 1, .power_domain = Domain::Mind};
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        // Effect unimplemented — see ESCALATE note above. Cost is still paid by
        // the engine via getActivationCost(); we only lack the grant primitive.
        ctx.events.logTrace("TEMPORAL PORTAL: [A][E] activated — next-spell "
                            "[Repeat] grant not engine-supported (ESCALATE)");
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
        d.keywords.set(Keyword::Repeat);
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
