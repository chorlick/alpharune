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

class SunDisc : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "[E]: [Legion] — The next unit you play this turn enters ready."
    // The activated ability ARMS a one-shot; the WhenYouPlayAUnit trigger
    // readies the unit that was played (its subject) and disarms.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    // Remembered so [Legion] can tell whether Sun Disc itself is one of the
    // cards played this turn.
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).card_counters["__sundisc_played_turn"] =
            ctx.state.turn.turn_number + 1;
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // [Legion] (CR 812.1.c): the effect happens only if a card OTHER than
        // this one was played this turn — so Sun Disc does not count itself
        // on the turn it was played.
        int others = ctx.state.player(ctx.controller).cards_played_this_turn;
        if (auto it = self.card_counters.find("__sundisc_played_turn");
            it != self.card_counters.end() && it->second == ctx.state.turn.turn_number + 1)
            --others;
        if (others < 1) {
            ctx.events.logTrace("SUN DISC: [Legion] not met — no effect");
            return;
        }
        self.card_counters["__sundisc_arm_turn"] = ctx.state.turn.turn_number;
        ctx.events.logTrace("SUN DISC: armed next-unit-enters-ready for this turn");
    }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__sundisc_arm_turn");
        if (it == self.card_counters.end()) return;
        if (it->second != ctx.state.turn.turn_number) return;  // stale arm
        // "The next unit you play": this unit uses the effect up, whether or
        // not it needed readying (it may have entered ready by itself).
        self.card_counters.erase(it);
        const GameObjectId played = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (played == kInvalidId || !ctx.state.objectExists(played)) return;
        const auto& unit = ctx.state.getObject(played);
        if (!unit.isUnit() || !unit.location.has_value() || unit.controller != ctx.controller)
            return;
        ctx.executor.readyObject(played);
        ctx.events.logTrace("SUN DISC: next unit entered ready");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 21;
        d.def_id = R"RB(ogn-021-298)RB";
        d.name = R"RB(Sun Disc)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-021/298)RB";
        d.collector_number = 21;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([E]: [Legion] — The next unit you play this turn enters ready. (Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/146d7514f15e6674f471f3aa9c3fadf22c0b634b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_21(CardRegistry& r) {
    r.registerCard(21, std::make_unique<SunDisc>());
}

} // namespace riftbound
