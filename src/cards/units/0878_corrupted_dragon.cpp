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
    // The Victory Score in effect: the mode's score plus what battlefields in
    // play add to it (TurnState::victory_score_bonus, set every aura pass).
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        const int victory = state.mode.victory_score + state.turn.victory_score_bonus;
        return victory - state.player(controller).score > 3;
    }
    // "When I attack, you may move any number of enemy units here each with
    // 5 [M] or less to their base." Offered one unit at a time with a trailing
    // "done" choice (empty chosen_objects) until the agent stops or none remain.
    // Each unit moved is a unit chosen: units that can't be chosen by enemy
    // abilities are left out, Deflect is paid (Card::offerableChoices /
    // commitChosenObject).
    // ENGINE-GAP: the resolve loop allows 16 re-entries, so at most kMaxMoves
    // units can be moved by one attack.
    static constexpr int kMaxMoves = 12;
    static constexpr size_t kMovedSlot = 7;   // resume_data: units moved so far
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.chain.resuming.has_value()) return;    // needs the resume loop
        auto& ri = *ctx.state.chain.resuming;
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf) return;
        while (ri.resume_data.size() <= kMovedSlot) ri.resume_data.push_back(0);
        auto eligible = [&]() {
            const PlayerId me = ctx.controller;
            const BattlefieldId here = *bf;
            auto out = chooseableUnits(ctx.state, me, [me, here](const GameObject& obj) {
                auto obf = obj.battlefieldId();
                return obj.controller != me && obf && *obf == here && obj.current_might <= 5;
            });
            std::sort(out.begin(), out.end());
            return offerableChoices(ctx, out);
        };
        auto offer = [&]() {
            if (ri.resume_data[kMovedSlot] >= kMaxMoves) return false;
            auto units = eligible();
            if (units.empty()) return false;
            std::vector<Intent> out;
            for (auto uid : units) out.push_back(objChoice(ctx.controller, uid));
            Intent done;
            done.type = IntentType::MakeChoice;
            done.player = ctx.controller;
            out.push_back(std::move(done));
            ctx.executor.requestChoice(ctx.controller, std::move(out),
                "Corrupted Dragon: move an enemy unit (5 [M] or less) to its base, or stop");
            return true;
        };
        switch (ri.resume_point) {
        case 0:
            ri.resume_point = offer() ? 1 : 2;
            return;
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) { ri.resume_point = 2; return; }
            auto uid = choice->chosen_objects[0];
            auto legal = eligible();
            if (std::find(legal.begin(), legal.end(), uid) != legal.end()) {
                commitChosenObject(ctx, uid);
                ctx.executor.moveToBase(uid);
                ++ri.resume_data[kMovedSlot];
                ctx.events.logTrace("CORRUPTED DRAGON: moved an enemy unit to its base");
            }
            if (!offer()) ri.resume_point = 2;
            return;
        }
        default:
            return;
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
