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

class DangerousDuo : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;
        if (!targets.empty())
            ctx.executor.giveTemporaryMight(targets[0], 2);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool requiresLegion() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 16;
        d.def_id = R"RB(ogn-016-298)RB";
        d.name = R"RB(Dangerous Duo)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-016/298)RB";
        d.collector_number = 16;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, give a unit +2 [M] this turn. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c02ea9e438d407c739276b788e015ac93843651-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_16(CardRegistry& r) {
    r.registerCard(16, std::make_unique<DangerousDuo>());
}

} // namespace riftbound
