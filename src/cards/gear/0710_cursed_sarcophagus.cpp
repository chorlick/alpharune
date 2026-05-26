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

// "When you play this, banish all units from your trash.
//  [E]: Play a unit banished with this. (You must pay its costs.)"
// Units banished by this are recorded in the gear's tracked_objects so the
// activated ability knows which banished units it may re-play. Power/Energy
// cost payment for the replayed unit is approximated as free (playIgnoringCost)
// — no structured "play from banishment while paying full cost" hook exists.

class CursedSarcophagus : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Banish all units from YOUR TRASH (not the board).
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> trash_units;
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) trash_units.push_back(cid);
        }
        for (auto cid : trash_units) {
            ctx.executor.banishObject(cid);
            // banishObject pushes to player.banishment and zones it.
            if (ctx.state.objectExists(ctx.source))
                ctx.state.getObject(ctx.source).tracked_objects.push_back(cid);
        }
        if (!trash_units.empty())
            ctx.events.logTrace("CURSED SARCOPHAGUS: banished " +
                                 std::to_string(trash_units.size()) +
                                 " unit(s) from trash");
    }

    // [E]: Play a unit banished with this.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    bool canActivateAbility(const GameState& state, PlayerId controller) const override {
        // Find this gear instance and check it has any tracked banished unit
        // still in banishment.
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            for (auto bid : obj.tracked_objects) {
                if (state.objectExists(bid) &&
                    state.getObject(bid).zone == ZoneType::Banishment) {
                    return true;
                }
            }
        }
        return false;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        std::vector<GameObjectId> legal;
        for (auto bid : self.tracked_objects) {
            if (ctx.state.objectExists(bid) &&
                ctx.state.getObject(bid).zone == ZoneType::Banishment) {
                legal.push_back(bid);
            }
        }
        if (legal.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Cursed Sarcophagus (banished unit)", legal);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // Remove from owner's banishment zone before re-playing.
        auto& bz = ctx.state.player(ctx.state.getObject(picked).owner).banishment;
        bz.erase(std::remove(bz.begin(), bz.end(), picked), bz.end());
        auto& tr = self.tracked_objects;
        tr.erase(std::remove(tr.begin(), tr.end(), picked), tr.end());
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("CURSED SARCOPHAGUS: played a banished unit");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 710;
        d.def_id = R"RB(unl-148-219)RB";
        d.name = R"RB(Cursed Sarcophagus)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-148/219)RB";
        d.collector_number = 148;
        d.artist = R"RB(Polar Engine Studio/华锐)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play this, banish all units from your trash.
[E]: Play a unit banished with this. (You must pay its costs.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ae3823d4109d9bbae8c7983ec1c1082ba5b4f190-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_710(CardRegistry& r) {
    r.registerCard(710, std::make_unique<CursedSarcophagus>());
}

} // namespace riftbound
