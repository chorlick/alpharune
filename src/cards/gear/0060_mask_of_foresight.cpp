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

class MaskOfForesight : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a friendly unit attacks or defends alone, give it +1 [M] this turn."
    // Wired via WhenAUnitAttacksOrDefendsAlone (TriggerManager::onCombatStarted
    // fires this on the side's controller's cards when exactly one friendly unit
    // participates at the BF; the lone unit is the subject).
    TriggerType triggerType() const override {
        return TriggerType::WhenAUnitAttacksOrDefendsAlone;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId subject = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (subject == kInvalidId || !ctx.state.objectExists(subject)) return;
        ctx.executor.giveTemporaryMight(subject, 1);
        ctx.events.logTrace("MASK OF FORESIGHT: +1 [M] to the lone attacker/defender");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 60;
        d.def_id = R"RB(ogn-060-298)RB";
        d.name = R"RB(Mask of Foresight)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-060/298)RB";
        d.collector_number = 60;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a friendly unit attacks or defends alone, give it +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03824709acbb4151d13b083a842c4702a3e61221-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_60(CardRegistry& r) {
    r.registerCard(60, std::make_unique<MaskOfForesight>());
}

} // namespace riftbound
