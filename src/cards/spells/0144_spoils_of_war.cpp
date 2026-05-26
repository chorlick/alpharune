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

class SpoilsOfWar : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "If an enemy unit has died this turn, this costs [2] less."
    // APPROXIMATION: there is no turn-scoped death tracker in GameState, so
    // "has died this turn" is approximated as "an enemy unit is in the
    // opponent's trash" (over-approximates across turns; engine edit needed
    // for an exact this-turn gate). Reduce energy cost by 2 when true.
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        PlayerId opp = opponent(player);
        for (auto cid : state.player(opp).trash) {
            if (!state.objectExists(cid)) continue;
            if (state.getObject(cid).isUnit()) return 2;
        }
        return 0;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.drawCards(ctx.controller, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 144;
        d.def_id = R"RB(ogn-144-298)RB";
        d.name = R"RB(Spoils of War)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-144/298)RB";
        d.collector_number = 144;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
If an enemy unit has died this turn, this costs [2] less.
Draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b8d59d015d6b8e15822b360447fcb364e14d550f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_144(CardRegistry& r) {
    r.registerCard(144, std::make_unique<SpoilsOfWar>());
}

} // namespace riftbound
