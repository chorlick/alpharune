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

// "You may play a unit from hand to a battlefield you control, reducing its
//  cost by [3]." ([Hidden] / [Action] are engine-handled timing.)
// The reduced cost is paid through the engine's cost path
// (EffectExecutor::playPayingCost).

class HereToHelp : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const EffectExecutor::EffectPlayCost reduced{.energy_reduction = 3};
        auto playable = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ctx.state.player(ctx.controller).hand) {
                if (!ctx.state.objectExists(cid)) continue;
                if (!ctx.state.getObject(cid).isUnit()) continue;
                if (ctx.executor.canPlayPayingCost(ctx.controller, cid, reduced))
                    out.push_back(cid);
            }
            return out;
        };
        // "to a battlefield you control": never the base. Played from
        // facedown, that battlefield (CR 811.1.d.5).
        const auto where = playLocations(ctx.state, ctx.controller,
                                         facedownPlayBattlefield(ctx.state, ctx.source),
                                         /*include_base=*/false);
        // All three choices come before the play: this callback is re-entered
        // from the top after each answer.
        // At resolution: a spell's "may" (CR 355.12, 355.18; batch M1).
        int go = confirmOptional(ctx, "Here to Help: play a unit from hand",
            [&]() { return where.count() > 0 && !playable().empty(); });
        if (go <= 0) return;                 // waiting, declined, or nothing to play
        // At resolution: a card in a hidden zone is not a target (CR 355.10.a; batch M1).
        GameObjectId unit = pickUnseenCard(ctx, "Here to Help: unit to play", playable());
        if (unit == kInvalidId) return;      // waiting
        // At resolution: an effect play's location is chosen as it plays (CR 420.3.b.2; batch M1).
        int mode = pickMode(ctx, "Here to Help: battlefield", where.count(), where.labels);
        if (mode < 0 || mode >= where.count()) return;
        if (ctx.executor.playPayingCost(ctx.controller, unit, where.locations[mode], reduced))
            RIFT_TRACE(ctx.events, "HERE TO HELP: played a unit from hand to a controlled "
                                "battlefield, its cost reduced by [3]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 433;
        d.def_id = R"RB(sfd-111-221)RB";
        d.name = R"RB(Here to Help)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-111/221)RB";
        d.collector_number = 111;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Action] (Play on your turn or in showdowns.)
You may play a unit from hand to a battlefield you control, reducing its cost by [3].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6daa8caf27f76f2b0bce189fb9df196129582afa-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_433(CardRegistry& r) {
    r.registerCard(433, std::make_unique<HereToHelp>());
}

} // namespace riftbound
