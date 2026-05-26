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

class FindYourCenter : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        // "If an opponent's score is within 3 points of the Victory Score,
        // this costs [2] less."
        int vs = state.mode.victory_score;
        if (state.player(opponent(player)).score >= vs - 3) return 2;
        return 0;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Draw 1 and channel 1 rune exhausted."
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 47;
        d.def_id = R"RB(ogn-047-298)RB";
        d.name = R"RB(Find Your Center)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-047/298)RB";
        d.collector_number = 47;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 3;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
If an opponent's score is within 3 points of the Victory Score, this costs [2] less.
Draw 1 and channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cd9d798b496fdce133c5b0106d636a230b6e7ebe-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_47(CardRegistry& r) {
    r.registerCard(47, std::make_unique<FindYourCenter>());
}

} // namespace riftbound
