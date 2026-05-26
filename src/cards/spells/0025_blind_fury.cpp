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

class BlindFury : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Each opponent reveals the top card of their Main Deck. Choose one and
    //  banish it, then play it, ignoring its cost. Then recycle the rest."
    // 1v1: a single opponent reveals one card; you choose it, banish, then
    //  play it under your control. "the rest" is empty in 1v1.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        auto& opp_deck = ctx.state.player(opp).main_deck;
        if (opp_deck.empty()) {
            ctx.events.logTrace("BLIND FURY: opponent deck empty — no-op");
            return;
        }
        GameObjectId top = opp_deck.back();
        if (!ctx.state.objectExists(top)) return;
        auto& card = ctx.state.getObject(top);
        // Reveal publicly.
        ctx.events.emit(CardRevealedEvent{
            top, card.card_def_id, card.owner,
            /*revealed_to_all=*/true, ctx.controller, ZoneType::MainDeck});
        ctx.events.logTrace("BLIND FURY: opponent reveals " + card.name);
        // Detach from the opponent's deck, banish, then play under my control.
        opp_deck.pop_back();
        bool is_permanent = card.isUnit() || card.isGear();
        ctx.executor.banishObject(top);
        if (is_permanent) {
            // playIgnoringCost only boards permanents; a spell would be placed
            // on the board incorrectly, so spells are left banished
            // (documented limitation of the ignore-cost primitive).
            ctx.executor.playIgnoringCost(ctx.controller, top);
            ctx.events.logTrace("BLIND FURY: banished + played " + card.name +
                                " ignoring cost");
        } else {
            ctx.events.logTrace("BLIND FURY: banished " + card.name +
                                " (non-permanent play unsupported by primitive)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 25;
        d.def_id = R"RB(ogn-025-298)RB";
        d.name = R"RB(Blind Fury)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-025/298)RB";
        d.collector_number = 25;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Each opponent reveals the top card of their Main Deck. Choose one and banish it, then play it, ignoring its cost. Then recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d74ba4998df9348408f1400090a0b0737c675fbd-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_25(CardRegistry& r) {
    r.registerCard(25, std::make_unique<BlindFury>());
}

} // namespace riftbound
