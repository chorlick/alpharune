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

class Isolate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move an enemy unit from a battlefield to its base. Then, if there's an
    //  enemy unit alone at that battlefield, draw 1."
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId tgt = targets[0];
        auto from_bf = ctx.state.getObject(tgt).battlefieldId();
        ctx.executor.moveToBase(tgt);
        if (!from_bf) return;

        // After the move, count enemy units remaining at that battlefield.
        PlayerId opp = opponent(ctx.controller);
        int enemy_here = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == tgt) continue;
            if (!obj.isUnit() || obj.controller != opp || !obj.location.has_value()) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == *from_bf) enemy_here++;
        }
        if (enemy_here == 1) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("ISOLATE: an enemy unit is alone at that BF -> draw 1");
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true,
                                  .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 686;
        d.def_id = R"RB(unl-124-219)RB";
        d.name = R"RB(Isolate)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-124/219)RB";
        d.collector_number = 124;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.ability_text = R"RB(Move an enemy unit from a battlefield to its base. Then, if there's an enemy unit alone at that battlefield, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bde23f07e4869fa96b14fcab329e782894b1e1e3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_686(CardRegistry& r) {
    r.registerCard(686, std::make_unique<Isolate>());
}

} // namespace riftbound
