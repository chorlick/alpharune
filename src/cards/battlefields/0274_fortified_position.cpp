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

class FortifiedPosition : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you defend here, choose a unit. It gains [Shield 2] this combat."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Choose a friendly unit (the defender) controlled by the defending
        // player (ctx.controller).
        std::vector<GameObjectId> friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                friendly.push_back(id);
        }
        if (friendly.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Fortified Position (gain Shield 2)", friendly);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryKeyword(picked, Keyword::Shield, 2);
        ctx.events.logTrace("FORTIFIED POSITION: defend -> grant Shield 2");
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouDefendHere; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 274;
        d.def_id = R"RB(ogn-279-298)RB";
        d.name = R"RB(Fortified Position)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-279/298)RB";
        d.collector_number = 279;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.shield_value = 2;
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB(When you defend here, choose a unit. It gains [Shield 2] this combat. (+2 [M] while it's a defender.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/45363bbd907f4f3717868cb04b3cfed814b3bb32-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_274(CardRegistry& r) {
    r.registerCard(274, std::make_unique<FortifiedPosition>());
}

} // namespace riftbound
