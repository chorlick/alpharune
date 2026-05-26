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

// "[Reaction] Choose a friendly unit and a spell. Counter that spell and give
// that unit +[M] equal to that spell's Energy cost this turn."

class Riposte : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // [Reaction] timing comes from the CardDef. Defer the friendly-unit
    // target choice to resolve time (counters target the chain top).
    bool needsPlayTimeTarget() const override { return true; }

    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }

    // Only playable if there is a counterable spell on the chain.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        bool has_spell = false;
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell && it->card_def_id != kInvalidId) { has_spell = true; break; }
        }
        if (!has_spell) return false;
        // Also need a friendly unit to buff.
        for (auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.controller == controller && obj.location.has_value())
                return true;
        }
        return false;
    }

    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.controller == controller && obj.location.has_value())
                out.push_back(id);
        }
        return out;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Read the energy cost of the spell to counter (chain top) BEFORE
        // countering it.
        int cost = 0;
        if (!ctx.state.chain.items.empty()) {
            auto& top = ctx.state.chain.items.back();
            if (top.is_spell && top.card_def_id != kInvalidId)
                cost = ctx.executor.cardDB().get(top.card_def_id).energy_cost;
        }

        // Pick the friendly unit to buff (resolve-time target choice). This
        // may suspend (returns kInvalidId, caller must return immediately);
        // on re-entry it returns the chosen unit. We do this BEFORE the
        // counter so the counter runs exactly once (after the unit is known).
        GameObjectId unit = kInvalidId;
        if (!targets.empty()) {
            unit = targets[0];
        } else {
            unit = pickTarget(ctx, "Riposte: buff a friendly unit",
                              enumerateLegalTargets(ctx.state, ctx.controller));
            if (unit == kInvalidId) return;  // suspended or no legal unit
        }

        // Counter the spell on top of the chain.
        counterChainTop(ctx);

        // Give the chosen unit +[M] equal to that spell's Energy cost this turn.
        if (unit != kInvalidId && ctx.state.objectExists(unit) && cost > 0) {
            ctx.executor.giveTemporaryMight(unit, cost);
            ctx.events.logTrace("RIPOSTE: countered spell -> +" +
                                std::to_string(cost) + " [M] this turn");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 520;
        d.def_id = R"RB(sfd-206-221)RB";
        d.name = R"RB(Riposte)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-206/221)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Fiora)RB"};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit and a spell. Counter that spell and give that unit +[M] equal to that spell's Energy cost this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/07af52eada661904b467ca118c2715435f0a3b00-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_520(CardRegistry& r) {
    r.registerCard(520, std::make_unique<Riposte>());
}

} // namespace riftbound
