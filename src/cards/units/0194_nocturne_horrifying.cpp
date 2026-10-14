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
    // "As you look at or reveal me from the top of your deck, you may banish
    // me. If you do, you may play me for [A]." [Ganking] engine-handled.
    // A replacement (CR 370.1.c.1 and its Nocturne example): applied as he is
    // looked at or revealed, inside the effect that does it — no chain item,
    // no priority window (testing phase D71). The banish is the "may"; then
    // whether to play him and where (his owner's base or a battlefield they
    // control, CR 355.2.a) is one choice: 0 = stay banished, 1 = base,
    // 2 + i = battlefield i. Playing costs [A] — any one Power — in place of
    // his printed cost. The effect that was looking goes on without him
    // (GameState::left_while_seen).
    bool replacesBeingSeenOnTop() const override { return true; }
    void seenOnTopOfDeck(CardContext& ctx) override {
        const PlayerId owner = ctx.controller;
        if (ctx.state.getObject(ctx.source).zone != ZoneType::MainDeck) return;
        if (!ctx.executor.confirmNow(owner, "Nocturne: banish me from the top of your deck?"))
            return;
        deck::take(ctx.state, ctx.source, deck::Place::Public, knowledge::everyone());
        ctx.executor.banishObject(ctx.source);
        std::vector<std::pair<int, std::string>> modes{{0, "Don't play me"}};
        if (ctx.executor.payableAnyPower(owner) >= 1) {
            modes.push_back({1, "Play me for [A] to base"});
            const auto& bfs = ctx.state.battlefields;
            for (size_t i = 0; i < bfs.size() && i < 30; ++i)
                if (!ctx.state.player(owner).units_play_base_only &&
                    !bfs[i].blocks_unit_play && bfs[i].controller.has_value() &&
                    *bfs[i].controller == owner)
                    modes.push_back({static_cast<int>(i) + 2,
                                     "Play me for [A] to battlefield #" + toString(bfs[i].id)});
        }
        const int mode = ctx.executor.chooseAmongNow(owner, modes, "Nocturne: play me for [A]?");
        if (mode >= 1 && ctx.executor.payAnyPower(owner, 1)) {
            ctx.executor.playIgnoringCost(owner, ctx.source,
                                          effectPlayLocation(ctx.state, owner, mode - 1));
            RIFT_TRACE(ctx.events, "NOCTURNE: banished from deck top and played for [A]");
        } else {
            RIFT_TRACE(ctx.events, "NOCTURNE: banished from deck top");
        }
        ctx.state.left_while_seen.push_back(ctx.source);
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
