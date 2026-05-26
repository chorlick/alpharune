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

class Challenge : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        // Two targets — generic count=2 unit; actual side filters
        // applied per-pick in pickTargetPair's legal_a/legal_b_fn
        // closures below.
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    // Gate playability: must have at least one friendly unit AND one
    // enemy unit on the board.
    bool hasLegalTargets(const GameState& state,
                          PlayerId controller) const override {
        bool any_friendly = false, any_enemy = false;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller == controller) any_friendly = true;
            else if (!obj.untargetable_by_enemy) any_enemy = true;
        }
        return any_friendly && any_enemy;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal_friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_friendly.push_back(id);
        }
        auto enemy_fn = [&](GameObjectId /*picked_a*/) {
            std::vector<GameObjectId> legal_enemy;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value() || !obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.untargetable_by_enemy) continue;
                legal_enemy.push_back(id);
            }
            return legal_enemy;
        };
        auto [friendly, enemy] = pickTargetPair(ctx, "Challenge",
                                                  legal_friendly,
                                                  enemy_fn);
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly == kInvalidId || enemy == kInvalidId) return;
        if (!ctx.state.objectExists(friendly) || !ctx.state.objectExists(enemy)) return;

        int friendly_might = ctx.state.getObject(friendly).current_might;
        int enemy_might    = ctx.state.getObject(enemy).current_might;
        ctx.events.logTrace("CHALLENGE: " + ctx.state.getObject(friendly).name +
                             " (" + std::to_string(friendly_might) + "M) <-> " +
                             ctx.state.getObject(enemy).name +
                             " (" + std::to_string(enemy_might) + "M)");
        // Snapshot might before damage so we don't have order-dependent
        // outcomes (e.g. if friendly's damage drops enemy might).
        ctx.executor.dealDamage(enemy, friendly_might, ctx.source);
        ctx.executor.dealDamage(friendly, enemy_might, ctx.source);
        // Inline kill-on-lethal (test fixture bypasses cleanup).
        for (auto tid : {enemy, friendly}) {
            if (ctx.state.objectExists(tid) &&
                ctx.state.getObject(tid).hasLethalDamage()) {
                ctx.executor.killObject(tid);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 128;
        d.def_id = R"RB(ogn-128-298)RB";
        d.name = R"RB(Challenge)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-128/298)RB";
        d.collector_number = 128;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a friendly unit and an enemy unit. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5c1aeaee373c481b7ea2a638cd6f9e815ac105d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_128(CardRegistry& r) {
    r.registerCard(128, std::make_unique<Challenge>());
}

} // namespace riftbound
