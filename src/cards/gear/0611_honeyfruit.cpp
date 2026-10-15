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

class Honeyfruit : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "This enters exhausted." (read by GameEngine::resolvePermanent)
    bool entersExhausted() const override { return true; }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            // Ability 0: [Reaction] [E]: [Add] [A].
            {
                .cost = {.exhaust = true},
                .targets = {},
                .is_action = false, .is_reaction = true,
                .needs_activation_time_target = false,
                .is_empower = false,
                .adds_resources = true,   // resolves as it is activated (CR 430.2; D2)
            },
            // Ability 1: [Level 6] [Reaction] [E]: [Add] [1][A].
            {
                .cost = {.exhaust = true},
                .targets = {},
                .is_action = false, .is_reaction = true,
                .needs_activation_time_target = false,
                .is_empower = false,
                .adds_resources = true,   // resolves as it is activated (CR 430.2; D2)
            },
        };
    }
    // Neither ability has board targets.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& /*state*/, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        return {};
    }
    // "[Level 6] ... (Use this ability only while you have 6+ XP.)" gates
    // ONLY ability 1: it is not offered below 6 XP.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (ability_index == 1 && state.player(controller).xp < 6) return false;
        return GearCard::canActivateAbilityAt(state, controller, source, ability_index);
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (ability_index == 1) {
            if (ctx.state.player(ctx.controller).xp < 6) return;   // lost the Level in response
            // "[Add] [1][A]": one Energy and one Power of any domain.
            ctx.executor.addFloatingEnergy(ctx.controller, 1);
            ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
            RIFT_TRACE(ctx.events, "HONEYFRUIT: [Add] [1][A]");
        } else {
            ctx.executor.addFloatingUniversalPower(ctx.controller, 1);  // [A]
            RIFT_TRACE(ctx.events, "HONEYFRUIT: [Add] [A]");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 611;
        d.def_id = R"RB(unl-049-219)RB";
        d.name = R"RB(Honeyfruit)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-049/219)RB";
        d.collector_number = 49;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB(This enters exhausted.
[Reaction][>] [E]: [Add] [A]. (Abilities that add resources can't be reacted to.)
[Level 6][>] [>>][Reaction][>] [E]: [Add] [1][A]. (Use this ability only while you have 6+ XP.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4794052d16d88b12861b348a466673ace098c32e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_611(CardRegistry& r) {
    r.registerCard(611, std::make_unique<Honeyfruit>());
}

} // namespace riftbound
