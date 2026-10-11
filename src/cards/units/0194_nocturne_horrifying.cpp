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

class NocturneHorrifying : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "As you look at or reveal me from the top of your deck, you may banish
    // me. If you do, you may play me for [A]." [Ganking] engine-handled.
    // Two decisions: the banish is the "may" (confirmOptional); whether to
    // then play me, and where (my base or a battlefield I control, CR
    // 355.2.a), is one pickMode. Playing costs [A] — any one Power — in place
    // of my printed cost, paid with the choices (CR 383.3).
    TriggerType triggerType() const override { return TriggerType::WhenIRevealedFromTop; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId owner = ctx.state.getObject(ctx.source).owner;
        auto still_in_deck = [&] {
            return ctx.state.getObject(ctx.source).zone == ZoneType::MainDeck;
        };
        // Already off the deck top: no return ahead of the "may" —
        // confirmOptional stores the "no" on the chain item, so the resolution
        // asks nothing if I am back in the deck by then.
        int conf = confirmOptional(ctx, "Nocturne: banish me from the top of your deck?",
                                   still_in_deck);
        if (conf < 1) return;     // waiting / declined
        // Mode 0 = stay banished; 1 = play to base; 2 + i = play to battlefield i.
        const auto& bfs = ctx.state.battlefields;
        std::vector<std::string> labels{"Don't play me", "Play me for [A] to base"};
        uint32_t legal = 1u;
        const bool can_pay = costAlreadyPaid(ctx) || ctx.executor.payableAnyPower(owner) >= 1;
        if (can_pay) legal |= 2u;
        for (size_t i = 0; i < bfs.size() && i < 30; ++i) {
            labels.push_back("Play me for [A] to battlefield #" + std::to_string(bfs[i].id));
            if (can_pay && !ctx.state.player(owner).units_play_base_only &&
                !bfs[i].blocks_unit_play && bfs[i].controller.has_value() &&
                *bfs[i].controller == owner)
                legal |= 1u << (i + 2);
        }
        const int mode = pickMode(ctx, "Nocturne: play me for [A]?",
                                  static_cast<int>(labels.size()), labels, legal);
        if (mode == -1) return;   // waiting for the agent
        if (mode >= 1 && costDueNow(ctx) && !ctx.executor.payAnyPower(owner, 1)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (!still_in_deck()) return;         // left the deck in response
        deck::take(ctx.state, ctx.source, deck::Place::Public, knowledge::everyone());
        ctx.executor.banishObject(ctx.source);
        if (mode < 1) {
            ctx.events.logTrace("NOCTURNE: banished from deck top");
            return;
        }
        // Mode 1 is the base; mode 2 + i is battlefield i (the base if it is
        // no longer mine to play to).
        ctx.executor.playIgnoringCost(owner, ctx.source,
                                      effectPlayLocation(ctx.state, owner, mode - 1));
        ctx.events.logTrace("NOCTURNE: banished from deck top and played for [A]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 194;
        d.def_id = R"RB(ogn-194-298)RB";
        d.name = R"RB(Nocturne, Horrifying)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-194/298)RB";
        d.collector_number = 194;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Nocturne)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
As you look at or reveal me from the top of your deck, you may banish me. If you do, you may play me for [A].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/19a751364bc6eb5297596e8733d0d30a1111ac78-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_194(CardRegistry& r) {
    r.registerCard(194, std::make_unique<NocturneHorrifying>());
}

} // namespace riftbound
