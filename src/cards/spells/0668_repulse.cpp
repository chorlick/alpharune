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

class Repulse : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // Counter an enemy spell/ability that targets a friendly UNIT. Not
    // playable unless the chain top matches that shape.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        if (state.chain.items.empty()) return false;
        const auto& top = state.chain.items.back();
        if (top.controller == controller) return false;
        for (auto tid : top.targets) {
            if (!state.objectExists(tid)) continue;
            const auto& t = state.getObject(tid);
            if (t.controller == controller && t.isUnit()) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (top.controller == ctx.controller) return;  // not enemy
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
        d.id = 668;
        d.def_id = R"RB(unl-106-219)RB";
        d.name = R"RB(Repulse)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-106/219)RB";
        d.collector_number = 106;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 1;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose a friendly unit at a battlefield. Counter an enemy spell or ability that chooses it and no other friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ca56ad44ec24db67e10a4ee18f5c7f3756c83d94-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_668(CardRegistry& r) {
    r.registerCard(668, std::make_unique<Repulse>());
}

} // namespace riftbound
