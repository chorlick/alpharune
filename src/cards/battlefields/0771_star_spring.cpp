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

class StarSpring : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        // Find the just-played unit: the newest unit controlled by ctx.controller
        // that sits at this battlefield and is non-token. (Trigger carries no
        // played-object id, so we self-select.)
        GameObjectId played = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.isToken()) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != here) continue;
            if (id > played || played == kInvalidId) played = id;  // newest id heuristic
        }
        if (played == kInvalidId) return;

        // First-time-this-turn gate, per player.
        auto& self = ctx.state.getObject(ctx.source);
        std::string key = "__starspring_p" + std::to_string(playerIndex(ctx.controller));
        if (self.card_counters[key] == ctx.state.turn.turn_number &&
            ctx.state.turn.turn_number != 0) {
            return;  // already triggered this turn for this player
        }

        // Build "another unit they control here" set (exclude the played one).
        std::vector<GameObjectId> movable;
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == played) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) movable.push_back(id);
        }

        int decision = confirmOptional(ctx, "Star Spring: move another unit here to base",
            [&]() { return !movable.empty(); });
        if (decision == -1) return;  // waiting on agent
        // Mark first-play handled regardless of yes/no (the "first time" is
        // consumed by reaching the decision).
        self.card_counters[key] = ctx.state.turn.turn_number;
        if (decision == 0) return;

        // Re-pick a still-legal movable unit and move it to its base.
        for (auto id : movable) {
            if (!ctx.state.objectExists(id)) continue;
            ctx.executor.moveToBase(id);
            ctx.events.logTrace("STAR SPRING: moved a unit here to its base");
            break;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 771;
        d.def_id = R"RB(unl-215-219)RB";
        d.name = R"RB(Star Spring)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-215/219)RB";
        d.collector_number = 215;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c4fe88ffb5c1b02b58b2c6b4f02de441ac451d6-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_771(CardRegistry& r) {
    r.registerCard(771, std::make_unique<StarSpring>());
}

} // namespace riftbound
