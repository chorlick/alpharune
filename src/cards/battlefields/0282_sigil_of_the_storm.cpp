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

class SigilOfTheStorm : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you conquer here, you must recycle one of your runes.
    //  (This doesn't choose anything.)" — mandatory, no choice. Recycle one
    //  rune the player controls.
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        GameObjectId rune = kInvalidId;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isRune() && obj.controller == ctx.controller && obj.location.has_value()) {
                rune = id;
                break;
            }
        }
        if (rune == kInvalidId) return;
        ctx.executor.recycleCards(ctx.controller, {rune});
        ctx.events.logTrace("SIGIL OF THE STORM: conquer -> recycle one rune");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 282;
        d.def_id = R"RB(ogn-287-298)RB";
        d.name = R"RB(Sigil of the Storm)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-287/298)RB";
        d.collector_number = 287;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you must recycle one of your runes. (This doesn't choose anything.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9b795e7a2af421aabc01dc6f35c0b5d547fe3c0e-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_282(CardRegistry& r) {
    r.registerCard(282, std::make_unique<SigilOfTheStorm>());
}

} // namespace riftbound
