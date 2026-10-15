#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class Firestorm : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Deal 3 to all enemy units at a battlefield." The battlefield is the
    // target, not the units (CR 355.10.b, 355.10.d: "Kill all units at a
    // battlefield" targets a battlefield), chosen as the spell is played (CR
    // 355.5), in its finalize pass before its cost: no unit's [Deflect] is
    // owed, and the enemy units there as it resolves are dealt 3 (testing
    // phase R8: a unit stood for the battlefield).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // a battlefield, not a unit
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const int mode = pickBattlefield(*this, ctx, "Firestorm: choose a battlefield");
        if (mode == -1) return;               // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mode < 0 || static_cast<size_t>(mode) >= ctx.state.battlefields.size()) return;
        const BattlefieldId bf = ctx.state.battlefields[static_cast<size_t>(mode)].id;

        // Collect all enemy units at the chosen battlefield, then deal-then-kill.
        std::vector<GameObjectId> to_damage;
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (obj.battlefieldId() != std::optional<BattlefieldId>{bf}) continue;
            to_damage.push_back(id);
        }
        std::sort(to_damage.begin(), to_damage.end());
        for (auto id : to_damage)
            ctx.executor.dealDamage(id, 3, ctx.source);
        {   // the batch is for these kills only
            ctx.executor.killLethallyDamagedTogether(to_damage);   // they die at the same time
        }
        RIFT_TRACE(ctx.events, "FIRESTORM: 3 to all enemy units at one battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 300;
        d.def_id = R"RB(ogs-002-024)RB";
        d.name = R"RB(Firestorm)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-002/024)RB";
        d.collector_number = 2;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Deal 3 to all enemy units at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a7a34129e64f0296bf2da166c2b06ed156d568db-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_300(CardRegistry& r) {
    r.registerCard(300, std::make_unique<Firestorm>());
}

} // namespace riftbound
