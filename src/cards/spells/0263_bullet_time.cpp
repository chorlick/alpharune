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

class BulletTime : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "... to all enemy units at a battlefield": a battlefield is chosen, not
    // a unit there — no Deflect is owed, nothing is "chosen", and a
    // battlefield may be named whatever stands on it. It is chosen as the
    // spell is played (CR 355); X is asked as it resolves. Nothing is paid or
    // dealt before both are known: the callback is re-entered from the top
    // with each answer.
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i)
            labels.push_back(ctx.state.objectExists(bfs[i].card_object_id)
                ? ctx.state.getObject(bfs[i].card_object_id).name
                : "Battlefield #" + std::to_string(static_cast<int>(bfs[i].id)));
        const int mode = pickMode(ctx, "Bullet Time: battlefield", n, labels);
        if (mode < 0 || mode >= n) return;   // -1 waiting, -2 no battlefield
        if (finalizingChoices(ctx)) return;  // choices end here (CR 355)
        const BattlefieldId bf_id = bfs[mode].id;

        // "Pay any amount of [A]": up to all the Power the controller can
        // pay — floating Power plus every rune in base, ready or exhausted
        // (CR 164.2.b). pickXAmount publishes one int-coded MakeChoice per X
        // and the action vocabulary has 16 int slots (0..15), so X is capped
        // at 15.
        const int max_x = std::min(ctx.executor.payableAnyPower(ctx.controller), 15);
        int x = pickXAmount(ctx, "Bullet Time: X power", 0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            RIFT_TRACE(ctx.events, "BULLET TIME: X=0, no damage");
            return;
        }

        if (!ctx.executor.payAnyPower(ctx.controller, x)) {
            RIFT_TRACE(ctx.events, "BULLET TIME: X can no longer be paid, no damage");
            return;
        }
        RIFT_TRACE(ctx.events, "BULLET TIME: paid " + std::to_string(x) +
                             " power → dealing " + std::to_string(x) +
                             " damage to all enemies at BF#" +
                             std::to_string(bf_id));

        // Deal X damage to all enemy units at the chosen battlefield.
        // Snapshot first so kills don't invalidate iteration.
        std::vector<GameObjectId> victims;
        PlayerId enemy = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit()) continue;
            if (obj.controller != enemy) continue;
            if (!obj.isAtBattlefield()) continue;
            auto vbf = obj.battlefieldId();
            if (vbf && *vbf == bf_id) victims.push_back(id);
        }
        for (auto v : victims) ctx.executor.dealDamage(v, x, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 263;
        d.def_id = R"RB(ogn-268-298)RB";
        d.name = R"RB(Bullet Time)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-268/298)RB";
        d.collector_number = 268;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Chaos};
        d.tags = {R"RB(Miss Fortune)RB"};
        d.energy_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Pay any amount of [A] to deal that much damage to all enemy units at a battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/96ed7f6b121a7f46003534393350838efe2776f0-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_263(CardRegistry& r) {
    r.registerCard(263, std::make_unique<BulletTime>());
}

} // namespace riftbound
