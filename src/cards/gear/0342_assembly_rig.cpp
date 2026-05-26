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

class AssemblyRig : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "[1][R], Recycle a unit from your trash, [E]: Play a 3 [M] Mech unit
    // token to your base." The [1][R] + [E] portion is the structured cost;
    // the "recycle a unit from your trash" additional cost is handled in
    // onActivate (and gated by canActivateAbility so the ability is only
    // offered when a unit is in trash).
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1, .power = 1,
                              .power_domain = Domain::Fury};
    }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        for (auto id : state.player(controller).trash) {
            if (!state.objectExists(id)) continue;
            if (state.getObject(id).isUnit()) return true;
        }
        return false;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Pay the "Recycle a unit from your trash" additional cost.
        auto& ps = ctx.state.player(ctx.controller);
        GameObjectId unit_in_trash = kInvalidId;
        for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
            if (!ctx.state.objectExists(*it)) continue;
            if (ctx.state.getObject(*it).isUnit()) { unit_in_trash = *it; break; }
        }
        if (unit_in_trash == kInvalidId) return;  // shouldn't happen (gated)
        auto trit = std::find(ps.trash.begin(), ps.trash.end(), unit_in_trash);
        if (trit != ps.trash.end()) ps.trash.erase(trit);
        ctx.executor.recycleCards(ctx.controller, {unit_in_trash});

        // Play a 3 [M] Mech unit token to base.
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech",
                                 /*might=*/3, /*tags=*/{"Mech"}, KeywordSet{},
                                 loc, /*enter_ready=*/false);
        ctx.events.logTrace("ASSEMBLY RIG: recycled a unit -> 3[M] Mech token to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 342;
        d.def_id = R"RB(sfd-019-221)RB";
        d.name = R"RB(Assembly Rig)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-019/221)RB";
        d.collector_number = 19;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([1][R], Recycle a unit from your trash, [E]: Play a 3 [M] Mech unit token to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/40ae6f61f1a0b5d65d91e3bb3fd2a0dad9148208-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_342(CardRegistry& r) {
    r.registerCard(342, std::make_unique<AssemblyRig>());
}

} // namespace riftbound
