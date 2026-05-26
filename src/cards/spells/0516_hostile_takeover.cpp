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

class HostileTakeover : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool needsPlayTimeTarget() const override { return true; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            if (obj.isAtBattlefield()) out.push_back(id);
        }
        return out;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) picked = targets[0];
        else picked = pickTarget(ctx, "Hostile Takeover: seize enemy unit",
                                 enumerateLegalTargets(ctx.state, ctx.controller));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.takeControl(picked, ctx.controller, /*until_end_of_turn=*/true);
        ctx.executor.readyObject(picked);
        ctx.events.logTrace("HOSTILE TAKEOVER: seized enemy unit until end of turn "
                            "(combat-start/conquer rider not modeled)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 516;
        d.def_id = R"RB(sfd-202-221)RB";
        d.name = R"RB(Hostile Takeover)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-202/221)RB";
        d.collector_number = 202;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
Take control of an enemy unit at a battlefield. Ready it. (Start a combat if other enemies are there. Otherwise, conquer.)
Lose control of that unit and recall it at end of turn. (Send it to base. This isn't a move.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/56651a87166df7108ce9f945bd71390d4926770a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_516(CardRegistry& r) {
    r.registerCard(516, std::make_unique<HostileTakeover>());
}

} // namespace riftbound
