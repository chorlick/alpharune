#include "cards/card.h"
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

class ShardOfUndoing : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "The first time a friendly unit dies during your Beginning Phase each
    //  turn, each opponent must kill one of their units."
    TriggerType triggerType() const override {
        return TriggerType::WhenAFriendlyUnitDies;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& state = ctx.state;
        if (!state.objectExists(ctx.source)) return;

        // Gate: must be the controller's own Beginning Phase. The "Beginning
        // Phase" spans the Awaken, Beginning, and Scoring steps (CR turn
        // structure); the dying-unit event fires within those steps.
        if (state.turn.turn_player != ctx.controller) return;
        TurnPhase ph = state.turn.phase;
        if (ph != TurnPhase::AwakenPhase && ph != TurnPhase::BeginningStep &&
            ph != TurnPhase::ScoringStep)
            return;

        // Gate: "the first time ... each turn". Track per-turn via a counter
        // on this gear object keyed by the current turn number.
        auto& self = state.getObject(ctx.source);
        const std::string key = "__shard_undoing_turn";
        if (self.card_counters.count(key) &&
            self.card_counters[key] == state.turn.turn_number)
            return;  // already fired this turn
        self.card_counters[key] = state.turn.turn_number;

        // "each opponent must kill one of their units." In 1v1 the opponent
        // selects which of their own units dies; their choice isn't surfaced
        // through this controller's agent, so approximate the rational pick
        // (lowest-Might unit they control) — same convention as King's Edict.
        PlayerId opp = opponent(ctx.controller);
        GameObjectId victim = kInvalidId;
        int best_might = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            if (victim == kInvalidId || obj.current_might < best_might) {
                victim = id;
                best_might = obj.current_might;
            }
        }
        if (victim != kInvalidId) {
            ctx.executor.killObject(victim);
            ctx.events.logTrace("SHARD OF UNDOING: opponent kills a unit "
                                "(first friendly death of their Beginning Phase)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 736;
        d.def_id = R"RB(unl-174-219)RB";
        d.name = R"RB(Shard of Undoing)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-174/219)RB";
        d.collector_number = 174;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(The first time a friendly unit dies during your Beginning Phase each turn, each opponent must kill one of their units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d167f4e37714f0f8983fce16a9c657bc7ce39642-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_736(CardRegistry& r) {
    r.registerCard(736, std::make_unique<ShardOfUndoing>());
}

} // namespace riftbound
