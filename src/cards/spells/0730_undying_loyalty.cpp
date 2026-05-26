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

// "This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
//  Play a unit with cost no more than [2] and no more than [A] from your
//  trash, ignoring its cost."
//
// The revival target lives in the trash → defer to resolve-time pickTarget.
// Eligible = unit with printed Energy cost <= 2 (the "no more than [A]" Power
// clause is approximated as no Power gate — no structured Power-budget hook).
// Cost reduction ([2] less) is applied when at least one eligible target is a
// Bird/Cat/Dog/Poro (approximation: the discount is committed at play time,
// before the agent's exact choice; over-grants only if the agent then picks a
// non-tagged unit while a tagged one was available).

class UndyingLoyalty : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }

    static bool eligible(const GameState& state, const EffectExecutor& exec,
                         const GameObject& obj) {
        if (!obj.isUnit() || obj.card_def_id == kInvalidId) return false;
        return exec.cardDB().get(obj.card_def_id).energy_cost <= 2;
    }

    int selfCostReduction(const GameState& state, PlayerId player) const override {
        // We can't see the executor here; energy cost comes from CardDB via
        // the static def. Conservatively grant the [2] discount if a tagged
        // eligible unit exists in trash.
        const auto& ps = state.player(player);
        for (auto cid : ps.trash) {
            if (!state.objectExists(cid)) continue;
            const auto& obj = state.getObject(cid);
            if (!obj.isUnit()) continue;
            // Energy gate re-checked at resolve; tag check here.
            if (hasTag(obj, "Bird") || hasTag(obj, "Cat") ||
                hasTag(obj, "Dog") || hasTag(obj, "Poro")) {
                return 2;
            }
        }
        return 0;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (eligible(ctx.state, ctx.executor, ctx.state.getObject(cid)))
                legal.push_back(cid);
        }
        if (legal.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Undying Loyalty (unit from trash)", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("UNDYING LOYALTY: played a unit from trash, ignoring cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 730;
        d.def_id = R"RB(unl-168-219)RB";
        d.name = R"RB(Undying Loyalty)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-168/219)RB";
        d.collector_number = 168;
        d.artist = R"RB(Max Grecke)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(This costs [2] less if you choose a Bird, Cat, Dog, or Poro.
Play a unit with cost no more than [2] and no more than [A] from your trash, ignoring its cost.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/703b85f8284ed13865012289d03602e2cd24f4b2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_730(CardRegistry& r) {
    r.registerCard(730, std::make_unique<UndyingLoyalty>());
}

} // namespace riftbound
