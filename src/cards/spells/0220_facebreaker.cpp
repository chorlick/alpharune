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

class Facebreaker : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        // Two targets — friendly first, enemy second; the same-battlefield
        // pairing is enforced in pickTargetPair's legal_b_fn below.
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }

    // Playable only if some battlefield holds both a friendly and an enemy
    // unit (so a legal same-BF pair exists).
    bool hasLegalTargets(const GameState& state,
                          PlayerId controller) const override {
        for (auto& [fid, f] : state.objects) {
            if (!f.isUnit() || f.controller != controller) continue;
            auto fbf = f.battlefieldId();
            if (!fbf) continue;
            for (auto& [eid, e] : state.objects) {
                if (!e.isUnit() || e.controller == controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *fbf) return true;
            }
        }
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // First pick: a friendly unit at a battlefield that ALSO has a
        // valid enemy unit (otherwise the pair can't complete).
        std::vector<GameObjectId> legal_friendly;
        for (auto& [fid, f] : ctx.state.objects) {
            if (!f.isUnit() || f.controller != ctx.controller) continue;
            auto fbf = f.battlefieldId();
            if (!fbf) continue;
            bool enemy_here = false;
            for (auto& [eid, e] : ctx.state.objects) {
                if (!e.isUnit() || e.controller == ctx.controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *fbf) { enemy_here = true; break; }
            }
            if (enemy_here) legal_friendly.push_back(fid);
        }

        auto enemy_fn = [&ctx](GameObjectId picked_a) {
            std::vector<GameObjectId> legal_enemy;
            if (!ctx.state.objectExists(picked_a)) return legal_enemy;
            auto abf = ctx.state.getObject(picked_a).battlefieldId();
            if (!abf) return legal_enemy;
            for (auto& [eid, e] : ctx.state.objects) {
                if (!e.isUnit() || e.controller == ctx.controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *abf) legal_enemy.push_back(eid);
            }
            return legal_enemy;
        };

        auto [friendly, enemy] = pickTargetPair(ctx, "Facebreaker",
                                                  legal_friendly, enemy_fn);
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly != kInvalidId && ctx.state.objectExists(friendly)) {
            ctx.executor.stunUnit(friendly);
        }
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            ctx.executor.stunUnit(enemy);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 220;
        d.def_id = R"RB(ogn-220-298)RB";
        d.name = R"RB(Facebreaker)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-220/298)RB";
        d.collector_number = 220;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Stun a friendly unit and an enemy unit at the same battlefield. (They don't deal combat damage this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b7cddc717b886bb955b900ffaae4db9154a19280-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_220(CardRegistry& r) {
    r.registerCard(220, std::make_unique<Facebreaker>());
}

} // namespace riftbound
