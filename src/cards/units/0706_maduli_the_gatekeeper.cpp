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

class MaduliTheGatekeeper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "I can't be readied."
    // ENGINE GAP: there is no GameObject "can't be readied" flag, and
    // EffectExecutor::readyObject (plus the phase-readying paths) have no hook
    // to suppress readying for a specific unit. Enforcing this requires engine
    // support. Left unimplemented.

    // "[P]: Move me to an occupied enemy battlefield if my Might is greater
    //  than the total Might of enemy units there." ([P] = one Chaos Power.)
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.power = 1, .power_domain = Domain::Chaos};
    }
    // Legality: there must be at least one qualifying enemy battlefield.
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            return !qualifyingBfs(state, controller, obj.current_might, id).empty();
        }
        return false;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int my_might = ctx.state.getObject(ctx.source).current_might;
        auto bfs = qualifyingBfs(ctx.state, ctx.controller, my_might, ctx.source);
        if (bfs.empty()) return;
        std::vector<std::string> labels;
        for (auto bf : bfs) labels.push_back("BF#" + std::to_string(static_cast<int>(bf)));
        uint32_t mask = (bfs.size() >= 32) ? 0xFFFFFFFFu : ((1u << bfs.size()) - 1);
        int chosen = pickMode(ctx, "Maduli: move to which enemy battlefield",
                              static_cast<int>(bfs.size()), labels, mask);
        if (chosen == -1) return;  // waiting on agent
        if (chosen < 0 || static_cast<size_t>(chosen) >= bfs.size()) chosen = 0;
        ctx.executor.moveToBattlefield(ctx.source, bfs[chosen]);
        ctx.events.logTrace("MADULI: moved to occupied enemy battlefield");
    }

private:
    // Battlefields that have enemy units AND where `my_might` > total enemy
    // Might there. Excludes the battlefield I'm already at.
    static std::vector<BattlefieldId> qualifyingBfs(const GameState& state,
                                                     PlayerId controller, int my_might,
                                                     GameObjectId self) {
        PlayerId opp = opponent(controller);
        std::optional<BattlefieldId> my_bf;
        if (state.objectExists(self)) my_bf = state.getObject(self).battlefieldId();
        std::vector<BattlefieldId> out;
        for (const auto& bf : state.battlefields) {
            if (my_bf && *my_bf == bf.id) continue;
            int enemy_might = 0;
            bool occupied = false;
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != opp) continue;
                auto ubf = obj.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                occupied = true;
                enemy_might += obj.current_might;
            }
            if (occupied && my_might > enemy_might) out.push_back(bf.id);
        }
        return out;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 706;
        d.def_id = R"RB(unl-144-219)RB";
        d.name = R"RB(Maduli the Gatekeeper)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-144/219)RB";
        d.collector_number = 144;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I can't be readied.
[P]: Move me to an occupied enemy battlefield if my Might is greater than the total Might of enemy units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/20f1b46435845de99987e37125e0ef7bd61c00bb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_706(CardRegistry& r) {
    r.registerCard(706, std::make_unique<MaduliTheGatekeeper>());
}

} // namespace riftbound
