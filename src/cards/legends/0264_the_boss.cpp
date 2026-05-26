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

class TheBoss : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // ── Part 2: "When you conquer, ready me." ──
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        // NOTE: "When YOU conquer" (any friendly conquer), but legends have no
        // location so WhenIConquer firing on the legend itself is the available
        // hook; engine fires legend conquer/hold triggers via the legend-zone
        // sweep. Ready the legend.
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("THE BOSS: conquer -> ready me");
    }

    // ── Part 1: buffed-unit death replacement ──
    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(ctx.source)) return false;
        if (!ctx.state.objectExists(dying_unit)) return false;
        auto& self = ctx.state.getObject(ctx.source);
        auto& victim = ctx.state.getObject(dying_unit);
        // Only a BUFFED FRIENDLY unit, and only while I'm ready (must "exhaust
        // me" to pay).
        if (!victim.isUnit()) return false;
        if (victim.controller != self.controller) return false;
        if (victim.buff_count <= 0) return false;
        if (self.is_exhausted) return false;  // can't exhaust me again
        // Pay: exhaust me, spend one of its buffs ([C] cost not charged here —
        // see header note).
        self.is_exhausted = true;
        victim.buff_count -= 1;
        victim.recomputeMight();
        ctx.events.logTrace("THE BOSS: save buffed unit " + victim.name +
                            " (exhaust me, spend buff) -> heal/exhaust/recall");
        // Heal, exhaust, recall to base.
        ctx.executor.healObject(dying_unit);
        ctx.executor.exhaustObject(dying_unit);
        ctx.executor.moveToBase(dying_unit);
        return true;  // replacement consumed — the unit does not die
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 264;
        d.def_id = R"RB(ogn-269-298)RB";
        d.name = R"RB(The Boss)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-269/298)RB";
        d.collector_number = 269;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Sett)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(If a buffed unit you control would die, you may pay [C], exhaust me, and spend its buff to heal it, exhaust it, and recall it instead. (Send it to base. This isn't a move.)
When you conquer, ready me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b132becb843b2cf418cb110ead64758f49f51554-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_264(CardRegistry& r) {
    r.registerCard(264, std::make_unique<TheBoss>());
}

} // namespace riftbound
