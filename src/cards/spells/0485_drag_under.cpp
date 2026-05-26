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

class DragUnder : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        // "anywhere other than your hand" — current play source is tracked on
        // the player. Hand = no reduction; any other source = [2] less.
        if (state.player(player).current_play_source != Intent::PlaySource::Hand)
            return 2;
        return 0;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            ctx.executor.killObject(targets[0]);
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_at_battlefield = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 485;
        d.def_id = R"RB(sfd-164-221)RB";
        d.name = R"RB(Drag Under)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-164/221)RB";
        d.collector_number = 164;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
I cost [2] less to play from anywhere other than your hand.
Kill a unit at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2400881960412ada4c1d1066105f206b02f8998a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_485(CardRegistry& r) {
    r.registerCard(485, std::make_unique<DragUnder>());
}

} // namespace riftbound
