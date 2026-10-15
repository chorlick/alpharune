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

class UltrasoftPoro : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::Activated; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    // "Use this ability only while I'm at a battlefield." — the Poro whose
    // ability it is, not any Ultrasoft Poro its controller has.
    bool canActivateAbilityAt(const GameState& state, PlayerId controller,
                              GameObjectId source, int ability_index) const override {
        if (!Card::canActivateAbilityAt(state, controller, source, ability_index)) return false;
        return state.objectExists(source) && state.getObject(source).isAtBattlefield();
    }
    // "[E]: Play two [1] [M] Bird unit tokens with [Deflect]." No location is
    // printed, so each token goes to its controller's base or a battlefield
    // they control, their choice (CR 355.2.a).
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto make = [&ctx](const LocationId& loc) {
            KeywordSet kw; kw.set(Keyword::Deflect);
            return ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                            /*might=*/1, /*tags=*/{"Bird"}, kw, loc,
                                            /*enter_ready=*/false);
        };
        std::vector<GameObjectId> made;
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        if (!playTokensChoosingLocations(ctx, "Ultrasoft Poro: play a Bird", 2, make, made))
            return;   // waiting for a location
        RIFT_TRACE(ctx.events, "ULTRASOFT PORO: played 2 Bird tokens w/ [Deflect]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 722;
        d.def_id = R"RB(unl-160-219)RB";
        d.name = R"RB(Ultrasoft Poro)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-160/219)RB";
        d.collector_number = 160;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Poro)RB", R"RB(Freljord)RB"};
        d.energy_cost = 5;
        d.might = 5;
        // [Deflect] belongs to the Bird tokens only, not to the Poro.
        d.ability_text = R"RB([E]: Play two [1] [M] Bird unit tokens with [Deflect]. Use this ability only while I'm at a battlefield. (Opponents must pay [A] to choose a [Deflect] unit with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/36900508e2ce61196450a4bf7e27722ae36521d9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_722(CardRegistry& r) {
    r.registerCard(722, std::make_unique<UltrasoftPoro>());
}

} // namespace riftbound
