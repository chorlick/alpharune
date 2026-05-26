#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class NotSoFast : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Counter an enemy spell/ability that targets a friendly UNIT or GEAR.
    // Not playable unless the chain top matches.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        if (state.chain.items.empty()) return false;
        const auto& top = state.chain.items.back();
        if (top.controller == controller) return false;
        for (auto tid : top.targets) {
            if (!state.objectExists(tid)) continue;
            const auto& t = state.getObject(tid);
            if (t.controller == controller && (t.isUnit() || t.isGear())) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (top.controller == ctx.controller) return;
        bool targets_friendly = false;
        for (auto tid : top.targets) {
            if (!ctx.state.objectExists(tid)) continue;
            if (ctx.state.getObject(tid).controller == ctx.controller) {
                targets_friendly = true; break;
            }
        }
        if (!targets_friendly) return;
        counterChainTop(ctx);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 368;
        d.def_id = R"RB(sfd-045-221)RB";
        d.name = R"RB(Not So Fast)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-045/221)RB";
        d.collector_number = 45;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Counter an enemy spell or ability that chooses a friendly unit or gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e576fefdce72ad862a7e374bf8c0fd509f359da9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_368(CardRegistry& r) {
    r.registerCard(368, std::make_unique<NotSoFast>());
}

} // namespace riftbound
