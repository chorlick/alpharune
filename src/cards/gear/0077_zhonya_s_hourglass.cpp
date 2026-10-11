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
    Applies replacementAppliesTo(const GameState& state, GameObjectId self_object,
                                 PlayerId controller, GameObjectId dying_unit) const override {
        return conditionsHold(state, self_object, controller, dying_unit) ? Applies::Yes
                                                                          : Applies::No;
    }
    bool conditionsHold(const GameState& state, GameObjectId self, PlayerId /*controller*/,
                        GameObjectId dying_unit) const {
        if (!state.objectExists(self) || !state.objectExists(dying_unit)) return false;
        const auto& victim = state.getObject(dying_unit);
        return victim.isUnit() && victim.controller == state.getObject(self).controller;
    }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        // Only a FRIENDLY unit's death (replacementApplies).
        if (!replacementApplies(ctx.state, ctx.source, ctx.controller, dying_unit)) return false;
        const auto& victim = ctx.state.getObject(dying_unit);
        RIFT_TRACE(ctx.events, "ZHONYA'S HOURGLASS: kill self instead of " +
                             victim.name + " — heal/exhaust/recall it");
        // Kill the gear instead.
        ctx.executor.killObject(ctx.source);
        // Heal, exhaust, and recall the saved unit to its base.
        // "(Send it to base. This isn't a move.)" — CR 457-460.
        ctx.executor.healExhaustRecall(dying_unit);
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
