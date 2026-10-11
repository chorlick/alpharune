#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AsheFocused : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, choose an opponent. They reveal their hand. Choose a
    //  card revealed this way and banish it. When they hold, return it to
    //  their hand (even if I'm no longer on the board)."
    // The return is a delayed ability keyed to that opponent's next hold
    // (TriggerManager::onScore); the banished card carries a marker, so it is
    // found again whatever has happened to Ashe since.
    static constexpr const char* kReturnMarker = "__ashe_focused_return_on_hold";
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            returnBanished(ctx);
            return;
        }
        PlayerId opp = opponent(ctx.controller);
        auto& opp_ps = ctx.state.player(opp);

        // Reveal their hand — once, not again when re-entered with the pick.
        const auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point == 0) {
            for (auto cid : opp_ps.hand) {
                if (!ctx.state.objectExists(cid)) continue;
                const auto& c = ctx.state.getObject(cid);
                ctx.events.emit(CardRevealedEvent{
                    /*card=*/cid,
                    /*card_def_id=*/c.card_def_id,
                    /*owner=*/c.owner,
                    /*revealed_to_all=*/false,
                    /*revealed_to=*/ctx.controller,
                    /*source_zone=*/ZoneType::Hand,
                });
            }
            RIFT_TRACE(ctx.events, "ASHE FOCUSED: opponent reveals hand (" +
                                std::to_string(opp_ps.hand.size()) + " card(s))");
        }

        // Choose a revealed card and banish it.
        std::vector<GameObjectId> legal(opp_ps.hand.begin(), opp_ps.hand.end());
        if (legal.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Ashe Focused: banish a revealed card",
                                         legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.getObject(picked).zone != ZoneType::Hand) return;   // left the hand
        ctx.executor.banishObject(picked);   // takes it out of the hand
        ctx.state.getObject(picked).card_counters[kReturnMarker] = 1;

        // "When they hold, return it to their hand." Never expires by itself.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;       // my controller's ability (CR 391-392)
        da.watched_player = opp;              // ... that waits for THEIR hold
        da.trigger = TriggerType::WhenIHold;
        da.expires_on_turn = DelayedAbility::kUntilFired;
        ctx.state.delayed_abilities.push_back(da);
        RIFT_TRACE(ctx.events, "ASHE FOCUSED: banished a card from opponent's hand "
                            "until they hold");
    }
    // The delayed half: my controller's ability; the player who held is the
    // opponent it was waiting on.
    void returnBanished(CardContext& ctx) {
        auto& ps = ctx.state.player(opponent(ctx.controller));
        std::vector<GameObjectId> back;
        for (auto cid : ps.banishment) {
            if (!ctx.state.objectExists(cid)) continue;
            auto& c = ctx.state.getObject(cid);
            auto it = c.card_counters.find(kReturnMarker);
            if (it == c.card_counters.end() || it->second == 0) continue;
            it->second = 0;
            back.push_back(cid);
        }
        for (auto cid : back) {
            auto& c = ctx.state.getObject(cid);
            ps.banishment.erase(std::remove(ps.banishment.begin(), ps.banishment.end(), cid),
                                ps.banishment.end());
            c.zone = ZoneType::Hand;
            c.location = std::nullopt;
            ps.hand.push_back(cid);
            RIFT_TRACE(ctx.events, "ASHE FOCUSED: they held -> " + c.name + " returns to hand");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 731;
        d.def_id = R"RB(unl-169-219)RB";
        d.name = R"RB(Ashe, Focused)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-169/219)RB";
        d.collector_number = 169;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Freljord)RB", R"RB(Ashe)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, choose an opponent. They reveal their hand. Choose a card revealed this way and banish it. When they hold, return it to their hand (even if I'm no longer on the board).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ebc033772f2005b7f39ba87ba9ea35f43df0da7a-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_731(CardRegistry& r) {
    r.registerCard(731, std::make_unique<AsheFocused>());
}

} // namespace riftbound
