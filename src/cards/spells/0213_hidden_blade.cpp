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

class HiddenBlade : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q proof-of-concept (any-side single unit target — Hidden
    // Blade is "Kill a unit at a battlefield" without friendly/enemy
    // restriction, so legal targets include BOTH sides). Distinct
    // policy-head slots per (target card type) become especially
    // valuable here because the model can learn "prefer killing enemy
    // X over friendly Y" through the MakeChoice head.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Hidden Blade", legal);
        if (picked == kInvalidId) return;
        // CR 463: re-validate the target requirements at resolve time.
        // pickTarget already filtered to currently-legal targets, but
        // the resume re-entry path could lag a turn — defensive re-check.
        auto& obj = ctx.state.getObject(picked);
        if (!obj.isUnit() || !obj.isAtBattlefield()) {
            ctx.events.logTrace("HIDDEN BLADE: target no longer at a "
                                 "battlefield — fizzle (CR 463)");
            return;
        }
        PlayerId target_controller = obj.controller;
        ctx.executor.killObject(picked);
        if (target_controller != PlayerId::None) {
            ctx.events.logTrace("HIDDEN BLADE: its controller (" +
                                 std::string(toString(target_controller)) +
                                 ") draws 2");
            ctx.executor.drawCards(target_controller, 2);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 213;
        d.def_id = R"RB(ogn-213-298)RB";
        d.name = R"RB(Hidden Blade)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-213/298)RB";
        d.collector_number = 213;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
Kill a unit at a battlefield. Its controller draws 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0437ab8a0b67f43ef5483a103bbae9e57fd05822-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_213(CardRegistry& r) {
    r.registerCard(213, std::make_unique<HiddenBlade>());
}

} // namespace riftbound
