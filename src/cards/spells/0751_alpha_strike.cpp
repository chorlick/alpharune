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

class AlphaStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId src = targets[0];
        int total = ctx.state.getObject(src).current_might;
        if (total <= 0) return;

        // Collect enemy units at battlefields BEFORE dealing/killing.
        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> enemies;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            enemies.push_back(id);
        }
        std::sort(enemies.begin(), enemies.end());
        if (enemies.empty()) return;

        // Round-robin spread `total` damage across the enemy units.
        std::vector<int> dmg(enemies.size(), 0);
        for (int i = 0; i < total; ++i) {
            dmg[i % enemies.size()] += 1;
        }
        for (size_t i = 0; i < enemies.size(); ++i) {
            if (dmg[i] <= 0) continue;
            if (!ctx.state.objectExists(enemies[i])) continue;
            ctx.executor.dealDamage(enemies[i], dmg[i], src);
        }

        // Kill lethally-damaged units; gain 1 XP per kill.
        int kills = 0;
        for (auto id : enemies) {
            if (!ctx.state.objectExists(id)) continue;
            if (ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
                ++kills;
            }
        }
        if (kills > 0) {
            ctx.state.player(ctx.controller).xp += kills;
            ctx.events.logTrace("ALPHA STRIKE: " + std::to_string(kills) +
                                " kill(s) -> gain " + std::to_string(kills) +
                                " XP (now " +
                                std::to_string(ctx.state.player(ctx.controller).xp) + ")");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 751;
        d.def_id = R"RB(unl-192-219)RB";
        d.name = R"RB(Alpha Strike)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-192/219)RB";
        d.collector_number = 192;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Master Yi)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly unit. It deals damage equal to its Might split among enemy units at battlefields. Then for each unit this kills, do this: Gain 1 XP.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/154940caa774fd8219625cf051ddcb230ffc7f02-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_751(CardRegistry& r) {
    r.registerCard(751, std::make_unique<AlphaStrike>());
}

} // namespace riftbound
