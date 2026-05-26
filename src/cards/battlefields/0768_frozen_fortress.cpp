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

class FrozenFortress : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "At the start of each player's Beginning Phase, deal 1 to EACH unit here."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Find "here" — the battlefield whose card object is this card.
        BattlefieldId here = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { here = bf.id; break; }
        }
        if (here == kInvalidId) return;
        // Collect ALL units here (both players) before dealing/killing (AoE
        // collect-then-kill to avoid iterator invalidation).
        std::vector<GameObjectId> units_here;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            auto bf = obj.battlefieldId();
            if (bf && *bf == here) units_here.push_back(id);
        }
        for (auto id : units_here) {
            if (ctx.state.objectExists(id))
                ctx.executor.dealDamage(id, 1, ctx.source);
        }
        for (auto id : units_here) {
            if (ctx.state.objectExists(id) &&
                ctx.state.getObject(id).hasLethalDamage()) {
                ctx.executor.killObject(id);
            }
        }
        ctx.events.logTrace("FROZEN FORTRESS: dealt 1 to each unit here");
    }
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 768;
        d.def_id = R"RB(unl-212-219)RB";
        d.name = R"RB(Frozen Fortress)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-212/219)RB";
        d.collector_number = 212;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(At the start of each player's Beginning Phase, deal 1 to each unit here. (This happens before scoring.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b5e33fdb4ddc8c73e03bca648862fc080aa8bbcf-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_768(CardRegistry& r) {
    r.registerCard(768, std::make_unique<FrozenFortress>());
}

} // namespace riftbound
