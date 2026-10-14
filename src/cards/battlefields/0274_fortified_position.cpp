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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When you defend here, choose a unit. It gains [Shield 2] this combat."
    // "A unit": any unit on the board, either player's (not only a defender).
    // "This combat", not "this turn": a continuous effect for the combat going
    // on here (EffectExecutor::giveKeywordThisCombat) that lasts until it ends
    // even if this battlefield card is replaced meanwhile (testing phase H1).
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto units = chooseableUnits(ctx.state, ctx.controller,
                                           [](const GameObject&) { return true; });
        GameObjectId picked = pickTarget(ctx, "Fortified Position (gain Shield 2)", units);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;     // none / mistarget / waiting
        // "This combat": the one at this battlefield. A token that replaced
        // this card since is the same object for what refers to it (CR
        // 439.1.a), so "here" is the slot that holds either.
        auto here = battlefieldOfCard(ctx.state, ctx.source);
        if (!here.has_value())
            for (const auto& bf : ctx.state.battlefields)
                if (bf.replaced_card == std::optional<GameObjectId>{ctx.source}) here = bf.id;
        if (!here.has_value()) return;
        if (!ctx.state.objectExists(picked) || !ctx.state.getObject(picked).location.has_value())
            return;   // it has left
        ctx.executor.giveKeywordThisCombat(picked, Keyword::Shield, 2, *here);
        RIFT_TRACE(ctx.events, "FORTIFIED POSITION: defend -> " +
                            ctx.state.getObject(picked).name + " gains [Shield 2] this combat");
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
