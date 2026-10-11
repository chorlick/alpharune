#include "cards/card.h"
#include "cards/card_helpers.h"
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

class TwilightStep : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "Move a unit with 3 [M] or less." The unit is the spell's target, chosen
    // as it is played (the engine's play-time choice, which pays Deflect and
    // records the choice); where it moves — its controller's base or any
    // other battlefield — is chosen as it resolves.
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .max_might = 3};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const auto locked = lockedPlayTarget(ctx, targets);
        const GameObjectId u = locked ? *locked
            : pickTarget(ctx, "Twilight Step: move a unit with 3 [M] or less",
                         enumerateLegalTargets(ctx.state, ctx.controller));
        if (u == kInvalidId || !ctx.state.objectExists(u)) return;   // waiting, or no target
        const auto dests = moveDests(ctx.state, ctx.state.getObject(u));
        const int n = 1 + static_cast<int>(ctx.state.battlefields.size());
        std::vector<std::string> labels;
        uint32_t mask = 0;
        for (int d = 0; d < n; ++d) labels.push_back(destLabel(d));
        for (int d : dests) if (d >= 0 && d < 32) mask |= (1u << d);
        const int dest = pickMode(ctx, "Twilight Step: move it where?", n, labels, mask);
        if (dest < 0) return;   // -1 waiting, -2 nowhere to move
        moveTo(ctx, u, dest);
    }
    // "[Flow] [4][P]"
    FlowCost flowCost() const override { return flow(4, 1, Domain::Chaos); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 892;
        d.def_id = R"RB(ven-105-166)RB";
        d.name = R"RB(Twilight Step)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-105/166)RB";
        d.collector_number = 105;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Move a unit with 3 [M] or less.
[Flow] [4][P] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1c5c10009a9fb2605df5db1ff12647a47ac07d98-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_892(CardRegistry& r) {
    r.registerCard(892, std::make_unique<TwilightStep>());
}

}  // namespace riftbound
