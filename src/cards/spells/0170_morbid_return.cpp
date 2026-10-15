#include "cards/card.h"
#include "cards/card_helpers.h"
#include "engine/deck_ops.h"
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

class MorbidReturn : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Return a unit from your trash to your hand." The trash is a public
    // zone, so the unit is a target (CR 355.10.a, the rule's own example),
    // chosen as the spell is played (CR 355.5) — in its finalize pass, before
    // its cost. It is not playable with no unit there (CR 355.8).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState&, PlayerId,
                                           GameObjectId) const override {
        return FinalizeChoice::No;   // a card in a trash, not a unit on the board
    }
    static std::vector<GameObjectId> trashUnits(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto cid : state.player(controller).trash)
            if (state.objectExists(cid) && state.getObject(cid).isUnit()) out.push_back(cid);
        return out;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !trashUnits(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // No unit: pickTarget stores "no target", so nothing is asked later.
        const GameObjectId picked = pickTarget(ctx, "Morbid Return (unit from trash)",
                                               trashUnits(ctx.state, ctx.controller));
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7)
            return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)
        // A unit no longer in the trash is a mistarget (CR 359.3.e.5).
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it == ps.trash.end()) return;
        ps.trash.erase(it);
        auto& obj = ctx.state.getObject(picked);
        deck::putInHand(ctx.state, picked);
        RIFT_TRACE(ctx.events, "MORBID RETURN: returned " + obj.name +
                             " from trash to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 170;
        d.def_id = R"RB(ogn-170-298)RB";
        d.name = R"RB(Morbid Return)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-170/298)RB";
        d.collector_number = 170;
        d.artist = R"RB(Rafael Zanchetin)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Return a unit from your trash to your hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/33b5ca4e580b94f2056bd884a4a35e8c630920a2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_170(CardRegistry& r) {
    r.registerCard(170, std::make_unique<MorbidReturn>());
}

} // namespace riftbound
