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

class ReckonerSArena : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, activate the conquer effects of units here."
    // Wired via CardContext::registry (set by the resolution path): on hold, find
    // this BF, then re-dispatch each holder-controlled unit's WhenIConquer
    // onTrigger here. NOTE: this is a direct single-pass dispatch (not a chained
    // re-fire), so resumable conquer effects (confirmOptional/pickTarget) are
    // approximated; simple conquer effects re-run faithfully.
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.registry) return;
        // Which battlefield does this BF-card object represent?
        std::optional<BattlefieldId> my_bf;
        for (const auto& b : ctx.state.battlefields)
            if (b.card_object_id == ctx.source) { my_bf = b.id; break; }
        if (!my_bf) return;
        // Snapshot holder-controlled units here that have a conquer effect.
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            auto ubf = obj.battlefieldId();
            if (!ubf || *ubf != *my_bf) continue;
            Card* uc = ctx.registry->get(obj.card_def_id);
            if (uc && (uc->firesOn(TriggerType::WhenIConquer) ||
                       uc->firesOn(TriggerType::WhenIConquerOrHold)))
                units.push_back(id);
        }
        for (auto uid : units) {
            if (!ctx.state.objectExists(uid)) continue;
            Card* uc = ctx.registry->get(ctx.state.getObject(uid).card_def_id);
            if (!uc) continue;
            // Dispatch with whichever conquer trigger the unit actually declares.
            TriggerType ft = uc->firesOn(TriggerType::WhenIConquer)
                ? TriggerType::WhenIConquer : TriggerType::WhenIConquerOrHold;
            CardContext sub{ctx.state, ctx.events, ctx.executor,
                            ctx.state.getObject(uid).controller, uid};
            sub.firing_trigger = ft;
            sub.registry = ctx.registry;
            uc->onTrigger(sub, {});
        }
        ctx.events.logTrace("RECKONER'S ARENA: re-activated conquer effects of units here");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 281;
        d.def_id = R"RB(ogn-286-298)RB";
        d.name = R"RB(Reckoner's Arena)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-286/298)RB";
        d.collector_number = 286;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, activate the conquer effects of units here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6b7d867487efcbefa8c3d67043a839497ec50388-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_281(CardRegistry& r) {
    r.registerCard(281, std::make_unique<ReckonerSArena>());
}

} // namespace riftbound
