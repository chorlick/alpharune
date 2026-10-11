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

class Switcheroo : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // A unit at a battlefield with another unit there.
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object,
            [&](const GameObject& u) {
                const auto bf = u.battlefieldId();
                if (!bf) return false;
                for (const auto& [id, o] : state.objects)
                    if (id != u.id && o.isUnit() && o.battlefieldId() == bf) return true;
                return false;
            });
    }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // chooseableUnits honours "can't be chosen by enemy spells and
        // abilities" for both picks.
        auto first_legal = chooseableUnits(ctx.state, ctx.controller,
            [](const GameObject& o) { return o.isAtBattlefield(); });
        auto second_fn = [&](GameObjectId picked_a) {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(picked_a)) return out;
            auto bf_a = ctx.state.getObject(picked_a).battlefieldId();
            if (!bf_a) return out;
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.id != picked_a && o.isAtBattlefield() && o.battlefieldId() == bf_a;
            });
        };
        auto [a, b] = pickTargetPair(ctx, "Switcheroo", first_legal, second_fn);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        bool suspending = (a == kInvalidId || b == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (a == kInvalidId || b == kInvalidId) return;
        if (!ctx.state.objectExists(a) || !ctx.state.objectExists(b)) return;
        int might_a = ctx.state.getObject(a).current_might;
        int might_b = ctx.state.getObject(b).current_might;
        // Swap: bump each toward the other's current Might (this turn).
        ctx.executor.giveTemporaryMight(a, might_b - might_a);
        ctx.executor.giveTemporaryMight(b, might_a - might_b);
        RIFT_TRACE(ctx.events, "SWITCHEROO: swapped Might " + std::to_string(might_a) +
                            " <-> " + std::to_string(might_b));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 466;
        d.def_id = R"RB(sfd-145-221)RB";
        d.name = R"RB(Switcheroo)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-145/221)RB";
        d.collector_number = 145;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Swap the Might of two units at the same battlefield this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a4338f495feff5eee46d5349b5fded5e35e76176-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_466(CardRegistry& r) {
    r.registerCard(466, std::make_unique<Switcheroo>());
}

} // namespace riftbound
