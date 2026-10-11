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

class StarSpring : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "The first time a player plays a non-token unit here each turn, they may
    //  move another unit they control here to its base."
    // The played unit is the trigger's subject. "First time each turn" is per
    // player: the battlefield card remembers the turn and the unit that used
    // it up.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    static std::string turnKey(PlayerId p) { return "__starspring_turn_p" + std::to_string(playerIndex(p)); }
    static std::string unitKey(PlayerId p) { return "__starspring_unit_p" + std::to_string(playerIndex(p)); }
    // True if `player` has not yet played a non-token unit here this turn, or
    // `played` is the unit that was first.
    static bool isFirstHereThisTurn(const GameState& state, GameObjectId self_id,
                                    PlayerId player, GameObjectId played) {
        const auto& counters = state.getObject(self_id).card_counters;
        const auto turn = counters.find(turnKey(player));
        if (turn == counters.end() || turn->second != state.turn.turn_number + 1) return true;
        const auto unit = counters.find(unitKey(player));
        return unit != counters.end() && unit->second == static_cast<int>(played.raw());
    }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId controller,
                          TriggerType /*trigger*/, GameObjectId subject) const override {
        if (subject == kInvalidId || !state.objectExists(subject)) return false;
        if (state.getObject(subject).isToken()) return false;   // "a non-token unit"
        return isFirstHereThisTurn(state, source, controller, subject);
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const auto here = battlefieldOfCard(ctx.state, ctx.source);
        const GameObjectId played = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (!here.has_value() || played == kInvalidId) return;
        if (!isFirstHereThisTurn(ctx.state, ctx.source, ctx.controller, played)) return;
        // This play uses up the player's "first time" whatever they answer.
        // Written before the end-of-choices marker on purpose: it records the
        // event, not the effect, and writes the same two values on every entry.
        {
            auto& self = ctx.state.getObject(ctx.source);
            self.card_counters[turnKey(ctx.controller)] = ctx.state.turn.turn_number + 1;
            self.card_counters[unitKey(ctx.controller)] = static_cast<int>(played.raw());
        }
        // "another unit they control here", able to move to its base.
        auto movable = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (id == played || !obj.isUnit() || obj.controller != ctx.controller) continue;
                if (obj.battlefieldId() != here || moveToBaseBlocked(ctx.state, obj)) continue;
                out.push_back(id);
            }
            return out;
        };
        const int go = confirmOptional(ctx, "Star Spring: move another unit here to its base?",
                                       [&]() { return !movable().empty(); });
        GameObjectId t = kInvalidId;
        if (go == 1) t = pickTarget(ctx, "Star Spring: move which unit to its base", movable());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (go < 1 || t == kInvalidId) return;   // declined / none / mistarget / waiting
        ctx.executor.moveToBase(t);
        RIFT_TRACE(ctx.events, "STAR SPRING: moved a unit here to its base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 771;
        d.def_id = R"RB(unl-215-219)RB";
        d.name = R"RB(Star Spring)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-215/219)RB";
        d.collector_number = 215;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0c4fe88ffb5c1b02b58b2c6b4f02de441ac451d6-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_771(CardRegistry& r) {
    r.registerCard(771, std::make_unique<StarSpring>());
}

} // namespace riftbound
