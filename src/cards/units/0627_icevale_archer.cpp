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

class IcevaleArcher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I attack, you may pay [1] to give a unit here -1 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }

    std::vector<GameObjectId> unitsHere(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        if (!ctx.state.objectExists(ctx.source)) return out;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return out;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *my_bf) out.push_back(id);
        }
        return out;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&]() { return ps.rune_pool.energy >= 1 && !unitsHere(ctx).empty(); };
        int conf = confirmOptional(ctx, "Icevale Archer: pay [1] to give a unit here -1 [M]?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / can't
        ps.rune_pool.energy -= 1;
        GameObjectId tgt = pickTarget(ctx, "Icevale Archer: choose a unit here",
                                      unitsHere(ctx));
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.giveTemporaryMight(tgt, -1);
        ctx.events.logTrace("ICEVALE ARCHER: paid [1] -> gave a unit here -1 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 627;
        d.def_id = R"RB(unl-065-219)RB";
        d.name = R"RB(Icevale Archer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-065/219)RB";
        d.collector_number = 65;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When I attack, you may pay [1] to give a unit here -1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c14db2ae064ccee80d8ec373f9fe9b4f44776e3e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_627(CardRegistry& r) {
    r.registerCard(627, std::make_unique<IcevaleArcher>());
}

} // namespace riftbound
