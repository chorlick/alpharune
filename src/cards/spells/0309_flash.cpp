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

class Flash : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId controller = ctx.controller;
        auto isMovable = [](const GameObject& obj, PlayerId c) {
            return obj.isUnit() && obj.controller == c &&
                   obj.location.has_value() && !obj.isAtBase();
        };
        std::vector<GameObjectId> legal_a;
        for (auto& [id, obj] : ctx.state.objects) {
            if (isMovable(obj, controller)) legal_a.push_back(id);
        }

        // "Up to 2 friendly units" — modeled as a pair pick (second filtered to
        // exclude the first). pickTargetPair returns a partial {a, kInvalidId}
        // when fewer than two units are available, naturally handling 1 or 0.
        // GAP: pickTargetPair has no "decline" once targets exist, so when 2+
        // units are present the agent must move two (cannot move only one).
        auto legal_b_fn = [&ctx, controller, isMovable](GameObjectId picked_a) {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == picked_a) continue;
                if (isMovable(obj, controller)) out.push_back(id);
            }
            return out;
        };
        auto pair = pickTargetPair(ctx, "Flash: move up to 2 friendly units to base",
                                   legal_a, legal_b_fn);
        if (pair.first == kInvalidId && pair.second == kInvalidId &&
            ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point != 13) {
            return;  // suspended for agent input
        }
        if (pair.first != kInvalidId && ctx.state.objectExists(pair.first)) {
            ctx.executor.moveToBase(pair.first);
            ctx.events.logTrace("FLASH: moved " +
                                 ctx.state.getObject(pair.first).name + " to base");
        }
        if (pair.second != kInvalidId && ctx.state.objectExists(pair.second)) {
            ctx.executor.moveToBase(pair.second);
            ctx.events.logTrace("FLASH: moved " +
                                 ctx.state.getObject(pair.second).name + " to base");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 309;
        d.def_id = R"RB(ogs-011-024)RB";
        d.name = R"RB(Flash)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-011/024)RB";
        d.collector_number = 11;
        d.artist = R"RB(Sugar Free)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Move up to 2 friendly units to base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4d9cc1c13b75933e509e642213f13359350cd3f9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_309(CardRegistry& r) {
    r.registerCard(309, std::make_unique<Flash>());
}

} // namespace riftbound
