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

class LeBlancEverywhereAtOnce : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Clause 1: "[Backline]" — engine keyword, set in def. Fully handled by
    //   the combat-damage assignment ordering.
    // Clause 2: "Your [Temporary] effects at my battlefield don't trigger."
    //   There is no engine hook to suppress a Temporary unit's triggered
    //   abilities scoped to a battlefield: TriggerManager fires triggers
    //   unconditionally, with no per-location / per-keyword suppression
    //   filter, and there is no "triggers suppressed here" flag on
    //   BattlefieldState or GameObject.
    // ESCALATE(trigger-suppression): need a battlefield-scoped suppression of
    //   triggered abilities from friendly [Temporary] units (consulted by
    //   TriggerManager before firing a trigger whose source is a Temporary
    //   unit at a battlefield where a controller's LeBlanc resides).
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 652;
        d.def_id = R"RB(unl-090-219)RB";
        d.name = R"RB(LeBlanc, Everywhere at Once)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-090/219)RB";
        d.collector_number = 90;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Noxus)RB", R"RB(LeBlanc)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Backline);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Backline] (I must be assigned combat damage last.)
Your [Temporary] effects at my battlefield don't trigger.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a0389576206b1e750a0852c6ff46efd2646d47da-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_652(CardRegistry& r) {
    r.registerCard(652, std::make_unique<LeBlancEverywhereAtOnce>());
}

} // namespace riftbound
