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

class MalzaharFanatic : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Kill a friendly unit or gear, [E]: [Action] — [Add] [A][A]."
    // Killing a friendly unit/gear is an additional cost; we resolve it
    // inside onActivate (the target is chosen at activation time).
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_friendly = true},
            .is_action = true, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }

    // Custom: friendly unit OR gear on the board (the default enumerator
    // ANDs must_be_unit + must_be_gear, which matches nothing).
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller, int /*ability_index*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value()) continue;
            if (obj.controller != controller) continue;
            if (!obj.isUnit() && !obj.isGear()) continue;
            out.push_back(id);
        }
        return out;
    }

    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Malzahar (kill friendly unit/gear)", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.killObject(picked);
        // [Add] [A][A] — two universal power.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 2);
        ctx.events.logTrace("MALZAHAR: killed a friendly unit/gear -> Add [A][A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 113;
        d.def_id = R"RB(ogn-113-298)RB";
        d.name = R"RB(Malzahar, Fanatic)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-113/298)RB";
        d.collector_number = 113;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Malzahar)RB", R"RB(The Void)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB(Kill a friendly unit or gear, [E]: [Action] — [Add] [A][A]. (Use on your turn or in showdowns. Abilities that add resources can't be reacted to.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ba1fa3a18b1c2ff132ad536577e53deb49bce1f9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_113(CardRegistry& r) {
    r.registerCard(113, std::make_unique<MalzaharFanatic>());
}

} // namespace riftbound
