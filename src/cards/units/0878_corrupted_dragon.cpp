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

class CorruptedDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "If your score is not within 3 points of the Victory Score, I enter ready."
    // The Victory Score in effect (GameState::effectiveVictoryScore).
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        const int victory = state.effectiveVictoryScore();
        return victory - state.player(controller).score > 3;
    }
    // "When I attack, you may move any number of enemy units here each with
    // 5 [M] or less to their base." The leading "may" is decided as the
    // ability is finalized (CR 383.3.a), and the units — targets, any number
    // (CR 355.5, 355.13) — are chosen then too (CR 403.2), one at a time with
    // "None". Units that can't be chosen by enemy abilities, or that enemy
    // effects can't move, are left out; Deflect is paid as a unit is chosen
    // (selectSeveralTargets). As it resolves, a chosen unit that is no longer
    // here with 5 [M] or less is a mistarget; the others move together.
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::optional<BattlefieldId> here;
        if (ctx.state.objectExists(ctx.source)) here = ctx.state.getObject(ctx.source).battlefieldId();
        const PlayerId me = ctx.controller;
        const auto eligible = !here.has_value() ? std::vector<GameObjectId>{}
            : chooseableUnits(ctx.state, me, [&](const GameObject& obj) {
                  // Not a unit enemy abilities can't move, and not one that
                  // can't move to base (the executor would leave it here).
                  return obj.controller != me && obj.battlefieldId() == here &&
                         obj.current_might <= 5 && !obj.cant_be_moved_by_enemy &&
                         !moveToBaseBlocked(ctx.state, obj);
              });
        // Offered when a unit could be chosen now (its Deflect payable); as it
        // resolves the units were chosen already, and it goes on while any
        // eligible unit is left (the chosen ones are checked one by one).
        const bool finalizing = ctx.state.chain.resuming.has_value() &&
                                ctx.state.chain.resuming->finalizing;
        const int go = confirmOptional(ctx,
            "Corrupted Dragon: move enemy units here (5 [M] or less) to their base?",
            [&]() {
                return finalizing ? !offerableChoices(ctx, eligible).empty() : !eligible.empty();
            });
        if (go == -1) return;   // waiting for the agent
        std::vector<GameObjectId> moving;
        if (go == 1) {
            const auto picks = selectSeveralTargets(*this, ctx,
                "Corrupted Dragon: move an enemy unit (5 [M] or less) to its base", eligible);
            if (!picks.has_value()) return;   // waiting for the agent
            moving = *picks;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go != 1) return;
        for (auto uid : moving) {
            ctx.executor.moveToBase(uid);
            RIFT_TRACE(ctx.events, "CORRUPTED DRAGON: moved an enemy unit to its base");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 878;
        d.def_id = R"RB(ven-091-166)RB";
        d.name = R"RB(Corrupted Dragon)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-091/166)RB";
        d.collector_number = 91;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 10;
        d.power_cost = 2;
        d.might = 10;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(If your score is not within 3 points of the Victory Score, I enter ready.
When I attack, you may move any number of enemy units here each with 5 [M] or less to their base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6000c549047967e619d47bdbd2f265e5bb8b5b19-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_878(CardRegistry& r) {
    r.registerCard(878, std::make_unique<CorruptedDragon>());
}

}  // namespace riftbound
