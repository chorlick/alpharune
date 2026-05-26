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

class AbandonedHall : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Resolve this BF's BattlefieldId from its card object.
        BattlefieldId my_bf = kInvalidId;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { my_bf = bf.id; break; }
        }
        if (my_bf == kInvalidId) return;

        auto findTargets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                auto bf = obj.battlefieldId();
                if (!bf || *bf != my_bf) continue;
                out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx,
            "Abandoned Hall: give a unit here +1 [M] this turn?",
            [&]() { return !findTargets().empty(); });
        if (conf == -1) return;  // suspended
        if (conf == 0) return;   // declined
        auto target = pickTarget(ctx, "Abandoned Hall: choose a unit here",
                                 findTargets());
        if (target == kInvalidId) return;
        ctx.executor.giveTemporaryMight(target, 1);
        ctx.events.logTrace("ABANDONED HALL: +1 [M] this turn to " +
                             ctx.state.getObject(target).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 761;
        d.def_id = R"RB(unl-205-219)RB";
        d.name = R"RB(Abandoned Hall)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-205/219)RB";
        d.collector_number = 205;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When a player plays a spell, they may give a unit they control here +1 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7447b04d1e78192509e89e5ff3556368ea5c471a-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_761(CardRegistry& r) {
    r.registerCard(761, std::make_unique<AbandonedHall>());
}

} // namespace riftbound
