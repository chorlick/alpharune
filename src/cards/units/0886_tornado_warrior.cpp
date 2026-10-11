#include "cards/card.h"
#include "cards/card_helpers.h"
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

class TornadoWarrior : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // [Hidden] is engine-handled.
    // "When you play me from face down, you may empower something here.
    //  Disempower it at end of turn."
    // GameEngine::executePlayFromHidden stamps
    // card_counters["__played_from_facedown"] = 1; it is consumed here so a
    // later replay from hand does not re-trigger.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    std::vector<GameObjectId> empowerableHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return out;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() && !obj.isGear()) continue;
            if (!obj.location.has_value() || *obj.location != *self.location) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
            int cap = 1;
            if (ctx.registry && obj.card_def_id != kInvalidId)
                if (const Card* c = ctx.registry->get(obj.card_def_id)) cap = c->maxEmpower();
            if (obj.empowered >= cap) continue;   // empowering it would do nothing
            out.push_back(id);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        const auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point == 0) {
            // First entry: latch (and consume) the played-from-face-down marker.
            auto& from_facedown = self.card_counters["__played_from_facedown"];
            self.card_counters["__tornado_from_facedown"] = from_facedown == 1 ? 1 : 0;
            from_facedown = 0;
        }
        if (self.card_counters["__tornado_from_facedown"] != 1) return;
        int conf = confirmOptional(ctx, "Tornado Warrior: empower something here?",
                                   [&]() { return !empowerableHere(ctx).empty(); });
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Tornado Warrior: empower", empowerableHere(ctx));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.executor.empower(picked)) {
            ctx.state.getObject(picked).empower_flip_eot = -1;   // disempower at end of turn
            RIFT_TRACE(ctx.events, "TORNADO WARRIOR: empowered " +
                                ctx.state.getObject(picked).name + " until end of turn");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 886;
        d.def_id = R"RB(ven-099-166)RB";
        d.name = R"RB(Tornado Warrior)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-099/166)RB";
        d.collector_number = 99;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me from face down, you may empower something here. Disempower it at end of turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f9b2ffecc997670713bffd3168e5578b5fb7a014-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_886(CardRegistry& r) {
    r.registerCard(886, std::make_unique<TornadoWarrior>());
}

}  // namespace riftbound
