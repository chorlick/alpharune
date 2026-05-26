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

class LastStand : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto& tgt = ctx.state.getObject(targets[0]);
        int bonus = tgt.current_might;       // snapshot before buff → doubling
        if (bonus > 0) ctx.executor.giveTemporaryMight(targets[0], bonus);
        // [Temporary]: set the keyword directly so it survives to the beginning
        // phase sweep (NOT giveTemporaryKeyword, which expires end of this turn).
        tgt.keywords.set(Keyword::Temporary);
        ctx.events.logTrace("LAST STAND: doubled Might this turn (+" +
                            std::to_string(bonus) + ") and gave [Temporary]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 69;
        d.def_id = R"RB(ogn-069-298)RB";
        d.name = R"RB(Last Stand)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-069/298)RB";
        d.collector_number = 69;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Double a friendly unit's Might this turn. Give it [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9062d372d299c5c6a0c679f0ff07ba71590ca5f1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_69(CardRegistry& r) {
    r.registerCard(69, std::make_unique<LastStand>());
}

} // namespace riftbound
