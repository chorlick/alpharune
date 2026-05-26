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

class Detonate : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.killObject(targets[0]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 328;
        d.def_id = R"RB(sfd-005-221)RB";
        d.name = R"RB(Detonate)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-005/221)RB";
        d.collector_number = 5;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.ability_text = R"RB(Kill a gear. Its controller draws 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e837fbd6aeeb1293386dcfa284aae5a4baca10ef-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_328(CardRegistry& r) {
    r.registerCard(328, std::make_unique<Detonate>());
}

} // namespace riftbound
