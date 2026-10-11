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

class Glowstone : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "[Empower] [A][A]"
    EmpowerSpec empowerSpec() const override {
        return empowerCost(0, 2, Domain::Fury, /*any_domain=*/true);
    }
    // "Disempower this, [E]: Choose a player. They gain control of this and
    //  recall it. (Send it to their base.)" (ability 0; Empower is appended as 1)
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        c.disempower_self = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const int who = pickMode(ctx, "Glowstone: choose a player", 2, {"You", "Opponent"});
        if (who < 0) return;   // waiting on the agent
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // left the board
        const PlayerId chosen = who == 0 ? ctx.controller : opponent(ctx.controller);
        ctx.executor.takeControl(ctx.source, chosen, /*until_end_of_turn=*/false);
        // Recalled on its own: an attached card cannot change location apart
        // from its unit (CR 718.5.c), so it comes off first.
        ctx.executor.unattachGear(ctx.source);
        ctx.executor.recallToBase(ctx.source);
        RIFT_TRACE(ctx.events, std::string("GLOWSTONE: ") + toString(chosen) +
                            " gains control of it and recalls it");
    }
    // "At the end of your turn, kill this and deal 5 to all units you control."
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Collect first: killing / damaging fires events that can touch the map.
        std::vector<GameObjectId> units;
        for (const auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                units.push_back(id);
        std::sort(units.begin(), units.end());
        ctx.executor.killObject(ctx.source);
        for (auto id : units) ctx.executor.dealDamage(id, 5, ctx.source);
        // Lethally damaged units are killed by the engine's cleanup.
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 920;
        d.def_id = R"RB(ven-133-166)RB";
        d.name = R"RB(Glowstone)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-133/166)RB";
        d.collector_number = 133;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [A][A] ([A][A]: Empower me. Use only if not Empowered.)
Disempower this, [E]: Choose a player. They gain control of this and recall it. (Send it to their base.)
At the end of your turn, kill this and deal 5 to all units you control.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6426e9c2b168f0dde5430d5bc57108a0cc2bcebb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_920(CardRegistry& r) {
    r.registerCard(920, std::make_unique<Glowstone>());
}

}  // namespace riftbound
