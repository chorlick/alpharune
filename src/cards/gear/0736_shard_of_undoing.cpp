#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
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

    // "during your Beginning Phase": the Beginning and Scoring Steps of the
    // controller's own turn (CR 315.2; the Awaken Phase, CR 315.1, is not part
    // of it). Checked as the unit dies.
    // "The first time a friendly unit dies during your Beginning Phase each
    // turn": the turn's first such death, counted for the player whether or
    // not this card was on the board for it (CR 383.1; testing phase B6).
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType, GameObjectId /*subject*/) const override {
        return state.turn.turn_player == controller && isBeginningPhase(state.turn.phase) &&
               state.player(controller).unit_deaths_in_beginning_this_turn == 1;
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& state = ctx.state;
        if (!state.objectExists(ctx.source)) return;
        ChainItem* ri = state.chain.resuming.has_value() ? &*state.chain.resuming : nullptr;
        if (ri && ri->resume_point == kDone) return;

        // "The first time ... each turn": several units can die together (the
        // [Temporary] ones), each putting this ability on the chain; only the
        // first to resolve does anything. Tracked per turn on this gear.
        // "each opponent must kill one of their units." The opponent chooses
        // which (it is their kill, of their own unit — no targeting, so no
        // Deflect and no "can't be chosen").
        const PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> theirs;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isUnit() && obj.controller == opp && obj.location.has_value())
                theirs.push_back(id);
        std::sort(theirs.begin(), theirs.end());
        if (theirs.empty()) { if (ri) ri->resume_point = kDone; return; }

        GameObjectId victim = theirs.front();
        if (ri && theirs.size() > 1) {
            if (ri->resume_point != kAsking) {
                std::vector<Intent> options;
                for (auto id : theirs) options.push_back(objChoice(opp, id));
                ctx.executor.requestChoice(opp, std::move(options),
                                           "Shard of Undoing: kill one of your units");
                ri->resume_point = kAsking;
                return;
            }
            const GameObjectId picked = takenObject(ctx);
            if (std::find(theirs.begin(), theirs.end(), picked) != theirs.end()) victim = picked;
        }
        if (ri) ri->resume_point = kDone;
        ctx.executor.killObject(victim);
        RIFT_TRACE(ctx.events, "SHARD OF UNDOING: opponent kills a unit "
                            "(first friendly death of their Beginning Phase)");
    }
private:
    static constexpr int kAsking = 20;   // clear of the choice helpers' resume points
    static constexpr int kDone = 21;
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
