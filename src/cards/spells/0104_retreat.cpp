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

class Retreat : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        PlayerId owner = ctx.state.getObject(targets[0]).owner;
        ctx.executor.bounceToHand(targets[0]);
        // "Its owner channels 1 rune exhausted."
        ctx.executor.channelRunes(owner, 1, /*enter_exhausted=*/true);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 104;
        d.def_id = R"RB(ogn-104-298)RB";
        d.name = R"RB(Retreat)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-104/298)RB";
        d.collector_number = 104;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Return a friendly unit to its owner's hand. Its owner channels 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/71ecf6c06d3110a81ac0f01c726c06921f479a13-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_104(CardRegistry& r) {
    r.registerCard(104, std::make_unique<Retreat>());
}

} // namespace riftbound
