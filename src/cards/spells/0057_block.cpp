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

class Block : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty()) {
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Shield, 3);
            ctx.executor.giveTemporaryKeyword(targets[0], Keyword::Tank, 1);
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 57;
        d.def_id = R"RB(ogn-057-298)RB";
        d.name = R"RB(Block)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-057/298)RB";
        d.collector_number = 57;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 3;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Give a unit [Shield 3] and [Tank] this turn. (+3 [M] while it's a defender. It must be assigned combat damage first.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/36f7352e715fe073631ed85be41408c9a38ab865-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_57(CardRegistry& r) {
    r.registerCard(57, std::make_unique<Block>());
}

} // namespace riftbound
