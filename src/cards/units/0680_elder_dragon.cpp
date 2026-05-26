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

class ElderDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto opp = opponent(ctx.controller);

        // Build the list of LOCATIONS to hit: opponent's base + every
        // battlefield. Card text "at each location" is the canonical
        // Riftbound term — see CR 151 / 144.4 for the location-vs-zone
        // distinction.
        std::vector<LocationId> locations;
        locations.reserve(ctx.state.battlefields.size() + 1);
        locations.push_back(BaseLocation{opp});
        for (auto& bf : ctx.state.battlefields) {
            locations.push_back(BattlefieldLocation{bf.id});
        }

        for (const auto& loc : locations) {
            auto enemies = ctx.state.unitsAt(loc, opp);
            if (enemies.empty()) continue;
            // Pick the first enemy at this location. CR-faithful "up to
            // one" with agent choice would route through pickTarget;
            // first-enemy is a placeholder that matches the prior
            // battlefield-only behaviour.
            auto victim = enemies.front();
            ctx.executor.dealDamage(victim, 1, ctx.source);
            // Two complementary kill paths: (1) inline natural-lethal
            // check for units already at 0–1 might (caught here in the
            // trigger), (2) GameEngine::processLethalDamage during the
            // post-trigger cleanup applies the Elder Dragon
            // "any-damage-lethal" aura to higher-might enemies. Both
            // are needed: this trigger is sometimes invoked outside
            // the full engine flow (per-card tests), so inline kill
            // catches the easy case; cleanup catches the aura case
            // when the engine drives it for real.
            if (ctx.state.objectExists(victim) &&
                ctx.state.getObject(victim).hasLethalDamage()) {
                ctx.executor.killObject(victim);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 680;
        d.def_id = R"RB(unl-118-219)RB";
        d.name = R"RB(Elder Dragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-118/219)RB";
        d.collector_number = 118;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Demacia)RB"};
        d.energy_cost = 12;
        d.power_cost = 4;
        d.might = 10;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Any amount of your damage is enough to kill enemy units.
When you play me, choose up to one enemy unit at each location. Deal 1 to them.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/fcf578e447d4e2785c8b25f3c39928b98e7b44ca-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_680(CardRegistry& r) {
    r.registerCard(680, std::make_unique<ElderDragon>());
}

} // namespace riftbound
