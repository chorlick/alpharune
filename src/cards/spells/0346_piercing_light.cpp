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

class PiercingLight : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Repeat] [2][R]" (CR 820): the Repeat cost, as data the engine charges.
    std::vector<RepeatCost> repeatCosts() const override {
        return {RepeatCost{.energy = 2, .power = 1, .power_domain = Domain::Fury}};
    }
    // "Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit."
    // The two slots differ: the first must be at a battlefield, the second is
    // any other unit (a base unit included) and may be left out. The location
    // rule is therefore not in the requirement — it would be applied to both —
    // but in the target sets and in onResolve.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .optional = true};
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto id : enumerateLegalTargets(state, controller))
            if (state.getObject(id).isAtBattlefield()) return true;
        return false;
    }
    std::optional<std::vector<std::vector<GameObjectId>>>
    enumerateLegalTargetSets(const GameState& state, PlayerId controller) const override {
        auto units = enumerateLegalTargets(state, controller);
        std::sort(units.begin(), units.end());
        std::vector<std::vector<GameObjectId>> sets;
        for (auto first : units) {
            if (!state.getObject(first).isAtBattlefield()) continue;
            sets.push_back({first});
            for (auto second : units)
                if (second != first) sets.push_back({first, second});
        }
        return sets;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // The engine hands over only the targets that are still legal, which
        // loses the slot a survivor was chosen for; read the slots as chosen.
        const auto& chosen = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->targets : targets;
        auto stillLegal = [&](GameObjectId id) {
            return std::find(targets.begin(), targets.end(), id) != targets.end();
        };
        if (!chosen.empty() && stillLegal(chosen[0]) &&
            ctx.state.getObject(chosen[0]).isAtBattlefield())
            ctx.executor.dealDamage(chosen[0], 2, ctx.source);
        if (chosen.size() >= 2 && chosen[1] != chosen[0] && stillLegal(chosen[1]))
            ctx.executor.dealDamage(chosen[1], 2, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 346;
        d.def_id = R"RB(sfd-023-221)RB";
        d.name = R"RB(Piercing Light)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-023/221)RB";
        d.collector_number = 23;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Repeat);
        d.ability_text = R"RB([Repeat] [2][R] (You may pay the additional cost to repeat this spell's effect.)
Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7fb3283b11fb8e5b5b08f9ead9b98c695b75bff0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_346(CardRegistry& r) {
    r.registerCard(346, std::make_unique<PiercingLight>());
}

} // namespace riftbound
