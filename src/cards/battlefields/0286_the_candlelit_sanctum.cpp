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

class TheCandlelitSanctum : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you conquer here, look at the top two cards of your Main Deck. You
    //  may recycle one or both of them. Put those you don't back in any order."
    // predict() = look at top N, agent chooses which to recycle (to bottom) and
    // which to keep on top in chosen order — exactly this scry/recycle.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.predict(ctx.controller, 2);
        ctx.events.logTrace("THE CANDLELIT SANCTUM: conquer -> look at top 2, recycle/keep");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 286;
        d.def_id = R"RB(ogn-291-298)RB";
        d.name = R"RB(The Candlelit Sanctum)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-291/298)RB";
        d.collector_number = 291;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, look at the top two cards of your Main Deck. You may recycle one or both of them. Put those you don't back in any order.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f14fe78f2b7f3909eadb07bce24bd582e190653d-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_286(CardRegistry& r) {
    r.registerCard(286, std::make_unique<TheCandlelitSanctum>());
}

} // namespace riftbound
