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

class Unforgiven : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.moveToBase(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true, .energy = 2}; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 256;
        d.def_id = R"RB(ogn-259-298)RB";
        d.name = R"RB(Unforgiven)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-259/298)RB";
        d.collector_number = 259;
        d.artist = R"RB(TSWCK逍)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB([2], [E]: Move a friendly unit to or from its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/68e4d3230b785738ae9d86f780f7f5607ef11807-744x1040.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_256(CardRegistry& r) {
    r.registerCard(256, std::make_unique<Unforgiven>());
}

} // namespace riftbound
