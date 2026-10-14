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

class FlurryOfFeathers : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose one — Counter a spell. / Play four 1 [M] Bird unit tokens with
    // [Deflect]." The mode (CR 355.3) and, for the first, the spell (a target,
    // CR 355.9.a.2) are chosen as this is played.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto counterable = [&](const ChainItem& ci) { return ci.is_spell; };
        const auto spells = chainItemChoices(ctx, counterable);
        uint32_t legal_mask = 0b10;  // mode 1 (spawn birds) always legal
        if (!spells.empty()) legal_mask |= 0b01;
        int mode = pickMode(ctx, "Flurry of Feathers", /*num_modes=*/2,
                            {"Counter a spell", "Play four Bird tokens"}, legal_mask);
        if (mode == -1) return;   // yielded for agent input
        if (mode == -2) mode = 1; // (birds are always legal)
        std::optional<ChainItemId> spell = ChainItemId{kInvalidId};
        if (mode == 0) {
            spell = pickChainItem(ctx, "Flurry of Feathers: counter a spell", spells);
            if (!spell.has_value()) return;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        if (mode == 0) {
            if (*spell != kInvalidId) counterChosenItem(ctx, *spell);
            return;
        }
        // Mode 1: four 1 [M] Bird tokens with [Deflect], each played to the
        // controller's base or a battlefield they control.
        std::vector<GameObjectId> made;
        auto make = [&](LocationId loc) {
            KeywordSet kw; kw.set(Keyword::Deflect);
            return ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                            /*might=*/1, /*tags=*/{"Bird"}, kw, loc,
                                            /*enter_ready=*/false);
        };
        if (!playTokensChoosingLocations(ctx, "Flurry of Feathers: play a Bird", 4, make, made))
            return;   // waiting for a location
        RIFT_TRACE(ctx.events, "FLURRY OF FEATHERS: spawned 4 Bird tokens");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 606;
        d.def_id = R"RB(unl-044-219)RB";
        d.name = R"RB(Flurry of Feathers)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-044/219)RB";
        d.collector_number = 44;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction]
Choose one —
Counter a spell.Play four 1 [M] Bird unit tokens with [Deflect]. (Opponents must pay [A] to choose them with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/77bd864b29f4975868e557e31a39b94d06a4ecb2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_606(CardRegistry& r) {
    r.registerCard(606, std::make_unique<FlurryOfFeathers>());
}

} // namespace riftbound
