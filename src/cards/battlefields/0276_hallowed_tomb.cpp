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

class HallowedTomb : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you hold here, you may return your Chosen Champion from your trash
    //  to your Champion Zone if it is empty."
    // (Previously implemented as bounce-friendly-unit-to-hand — wrong effect.)
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.champion_zone != kInvalidId &&
            ctx.state.objectExists(ps.champion_zone)) {
            return;  // Champion Zone not empty
        }
        // Find a champion unit in the player's trash.
        GameObjectId champ = kInvalidId;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            const auto& obj = ctx.state.getObject(cid);
            if (obj.isUnit() && obj.isChampion()) { champ = cid; break; }
        }
        if (champ == kInvalidId) return;
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), champ),
                       ps.trash.end());
        auto& obj = ctx.state.getObject(champ);
        obj.zone = ZoneType::ChampionZone;
        obj.location = std::nullopt;
        ps.champion_zone = champ;
        ctx.events.logTrace("HALLOWED TOMB: return Chosen Champion from trash to "
                            "empty Champion Zone");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 276;
        d.def_id = R"RB(ogn-281-298)RB";
        d.name = R"RB(Hallowed Tomb)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-281/298)RB";
        d.collector_number = 281;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you hold here, you may return your Chosen Champion from your trash to your Champion Zone if it is empty.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2f29ea42fb3e1c8ce05d2e82f2a2fe0d45851953-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_276(CardRegistry& r) {
    r.registerCard(276, std::make_unique<HallowedTomb>());
}

} // namespace riftbound
