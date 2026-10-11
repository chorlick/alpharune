#include "cards/card.h"
#include "cards/card_helpers.h"
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

// Token created by Zed, From the Shadows / Zed, Without a Sound / Death Mark.
// The gallery has no entry for it; its text comes from those cards' reminder
// text. Creators stamp this card_def_id on the token so its trigger dispatches.
class ShadowClone : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When I attack, you may banish a unit from your trash. If you do, give me
    // [Assault 4] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto trashed_units = [&ctx]() {
            std::vector<GameObjectId> out;
            for (auto cid : ctx.state.player(ctx.controller).trash)
                if (ctx.state.objectExists(cid) && ctx.state.getObject(cid).isUnit())
                    out.push_back(cid);
            return out;
        };
        auto still_legal = [&]() {
            // objectExists is true for every id ever created: I must be on the board.
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value() &&
                   !trashed_units().empty();
        };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Shadow Clone: banish a unit from your trash?",
                                   still_legal);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Shadow Clone: unit to banish", trashed_units());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;   // I left the board
        ctx.executor.banishObject(picked);
        ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Assault, 4);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 955;
        d.def_id = R"RB(ven-token-shadow-clone)RB";
        d.name = R"RB(Shadow Clone)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Token;
        d.might = 0;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn.)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_955(CardRegistry& r) {
    r.registerCard(955, std::make_unique<ShadowClone>());
}

}  // namespace riftbound
