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

class ArenaKingpin : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Clause 1: "I enter ready."
    bool entersReadyOnPlay() const override { return true; }

    // Clause 2: "[E]: Give a unit +3 [M] this turn."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        if (!ctx.state.objectExists(targets[0])) return;
        ctx.executor.giveTemporaryMight(targets[0], 3);
        ctx.events.logTrace("ARENA KINGPIN: [E] -> +3 [M] this turn to target unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 557;
        d.def_id = R"RB(unl-001-219)RB";
        d.name = R"RB(Arena Kingpin)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-001/219)RB";
        d.collector_number = 1;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Yordle)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 3;
        d.ability_text = R"RB(I enter ready.
[E]: Give a unit +3 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/db48f8c1da0e4f9804ae94398f0864859db0002a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_557(CardRegistry& r) {
    r.registerCard(557, std::make_unique<ArenaKingpin>());
}

} // namespace riftbound
