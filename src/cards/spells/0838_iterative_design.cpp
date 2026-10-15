#include "cards/card.h"
#include "cards/card_helpers.h"
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

class IterativeDesign : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Play a 3 [M] Mech unit token." / "[Flow] [2][B]"
    // No location is printed, so the token is played like any unit: to your
    // base or to a battlefield you control (mode 0 = base, 1 + i = battlefield i).
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& bfs = ctx.state.battlefields;
        const int nbf = static_cast<int>(std::min<size_t>(bfs.size(), 31));
        uint32_t mask = 1u;
        std::vector<std::string> labels = {"Base"};
        for (int i = 0; i < nbf; ++i) {
            std::string name = "BF#" + std::to_string(static_cast<int>(bfs[i].id.raw()));
            if (ctx.state.objectExists(bfs[i].card_object_id))
                name = ctx.state.getObject(bfs[i].card_object_id).name;
            labels.push_back(name);
            if (bfs[i].controller.has_value() && *bfs[i].controller == ctx.controller)
                mask |= (1u << (i + 1));
        }
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        int mode = pickMode(ctx, "Iterative Design: where to play the Mech", nbf + 1, labels, mask);
        if (mode < 0 || mode > nbf) return;   // -1 waiting for the agent
        LocationId loc = BaseLocation{ctx.controller};
        if (mode >= 1) loc = BattlefieldLocation{bfs[mode - 1].id};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", /*might=*/3,
                                 {"Mech"}, KeywordSet{}, loc, /*enter_ready=*/false);
    }
    FlowCost flowCost() const override { return flow(2, 1, Domain::Mind); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 838;
        d.def_id = R"RB(ven-051-166)RB";
        d.name = R"RB(Iterative Design)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-051/166)RB";
        d.collector_number = 51;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.rarity = Rarity::Common;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB(Play a 3 [M] Mech unit token.
[Flow] [2][B] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8d85a05075f95b63832ed7aa15620041eacb49d3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_838(CardRegistry& r) {
    r.registerCard(838, std::make_unique<IterativeDesign>());
}

}  // namespace riftbound
