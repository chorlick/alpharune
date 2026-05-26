#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class GuardianAngel : public SimpleEquipGear {
public:
    GuardianAngel() : SimpleEquipGear(Domain::Calm) {}
    const CardDef& def() const override { return def_; }
    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        const auto& gear = ctx.state.getObject(ctx.source);
        // Only intercept the death of the unit we're attached to.
        if (!gear.attached_to.has_value() || *gear.attached_to != dying_unit) {
            return false;
        }
        ctx.events.logTrace("GUARDIAN ANGEL: kill self, heal/exhaust/recall bearer");
        ctx.executor.unattachGear(ctx.source);
        ctx.executor.killObject(ctx.source);       // kill Guardian Angel instead
        ctx.executor.healObject(dying_unit);        // heal me
        ctx.executor.exhaustObject(dying_unit);     // exhaust me
        ctx.executor.moveToBase(dying_unit);        // recall me
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 374;
        d.def_id = R"RB(sfd-051-221)RB";
        d.name = R"RB(Guardian Angel)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-051/221)RB";
        d.collector_number = 51;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 2;
        d.might_bonus = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [G] ([G]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(If I would die, kill Guardian Angel instead. Heal me, exhaust me, and recall me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d09a797345659a1853d6d12910cf3c634990ea0c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_374(CardRegistry& r) {
    r.registerCard(374, std::make_unique<GuardianAngel>());
}

} // namespace riftbound
