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

class Switcheroo : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isActionAbility() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> first_legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
            first_legal.push_back(id);
        }
        auto second_fn = [&](GameObjectId picked_a) {
            std::vector<GameObjectId> out;
            if (!ctx.state.objectExists(picked_a)) return out;
            auto bf_a = ctx.state.getObject(picked_a).battlefieldId();
            if (!bf_a) return out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == picked_a) continue;
                if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
                if (obj.battlefieldId() != bf_a) continue;
                out.push_back(id);
            }
            return out;
        };
        auto [a, b] = pickTargetPair(ctx, "Switcheroo", first_legal, second_fn);
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
        ctx.events.logTrace("SWITCHEROO: swapped Might " + std::to_string(might_a) +
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
