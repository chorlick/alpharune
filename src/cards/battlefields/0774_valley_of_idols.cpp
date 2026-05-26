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

class ValleyOfIdols : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When a player plays a unit here, they may pay [1] to [Buff] it."
    // The battlefield WhenYouPlayAUnit trigger fires (per TriggerManager) only
    // on the BF where the unit landed, with ctx.controller = the playing
    // player — so this covers BOTH players' plays here.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;

        // Find the just-played unit: newest non-token unit of ctx.controller
        // sitting here. (Trigger carries no played-object id.)
        GameObjectId played = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.isToken()) continue;
            auto bf = obj.battlefieldId();
            if (!bf || *bf != here) continue;
            if (played == kInvalidId || id > played) played = id;  // newest id heuristic
        }
        if (played == kInvalidId) return;

        // Find a ready rune to pay [1].
        auto base_loc = BaseLocation{ctx.controller};
        auto find_rune = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isRune() || obj.controller != ctx.controller || obj.is_exhausted) continue;
                if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                return id;
            }
            return kInvalidId;
        };

        int decision = confirmOptional(ctx, "Valley of Idols: pay [1] to [Buff] the unit?",
            [&]() {
                return ctx.state.objectExists(played) && find_rune() != kInvalidId;
            });
        if (decision == -1) return;  // waiting on agent
        if (decision == 0) return;   // declined / not legal

        GameObjectId pay_rune = find_rune();
        if (!ctx.state.objectExists(played) || pay_rune == kInvalidId) return;
        ctx.state.getObject(pay_rune).is_exhausted = true;
        // [Buff] = +1 [M] buff if it doesn't already have one.
        if (ctx.state.getObject(played).buff_count == 0) {
            ctx.executor.buffUnit(played);
            ctx.events.logTrace("VALLEY OF IDOLS: paid [1] -> [Buff] the played unit");
        } else {
            ctx.events.logTrace("VALLEY OF IDOLS: paid [1] but unit already has a buff");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 774;
        d.def_id = R"RB(unl-218-219)RB";
        d.name = R"RB(Valley of Idols)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-218/219)RB";
        d.collector_number = 218;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a player plays a unit here, they may pay [1] to [Buff] it. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4191aa2fda9e754a7f5421edc94bd829f5795650-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_774(CardRegistry& r) {
    r.registerCard(774, std::make_unique<ValleyOfIdols>());
}

} // namespace riftbound
