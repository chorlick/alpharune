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

class EnGarde : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId tid = targets[0];
        auto& tgt = ctx.state.getObject(tid);

        // Base +1 M this turn.
        ctx.executor.giveTemporaryMight(tid, 1);

        // Additional +1 M if it is the ONLY unit the controller has at its
        // location ("there"). Count friendly units sharing the target's
        // location (only meaningful when the unit is on board somewhere).
        if (!tgt.location.has_value()) return;
        int friendly_here = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location != *tgt.location) continue;
            friendly_here++;
        }
        if (friendly_here == 1) {
            ctx.executor.giveTemporaryMight(tid, 1);
            ctx.events.logTrace("EN GARDE: only unit there -> additional +1 M");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 46;
        d.def_id = R"RB(ogn-046-298)RB";
        d.name = R"RB(En Garde)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-046/298)RB";
        d.collector_number = 46;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 1;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Give a friendly unit +1 [M] this turn, then an additional +1 [M] this turn if it is the only unit you control there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/03c0ec7eaa5957a62869dad2a0ce40913fc874a9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_46(CardRegistry& r) {
    r.registerCard(46, std::make_unique<EnGarde>());
}

} // namespace riftbound
