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

class Shakedown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true};
    }
    // "Deal 6 to it unless its controller has you draw 2." — the unit's
    // controller decides as the spell resolves.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        const PlayerId unit_controller = ctx.state.getObject(targets[0]).controller;
        bool caster_draws = false;
        if (ctx.state.chain.resuming.has_value()) {
            auto& ri = *ctx.state.chain.resuming;
            if (ri.resume_point == kDone) return;
            if (ri.resume_point != kAwait) {
                // At resolution: another player's choice (CR 355.10.e; batch M1).
                ctx.executor.requestChoice(unit_controller,
                    {valChoice(unit_controller, 0, "Take 6 damage"),
                     valChoice(unit_controller, 1, "Opponent draws 2")},
                    "Shakedown: 6 damage to " + ctx.state.getObject(targets[0]).name +
                    " unless you have the opponent draw 2");
                ri.resume_point = kAwait;
                return;
            }
            ri.resume_point = kDone;
            caster_draws = takenValue(ctx, 0) == 1;
        }
        if (caster_draws) {
            ctx.executor.drawCards(ctx.controller, 2);
            RIFT_TRACE(ctx.events, "SHAKEDOWN: the unit's controller has the caster draw 2");
        } else {
            ctx.executor.dealDamage(targets[0], 6, ctx.source);
            RIFT_TRACE(ctx.events, "SHAKEDOWN: deal 6 to chosen enemy unit");
        }
    }
private:
    enum : int { kAwait = 21, kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 33;
        d.def_id = R"RB(ogn-033-298)RB";
        d.name = R"RB(Shakedown)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-033/298)RB";
        d.collector_number = 33;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose an enemy unit. Deal 6 to it unless its controller has you draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab71d92c94bf609e2fa6efc8fec06fe1e8b10108-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_33(CardRegistry& r) {
    r.registerCard(33, std::make_unique<Shakedown>());
}

} // namespace riftbound
