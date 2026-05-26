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

class StandUnited : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Buff a friendly unit. Buffs give an additional +1 [M] to friendly units
    //  this turn."
    // ENGINE GAP: there is no buff-amplification hook (a per-player field that
    // makes subsequent buffs grant +2 [M] this turn), so only the on-resolve
    // buff is applied. The amplification rider is unmodeled.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.buffUnit(targets[0]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 53;
        d.def_id = R"RB(ogn-053-298)RB";
        d.name = R"RB(Stand United)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-053/298)RB";
        d.collector_number = 53;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Buff a friendly unit. Buffs give an additional +1 [M] to friendly units this turn. (To buff a unit, give it a +1 [M] buff if it doesn't already have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d10570e70a998de520d4baba254c3b726caa4f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_53(CardRegistry& r) {
    r.registerCard(53, std::make_unique<StandUnited>());
}

} // namespace riftbound
