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

// "As an additional cost to play this, kill a friendly unit. Play a unit from
// your trash that costs no more Energy and no more Power than the killed unit,
// ignoring its cost."
//
// The additional "kill a friendly unit" is modeled at resolve time (the engine
// has no structured non-resource additional-cost surface for spells). Pick the
// friendly unit to kill (A), then the trash unit to revive (B) gated to the
// killed unit's printed Energy/Power. The kill is applied after both picks.

class HeedlessResurrection : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    // Playable only if there is a friendly unit to kill.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller == controller) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // First pick: a friendly unit on the board to kill.
        std::vector<GameObjectId> legal_kill;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_kill.push_back(id);
        }
        // Second pick: a unit in trash whose printed cost is <= the killed
        // unit's printed cost (both Energy and Power).
        auto revive_fn = [&](GameObjectId killed) -> std::vector<GameObjectId> {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(killed)) return out;
            CardDefId killed_def = ctx.state.getObject(killed).card_def_id;
            if (killed_def == kInvalidId) return out;
            const auto& kdef = ctx.executor.cardDB().get(killed_def);
            int max_e = kdef.energy_cost;
            int max_p = kdef.power_cost;
            for (auto cid : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (!obj.isUnit() || obj.card_def_id == kInvalidId) continue;
                const auto& cdef = ctx.executor.cardDB().get(obj.card_def_id);
                if (cdef.energy_cost <= max_e && cdef.power_cost <= max_p) {
                    out.push_back(cid);
                }
            }
            return out;
        };
        auto [killed, revived] = pickTargetPair(ctx, "Heedless Resurrection",
                                                 legal_kill, revive_fn);
        bool suspending = (killed == kInvalidId || revived == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        // Pay the additional cost: kill the chosen friendly unit.
        if (killed != kInvalidId && ctx.state.objectExists(killed)) {
            ctx.events.logTrace("HEEDLESS RESURRECTION: killing " +
                                 ctx.state.getObject(killed).name + " (additional cost)");
            ctx.executor.killObject(killed);
        }
        // Play the revived unit from trash, ignoring its cost.
        if (revived == kInvalidId || !ctx.state.objectExists(revived)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), revived);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, revived);
        ctx.events.logTrace("HEEDLESS RESURRECTION: played a unit from trash, ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 704;
        d.def_id = R"RB(unl-142-219)RB";
        d.name = R"RB(Heedless Resurrection)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-142/219)RB";
        d.collector_number = 142;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, kill a friendly unit.
Play a unit from your trash that costs no more Energy and no more Power than the killed unit, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92419d138824a7d46155b59f0e5903196d68d9f7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_704(CardRegistry& r) {
    r.registerCard(704, std::make_unique<HeedlessResurrection>());
}

} // namespace riftbound
