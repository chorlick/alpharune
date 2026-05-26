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

// "[Deflect 2] ... When I conquer after an attack, if you assigned 5 or more
//  excess damage to enemy units, you may deal that much to an enemy unit."

class SivirAmbitious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // ENGINE GAP: the amount of excess combat damage assigned to enemy
        // units is not surfaced to triggers (same gap as Tryndamere / Yeti
        // Brawler). Unlike those cards, Sivir's payoff SCALES with that amount
        // ("deal that much"), so it can't be faithfully approximated with a
        // fixed value. Left unimplemented pending excess-damage tracking.
        ctx.events.logTrace("SIVIR AMBITIOUS: conquer (excess-damage amount not "
                            "engine-tracked -> payoff skipped)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 441;
        d.def_id = R"RB(sfd-120-221)RB";
        d.name = R"RB(Sivir, Ambitious)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-120/221)RB";
        d.collector_number = 120;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Sivir)RB", R"RB(Shurima)RB"};
        d.energy_cost = 6;
        d.power_cost = 3;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.deflect_value = 2;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect 2] (Opponents must pay [A][A] to choose me with a spell or Ability.)
When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you may deal that much to an enemy unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bd3f242c9ca59cfc2d7dc5b146877c66bb310bb7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_441(CardRegistry& r) {
    r.registerCard(441, std::make_unique<SivirAmbitious>());
}

} // namespace riftbound
