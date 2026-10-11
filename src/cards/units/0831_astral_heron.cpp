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

class AstralHeron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play your first card each turn, if I'm at a battlefield, your
    //  next card costs [2][A][A] less."
    // "Your first card each turn": a permanent's play is announced as it is
    // played, a spell's only once it has resolved (CR 420.4.a) — by which
    // time cards played in response to it have been announced first, and
    // PlayerState::cards_played_this_turn has moved past 1. So the Heron
    // counts the plays it has been told about this turn: the first card is
    // the one announced when every other card played this turn has already
    // been announced. (A response that was countered is never announced; the
    // spell under it is then not recognised as the first card.)
    // The [A][A] half is CostModifier::power_reduction, which the engine's
    // cost path subtracts from the card's Power cost (clamped at 0).
    static constexpr const char* kTurnKey = "__heron_turn";          // turn + 1
    static constexpr const char* kSeenKey = "__heron_plays_seen";    // this turn
    static constexpr const char* kFirstKey = "__heron_first_seen";   // this turn
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayACard; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto& ps = ctx.state.player(ctx.controller);
        const int turn_id = ctx.state.turn.turn_number + 1;
        if (self.card_counters[kTurnKey] != turn_id) {
            self.card_counters[kTurnKey] = turn_id;
            self.card_counters[kSeenKey] = 0;
            self.card_counters[kFirstKey] = 0;
        }
        const int seen_before = self.card_counters[kSeenKey]++;
        if (self.card_counters[kFirstKey] != 0) return;              // the first card is past
        if (ps.cards_played_this_turn - seen_before != 1) return;    // not the first card
        self.card_counters[kFirstKey] = 1;
        if (!self.isAtBattlefield()) return;                          // "if I'm at a battlefield"
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_reduction = 2;
        m.power_reduction = 2;
        m.next_card_only = true;
        m.this_turn_only = false;   // "your next card", whenever it is played
        m.affects_friendly_only = true;
        ps.cost_modifiers.push_back(m);
        ctx.events.logTrace("ASTRAL HERON: first card this turn -> next card costs [2][A][A] less");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 831;
        d.def_id = R"RB(ven-044-166)RB";
        d.name = R"RB(Astral Heron)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-044/166)RB";
        d.collector_number = 44;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bird)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 7;
        d.might = 7;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(When you play your first card each turn, if I'm at a battlefield, your next card costs [2][A][A] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b1ce87efc9a1a7e2438c53e034a7f92caaa67ed6-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_831(CardRegistry& r) {
    r.registerCard(831, std::make_unique<AstralHeron>());
}

}  // namespace riftbound
