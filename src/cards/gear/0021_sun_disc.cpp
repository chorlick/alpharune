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

class SunDisc : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[E]: [Legion] — The next unit you play this turn enters ready."
    // The activated ability ARMS a one-shot; the WhenYouPlayAUnit trigger
    // readies the just-played unit and disarms (mirrors Nami, Headstrong).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool requiresLegion() const override { return true; }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters["__sundisc_arm_turn"] =
            ctx.state.turn.turn_number;
        ctx.events.logTrace("SUN DISC: armed next-unit-enters-ready for this turn");
    }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__sundisc_arm_turn");
        if (it == self.card_counters.end()) return;
        if (it->second != ctx.state.turn.turn_number) return;  // stale arm
        self.card_counters.erase(it);  // one-shot
        // Ready the just-played unit. The trigger carries no event object,
        // so pick among freshly-played (exhausted) friendly units.
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || !obj.is_exhausted) continue;
            legal.push_back(id);
        }
        if (legal.empty()) return;
        GameObjectId pick = pickTarget(ctx, "Sun Disc: ready the played unit", legal);
        if (pick == kInvalidId || !ctx.state.objectExists(pick)) return;
        ctx.executor.readyObject(pick);
        ctx.events.logTrace("SUN DISC: next unit entered ready");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 21;
        d.def_id = R"RB(ogn-021-298)RB";
        d.name = R"RB(Sun Disc)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-021/298)RB";
        d.collector_number = 21;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([E]: [Legion] — The next unit you play this turn enters ready. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/146d7514f15e6674f471f3aa9c3fadf22c0b634b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_21(CardRegistry& r) {
    r.registerCard(21, std::make_unique<SunDisc>());
}

} // namespace riftbound
