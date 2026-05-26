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

class ZhonyaSHourglass : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        if (!ctx.state.objectExists(dying_unit)) return false;
        const auto& self = ctx.state.getObject(ctx.source);
        const auto& victim = ctx.state.getObject(dying_unit);
        // Only intercept a FRIENDLY unit's death.
        if (!victim.isUnit()) return false;
        if (victim.controller != self.controller) return false;
        ctx.events.logTrace("ZHONYA'S HOURGLASS: kill self instead of " +
                             victim.name + " — heal/exhaust/recall it");
        // Kill the gear instead.
        ctx.executor.killObject(ctx.source);
        // Heal, exhaust, and recall the saved unit to its base.
        ctx.executor.healObject(dying_unit);
        ctx.executor.exhaustObject(dying_unit);
        ctx.executor.moveToBase(dying_unit);
        return true;  // replacement consumed — the unit does not die
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 77;
        d.def_id = R"RB(ogn-077-298)RB";
        d.name = R"RB(Zhonya's Hourglass)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-077/298)RB";
        d.collector_number = 77;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
If a friendly unit would die, kill this instead. Heal that unit, exhaust it, and recall it. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/dc38172c56f838b407fc9f170ba973da32d7cd4d-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_77(CardRegistry& r) {
    r.registerCard(77, std::make_unique<ZhonyaSHourglass>());
}

} // namespace riftbound
