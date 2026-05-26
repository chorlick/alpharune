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

class VoidAssault : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move a friendly unit, then move an enemy unit. (If they both move to a
    //  battlefield you don't control, you're the attacker.)"
    // The agent now picks a destination (the controller's base, or any
    // battlefield) that both moved units go to, instead of always forcing base.
    // ENGINE GAP: the parenthetical "you're the attacker" designation when both
    // units land on an enemy battlefield is NOT applied — there is no per-card
    // hook to designate the controller as the attacker / open a showdown
    // mid-resolution. Movement itself is faithful; only the attacker-status
    // rider is unimplemented.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() < 2) return;
        // Build destination menu: base + each battlefield.
        std::vector<std::optional<BattlefieldId>> dests;  // nullopt = base
        std::vector<std::string> labels;
        dests.push_back(std::nullopt); labels.push_back("Your base");
        for (const auto& bf : ctx.state.battlefields) {
            dests.push_back(bf.id);
            labels.push_back("BF#" + std::to_string(static_cast<int>(bf.id)));
        }
        uint32_t mask = (dests.size() >= 32) ? 0xFFFFFFFFu : ((1u << dests.size()) - 1);
        int chosen = pickMode(ctx, "Void Assault: move both units to where?",
                              static_cast<int>(dests.size()), labels, mask);
        if (chosen == -1) return;  // waiting on agent
        if (chosen < 0 || static_cast<size_t>(chosen) >= dests.size()) chosen = 0;
        auto move = [&](GameObjectId u) {
            if (!ctx.state.objectExists(u)) return;
            if (dests[chosen].has_value()) ctx.executor.moveToBattlefield(u, *dests[chosen]);
            else ctx.executor.moveToBase(u);
        };
        move(targets[0]);   // friendly
        move(targets[1]);   // enemy
        ctx.events.logTrace("VOID ASSAULT: moved both units (attacker designation "
                            "not engine-applied)");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        // All units on the board; engine generates friendly/enemy pairs.
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            targets.push_back(id);
        }
        return targets;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 758;
        d.def_id = R"RB(unl-202-219)RB";
        d.name = R"RB(Void Assault)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-202/219)RB";
        d.collector_number = 202;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Kha'Zix)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Move a friendly unit, then move an enemy unit. (If they both move to a battlefield you don't control, you're the attacker.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0b79108a9492fb34866cc63815b57bfab8a5aeae-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_758(CardRegistry& r) {
    r.registerCard(758, std::make_unique<VoidAssault>());
}

} // namespace riftbound
