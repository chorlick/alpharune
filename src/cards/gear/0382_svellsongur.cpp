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

class Svellsongur : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "As this is attached to a unit, copy that unit's text to this
    // Equipment's effect text for as long as this is attached to it." (A copy
    // effect, CR 479.1.d.) An Equipment's effect text speaks for its bearer,
    // so while attached the bearer has each of its abilities a second time:
    //  - a triggered ability triggers once more (TriggerManager::fireTrigger);
    //  - a passive ability applies once more, and printed [Assault N] /
    //    [Shield N] / [Deflect N] count again (GameEngine::recalculateAuras);
    //  - a second copy of an activated ability adds nothing the first does
    //    not already allow (same cost, same source to exhaust), so nothing
    //    more is offered.
    // The engine asks copiesBearerText(); the copy ends when this comes off.
    bool copiesBearerText() const override { return true; }
    bool hasEquipAbility() const override { return true; }
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .energy = 1, .power = 1, .power_domain = Domain::Calm};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 382;
        d.def_id = R"RB(sfd-059-221)RB";
        d.name = R"RB(Svellsongur)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-059/221)RB";
        d.collector_number = 59;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][G] ([1][G]: Attach this to a unit you control.)
As this is attached to a unit, copy that unit's text to this Equipment's effect text for as long as this is attached to it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bc254398dfb5db217327b56862011a2fd6020789-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_382(CardRegistry& r) {
    r.registerCard(382, std::make_unique<Svellsongur>());
}

} // namespace riftbound
