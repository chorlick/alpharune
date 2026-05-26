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

class BountyHunter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = false,
            .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    /// The engine's action-gen path calls this to gate the ability:
    /// if it returns empty, the ability isn't offered. The base impl
    /// reads `getTargetRequirements()` (legacy single-ability surface)
    /// which BountyHunter doesn't supply — so without this override
    /// the engine sees count=0, returns {}, and silently drops the
    /// ability. The same call drives `pickTarget` inside `onActivate`
    /// when production code routes through needs_activation_time_target.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_idx*/) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit()) continue;
            if (!obj.location.has_value()) continue;
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        // Backward-compat: direct-invocation tests pre-supply targets.
        // Production uses pickTarget.
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Bounty Hunter", legal);
        }
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryKeyword(picked, Keyword::Ganking, 0);
        ctx.events.logTrace("BOUNTY HUNTER: gives Ganking to " +
                             ctx.state.getObject(picked).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 262;
        d.def_id = R"RB(ogn-267-298)RB";
        d.name = R"RB(Bounty Hunter)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-267/298)RB";
        d.collector_number = 267;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Miss Fortune)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([E]: Give a unit [Ganking] this turn. (It can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cc11261fcdbf0851525030bd9e835b718efad3bc-744x1040.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_262(CardRegistry& r) {
    r.registerCard(262, std::make_unique<BountyHunter>());
}

} // namespace riftbound
