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

class CounterStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (obj.isUnit() && obj.location.has_value()) out.push_back(id);
        }
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Counter Strike: prevent next damage",
                                 enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.state.getObject(picked).prevent_next_damage_this_turn = true;
            ctx.events.logTrace("COUNTER STRIKE: armed damage prevention on " +
                                ctx.state.getObject(picked).name);
        }
        ctx.executor.drawCards(ctx.controller, 1);  // "Draw 1" — unconditional
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 510;
        d.def_id = R"RB(sfd-194-221)RB";
        d.name = R"RB(Counter Strike)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-194/221)RB";
        d.collector_number = 194;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Jax)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a unit. The next time that unit would be dealt damage this turn, prevent it. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7c643b3da9a78518b1b92e3eeb0078b88d82bf79-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_510(CardRegistry& r) {
    r.registerCard(510, std::make_unique<CounterStrike>());
}

} // namespace riftbound
