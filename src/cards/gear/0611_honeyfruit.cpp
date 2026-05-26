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

    // "This enters exhausted."
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.exhaustObject(ctx.source);
    }

    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            // Ability 0: [Reaction] [E]: [Add] [A].
            {
                .cost = {.exhaust = true},
                .targets = {},
                .is_action = false, .is_reaction = true,
            },
            // Ability 1: [Level 6] [Reaction] [E]: [Add] [1][A].
            {
                .cost = {.exhaust = true},
                .targets = {},
                .is_action = false, .is_reaction = true,
            },
        };
    }
    // Neither ability has board targets.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& /*state*/, PlayerId /*controller*/,
        int /*ability_index*/) const override {
        return {};
    }
    // NOTE: [Level 6] gates ONLY ability 1. The engine does not consult
    // requiresLevel()/levelThreshold() for activated-ability legality, so the
    // 6+ XP gate is enforced inline in onActivate for ability 1.
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (ability_index == 1) {
            // [Level 6] gate: only with 6+ XP.
            if (ctx.state.player(ctx.controller).xp < 6) return;
            ctx.executor.addFloatingUniversalPower(ctx.controller, 2);  // [1][A]
            ctx.events.logTrace("HONEYFRUIT: [Add] [1][A]");
        } else {
            ctx.executor.addFloatingUniversalPower(ctx.controller, 1);  // [A]
            ctx.events.logTrace("HONEYFRUIT: [Add] [A]");
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
        d.keywords.set(Keyword::Reaction);
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
