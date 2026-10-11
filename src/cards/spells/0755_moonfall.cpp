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

class Moonfall : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Choose a battlefield where you have units. You may move up to one enemy
    //  unit to that battlefield. Then give enemy units there -2 [M] this turn."
    // The battlefield, whether to move a unit, and which one are chosen as the
    // spell is played (CR 355).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    // It chooses no unit of its controller's (the play gate, CR 355 -> 356).
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        (void)state; (void)controller; (void)object;
        return FinalizeChoice::No;
    }

    /// Battlefields where `p` has a unit, as a pickMode mask over the slots.
    static uint32_t battlefieldMask(const GameState& state, PlayerId p) {
        uint32_t mask = 0;
        for (size_t i = 0; i < state.battlefields.size() && i < 32; ++i)
            for (const auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != p) continue;
                const auto at = obj.battlefieldId();
                if (at.has_value() && *at == state.battlefields[i].id) { mask |= (1u << i); break; }
            }
        return mask;
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return battlefieldMask(state, controller) != 0;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& bfs = ctx.state.battlefields;
        const int n = static_cast<int>(std::min<size_t>(bfs.size(), 32));
        std::vector<std::string> labels;
        for (int i = 0; i < n; ++i)
            labels.push_back(ctx.state.objectExists(bfs[i].card_object_id)
                ? ctx.state.getObject(bfs[i].card_object_id).name
                : "Battlefield #" + std::to_string(static_cast<int>(bfs[i].id)));
        const int mode = pickMode(ctx, "Moonfall: battlefield", n, labels,
                                  battlefieldMask(ctx.state, ctx.controller));
        if (mode < 0 || mode >= n) return;   // -1 waiting, -2 no battlefield with your units
        const BattlefieldId bf = bfs[mode].id;

        // "You may move up to one enemy unit to that battlefield."
        auto movable = [&]() {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                if (o.controller == ctx.controller) return false;
                const auto at = o.battlefieldId();
                return !(at.has_value() && *at == bf);   // not already there
            });
        };
        // "No" is zero units (CR 355.13); the -2 [M] below does not depend on it.
        const int conf = confirmOptional(ctx,
            "Moonfall: move an enemy unit to that battlefield?",
            [&]() { return !movable().empty(); });
        if (conf == -1) return;  // waiting for agent
        GameObjectId mover = kInvalidId;
        if (conf == 1) {
            mover = pickTarget(ctx, "Moonfall: enemy unit to move", movable());
            if (mover == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) return;   // waiting
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 355)

        if (mover != kInvalidId && ctx.state.objectExists(mover)) {
            ctx.executor.moveToBattlefield(mover, bf);
            RIFT_TRACE(ctx.events, "MOONFALL: moved an enemy unit to the battlefield");
        }
        // "Then give enemy units there -2 [M] this turn."
        std::vector<GameObjectId> there;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            const auto at = obj.battlefieldId();
            if (at.has_value() && *at == bf) there.push_back(id);
        }
        for (auto id : there) ctx.executor.giveTemporaryMight(id, -2);
        RIFT_TRACE(ctx.events, "MOONFALL: -2 [M] to enemy units at the battlefield");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 755;
        d.def_id = R"RB(unl-198-219)RB";
        d.name = R"RB(Moonfall)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-198/219)RB";
        d.collector_number = 198;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Diana)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Choose a battlefield where you have units. You may move up to one enemy unit to that battlefield. Then give enemy units there -2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2eda8d4bbee47857b1916599b5c7d5fa7ca400dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_755(CardRegistry& r) {
    r.registerCard(755, std::make_unique<Moonfall>());
}

} // namespace riftbound
